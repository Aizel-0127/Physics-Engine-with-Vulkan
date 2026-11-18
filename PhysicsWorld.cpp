#include "PhysicsWorld.h"
#include "Physics/CircleShape.h"
#include "Physics/PolygonShape.h"
#include "Physics/ChainShape.h"
#include "Physics/EdgeShape.h"


void PhysicsWorld::clearBodies()
{
    for (int i = bodies.size()-1; i >= 0; i--) {
        if (bodies[i].shape && bodies[i].invMass != 0) {
            delete bodies[i].shape;
            bodies[i].shape = nullptr;
            bodies.erase(bodies.begin() + i);
        }
    }
}

void PhysicsWorld::update(float realDeltaTime) {
    // Защита от огромных dt (например, если вкладка была свернута)
    if (realDeltaTime > MAX_FRAME_TIME) realDeltaTime = MAX_FRAME_TIME;

    accumulator += realDeltaTime;

    // Выполняем фиксированные шаги
    while (accumulator >= FIXED_TIMESTEP) {
        step(FIXED_TIMESTEP);
        accumulator -= FIXED_TIMESTEP;
    }
}

PhysicsWorld::~PhysicsWorld() {
    for (auto& body : bodies) {
        if (body.shape) {
            delete body.shape;
            body.shape = nullptr;
        }
    }
}

void PhysicsWorld::step(float dt) {
    // 1. Сбрасываем аккумулятор сил
    for (auto& body : bodies) {
        if (body.type != BodyType::Dynamic || body.isSleeping) continue;
        body.forceAccumulator.SetZero();
    }

    // 2. Собираем силы (гравитация)
    applyGravity();

    // 3. Интегрируем (Verlet + Sleeping)
    integrate(dt);

    // 2. Поиск столкновений
    std::vector<ContactConstraint> contacts;
    findCollisions(contacts);

    // 3. Перенос импульсов с прошлого кадра (Warm Starting)
    matchAndTransferImpulses(contacts);

    // 4. Применение старых импульсов к скоростям
    for (auto& cc : contacts) {
        warmStartContact(cc);
    }

    // 4. Решатель скоростей (Velocity Solver)
    // 8 итераций достаточно для стабильной стопки из 100 объектов
    for (int iter = 0; iter < 8; ++iter) {
        for (auto& cc : contacts) {
            solveContactVelocity(cc);
        }
    }

    // 5. Итерации решателя
    for (int iter = 0; iter < 8; ++iter) {
        for (auto& cc : contacts) solveContactVelocity(cc);
    }

    for (int iter = 0; iter < 3; ++iter) {
        bool resolved = true;
        for (auto& cc : contacts) {
            if (!solveContactPosition(cc)) resolved = false;
        }
        if (resolved) break;
    }

    // 6. Сохраняем контакты для следующего кадра
    m_persistentContacts = contacts;
    // 7. Логика сна
    for (auto& body : bodies) {
        if (body.type != BodyType::Dynamic || body.isSleeping) continue;

        // Вычисляем квадрат скорости (для производительности)
        float linVelSq = phys::Dot(body.velocity, body.velocity);
        float angVelSq = body.angularVelocity * body.angularVelocity;

        // Если объект движется медленнее порога
        if (linVelSq < phys::linearSleepTolerance * phys::linearSleepTolerance &&
            angVelSq < phys::angularSleepTolerance * phys::angularSleepTolerance)
        {
            body.sleepTimer += dt;

            // Если он лежит неподвижно достаточно долго (например, 0.5 сек) — усыпляем
            if (body.sleepTimer > phys::timeToSleep) {
                body.isSleeping = true;
                body.velocity.SetZero();        // Обнуляем скорости
                body.angularVelocity = 0.0f;
            }
        }
        else {
            // Если объект двигается, сбрасываем таймер
            body.sleepTimer = 0.0f;
        }
    }
}

// Вспомогательная функция для получения конкретного ребра из ChainShape
static void GetEdgeFromShape(const phys::Shape* shape, int childIndex,
    phys::EdgeShape& edgeOut,
    const phys::Transform& xf,
    phys::Transform& edgeTransform)
{
    if (shape->GetType() == phys::Shape::e_chain) {
        const phys::ChainShape* chain = static_cast<const phys::ChainShape*>(shape);
        chain->GetChildEdge(&edgeOut, childIndex);
        // Трансформация для цепочки — это трансформация самого тела
        edgeTransform = xf;
    }
    else {
        const phys::EdgeShape* edge = static_cast<const phys::EdgeShape*>(shape);
        edgeOut = *edge;
        edgeTransform = xf;
    }
}

void PhysicsWorld::applyGravity() {
    // --- 1. Нисходящая гравитация ---
    if (gravitySettings.enableDownward) {
        for (auto& body : bodies) {
            if (body.type != BodyType::Dynamic || body.isSleeping) continue;

            // F = m * g (сила = масса * ускорение свободного падения)
            phys::Vec2 gravityForce = gravitySettings.downwardDirection *
                (gravitySettings.downwardStrength * body.mass);

            body.forceAccumulator += gravityForce;
        }
    }

    // --- 2. Взаимная гравитация ---
    if (gravitySettings.enableMutual) {
        for (size_t i = 0; i < bodies.size(); ++i) {
            // Статические и спящие тела не притягиваются
            if (bodies[i].type != BodyType::Dynamic || bodies[i].isSleeping) continue;

            for (size_t j = i + 1; j < bodies.size(); ++j) {
                if (bodies[j].type == BodyType::Kinematic || bodies[j].isSleeping) continue;

                // Вектор от i к j
                phys::Vec2 delta = bodies[j].transform.p - bodies[i].transform.p;
                float distSq = phys::Dot(delta, delta);
                float dist = std::sqrt(distSq);

                if (dist > 0.0001f) {
                    // Направление силы (нормализованный вектор)
                    phys::Vec2 dir = (1.0f / dist) * delta;

                    // Смягчение (чтобы избежать бесконечной силы при dist -> 0)
                    float softDistSq = distSq + (gravitySettings.mutualSoftening * gravitySettings.mutualSoftening);

                    // F = G * m1 * m2 / r^2
                    float forceMagnitude = gravitySettings.gravitationalConstant *
                        bodies[i].mass * bodies[j].mass / softDistSq;

                    phys::Vec2 force = forceMagnitude * dir;

                    // Применяем силу к обоим телам (в противоположных направлениях)
                    bodies[i].forceAccumulator += force;
                    bodies[j].forceAccumulator -= force;
                }
            }
        }
    }
}

void PhysicsWorld::integrate(float dt) {
    for (auto& body : bodies) {
        if (body.type != BodyType::Dynamic || body.isSleeping) continue;

        // 1. Вычисляем линейное ускорение
        phys::Vec2 linearAcceleration = body.invMass * body.forceAccumulator;

        // 2. Вычисляем угловое ускорение (alpha = torque / I)
        float angularAcceleration = body.invInertia * body.torque;

        // 3. Velocity Verlet для линейной скорости
        body.velocity += (0.5f * dt) * linearAcceleration;
        body.transform.p += dt * body.velocity;
        body.velocity += (0.5f * dt) * linearAcceleration;

        // 4. Velocity Verlet для угловой скорости
        body.angularVelocity += 0.5f * dt * angularAcceleration;
        body.transform.q.Set(body.transform.q.GetAngle() + body.angularVelocity * dt);
        body.angularVelocity += 0.5f * dt * angularAcceleration;

        // 5. Применяем угловое демпфирование (как в Box2D)
        // Ограничиваем множитель, чтобы он не стал отрицательным
        float dampingFactor = std::max(0.0f, 1.0f - dt * body.angularDamping);
        body.angularVelocity *= dampingFactor;

        // 6. Логика сна
        if (phys::Dot(body.velocity, body.velocity) < 25.0f) {
            body.sleepTimer += dt;
            if (body.sleepTimer > 0.5f) {
                body.isSleeping = true;
                body.velocity.SetZero();
                body.angularVelocity = 0.0f;
            }
        }
        else {
            body.sleepTimer = 0.0f;
        }
    }
}

void PhysicsWorld::warmStartContact(ContactConstraint& cc) {
    Rigidbody2D* a = cc.bodyA;
    Rigidbody2D* b = cc.bodyB;

    for (int i = 0; i < cc.manifold.pointCount; ++i) {
        auto& cp = cc.points[i];
        phys::Vec2 P = cp.normalImpulse * cc.normal + cp.tangentImpulse * phys::Cross(cc.normal, 1.0f);

        // Применяем накопленные импульсы к телам
        a->velocity -= a->invMass * P;
        a->angularVelocity -= a->invInertia * phys::Cross(cp.rA, P);

        b->velocity += b->invMass * P;
        b->angularVelocity += b->invInertia * phys::Cross(cp.rB, P);
    }
}

void PhysicsWorld::solveContactVelocity(ContactConstraint& cc) {
    Rigidbody2D* a = cc.bodyA;
    Rigidbody2D* b = cc.bodyB;
    phys::Vec2 tangent = phys::Cross(cc.normal, 1.0f);

    for (int i = 0; i < cc.manifold.pointCount; ++i) {
        auto& cp = cc.points[i];

        // 1. Решаем нормальный импульс (отталкивание)
        phys::Vec2 dv = (b->velocity + phys::Cross(b->angularVelocity, cp.rB)) -
            (a->velocity + phys::Cross(a->angularVelocity, cp.rA));
        float vn = phys::Dot(dv, cc.normal);

        float dPn = cp.normalMass * (-vn + cp.velocityBias);

        // Ограничиваем импульс (он может только толкать, но не притягивать)
        float Pn0 = cp.normalImpulse;
        cp.normalImpulse = std::max(Pn0 + dPn, 0.0f);
        dPn = cp.normalImpulse - Pn0;

        phys::Vec2 Pn = dPn * cc.normal;
        a->velocity -= a->invMass * Pn;
        a->angularVelocity -= a->invInertia * phys::Cross(cp.rA, Pn);
        b->velocity += b->invMass * Pn;
        b->angularVelocity += b->invInertia * phys::Cross(cp.rB, Pn);

        // 2. Решаем тангенциальный импульс (трение)
        dv = (b->velocity + phys::Cross(b->angularVelocity, cp.rB)) -
            (a->velocity + phys::Cross(a->angularVelocity, cp.rA));
        float vt = phys::Dot(dv, tangent);

        float dPt = cp.tangentMass * (-vt);

        // Ограничиваем трение законом Кулона: |Pt| <= friction * Pn
        float maxPt = cc.friction * cp.normalImpulse;
        float Pt0 = cp.tangentImpulse;
        cp.tangentImpulse = phys::Clamp(Pt0 + dPt, -maxPt, maxPt);
        dPt = cp.tangentImpulse - Pt0;

        phys::Vec2 Pt = dPt * tangent;
        a->velocity -= a->invMass * Pt;
        a->angularVelocity -= a->invInertia * phys::Cross(cp.rA, Pt);
        b->velocity += b->invMass * Pt;
        b->angularVelocity += b->invInertia * phys::Cross(cp.rB, Pt);
    }
}

void PhysicsWorld::prepareContact(ContactConstraint& cc) {
    Rigidbody2D* a = cc.bodyA;
    Rigidbody2D* b = cc.bodyB;

    // 1. Получаем радиусы фигур (для кругов - реальный, для полигонов - skin radius)
    float radiusA = a->shape->m_radius;
    float radiusB = b->shape->m_radius;

    // 2. Инициализируем WorldManifold
    phys::WorldManifold wm;
    wm.Initialize(&cc.manifold, a->transform, radiusA, b->transform, radiusB);

    // 3. Забираем мировую нормаль
    cc.normal = wm.normal;

    // Смешиваем материалы
    cc.friction = std::sqrt(a->friction * b->friction);
    cc.restitution = std::max(a->restitution, b->restitution);

    // 4. Настраиваем точки контакта
    for (int i = 0; i < cc.manifold.pointCount; ++i) {
        auto& cp = cc.points[i];

        // Берем мировую точку контакта и глубину проникновения из WorldManifold
        phys::Vec2 contactPoint = wm.points[i];
        cp.separation = wm.separations[i]; // <-- ВОТ ОНА, ГЛУБИНА ПРОНИКНОВЕНИЯ!

        // Векторы от центров масс до точки контакта
        cp.rA = contactPoint - a->transform.p;
        cp.rB = contactPoint - b->transform.p;

        // Вычисляем эффективную массу вдоль нормали
        float rnA = phys::Cross(cp.rA, cc.normal);
        float rnB = phys::Cross(cp.rB, cc.normal);

        float kNormal = a->invMass + b->invMass +
            a->invInertia * rnA * rnA +
            b->invInertia * rnB * rnB;
        cp.normalMass = (kNormal > 0.0f) ? 1.0f / kNormal : 0.0f;

        // Вычисляем эффективную массу вдоль касательной
        phys::Vec2 tangent = phys::Cross(cc.normal, 1.0f);
        float rtA = phys::Cross(cp.rA, tangent);
        float rtB = phys::Cross(cp.rB, tangent);

        float kTangent = a->invMass + b->invMass +
            a->invInertia * rtA * rtA +
            b->invInertia * rtB * rtB;
        cp.tangentMass = (kTangent > 0.0f) ? 1.0f / kTangent : 0.0f;

        // Velocity Bias для упругости (используем относительную скорость)
        phys::Vec2 vA = a->velocity + phys::Cross(a->angularVelocity, cp.rA);
        phys::Vec2 vB = b->velocity + phys::Cross(b->angularVelocity, cp.rB);
        float vRel = phys::Dot(cc.normal, vB - vA);

        cp.velocityBias = 0.0f;
        if (vRel < -phys::velocityThreshold) {
            cp.velocityBias = -cc.restitution * vRel;
        }
    }
}

bool PhysicsWorld::solveContactPosition(ContactConstraint& cc) {
    Rigidbody2D* a = cc.bodyA;
    Rigidbody2D* b = cc.bodyB;
    float minSeparation = 0.0f;

    for (int i = 0; i < cc.manifold.pointCount; ++i) {
        auto& cp = cc.points[i];

        // Теперь используем сохраненную глубину
        float separation = cp.separation;

        // Выталкиваем тела (Baumgarte)
        // linearSlop - это допуск, чтобы не тратить CPU на микро-проникновения
        float C = phys::Clamp(phys::baumgarte * (separation + phys::linearSlop), -phys::maxLinearCorrection, 0.0f);

        float rnA = phys::Cross(cp.rA, cc.normal);
        float rnB = phys::Cross(cp.rB, cc.normal);
        float K = a->invMass + b->invMass + a->invInertia * rnA * rnA + b->invInertia * rnB * rnB;

        float impulse = (K > 0.0f) ? -C / K : 0.0f;

        phys::Vec2 P = impulse * cc.normal;

        // Двигаем позицию и ПОВОРАЧИВАЕМ тело (так как точка контакта может быть не в центре)
        a->transform.p -= a->invMass * P;
        a->transform.q.Set(a->transform.q.GetAngle() - a->invInertia * phys::Cross(cp.rA, P));

        b->transform.p += b->invMass * P;
        b->transform.q.Set(b->transform.q.GetAngle() + b->invInertia * phys::Cross(cp.rB, P));

        minSeparation = std::min(minSeparation, separation);
    }

    // Возвращаем true, если проникновение в пределах нормы (linearSlop * 3)
    return minSeparation >= -3.0f * phys::linearSlop;
}

size_t PhysicsWorld::addBody(const Rigidbody2D& body) {
    bodies.push_back(body);
    return bodies.size() - 1;
}


void PhysicsWorld::findCollisions(std::vector<ContactConstraint>& contacts) {
    contacts.clear();

    for (size_t i = 0; i < bodies.size(); ++i) {
        for (size_t j = i + 1; j < bodies.size(); ++j) {
            Rigidbody2D* a = &bodies[i];
            Rigidbody2D* b = &bodies[j];

            if (a->isSleeping && b->isSleeping) continue;
            if (a->type != BodyType::Dynamic && b->type != BodyType::Dynamic) continue;
            if (!a->shape || !b->shape) continue;

            phys::Manifold manifold;
            phys::Shape::Type typeA = a->shape->GetType();
            phys::Shape::Type typeB = b->shape->GetType();

            // Пропускаем Edge vs Edge (линии не сталкиваются с линиями)
            bool isEdgeA = (typeA == phys::Shape::e_edge || typeA == phys::Shape::e_chain);
            bool isEdgeB = (typeB == phys::Shape::e_edge || typeB == phys::Shape::e_chain);
            if (isEdgeA && isEdgeB) continue;

            // --- МАРШРУТИЗАЦИЯ ---
            if (typeA == phys::Shape::e_circle && typeB == phys::Shape::e_circle) {
                phys::CollideCircles(&manifold,
                    static_cast<const phys::CircleShape*>(a->shape), a->transform,
                    static_cast<const phys::CircleShape*>(b->shape), b->transform);
            }
            else if (typeA == phys::Shape::e_polygon && typeB == phys::Shape::e_polygon) {
                phys::CollidePolygons(&manifold,
                    static_cast<const phys::PolygonShape*>(a->shape), a->transform,
                    static_cast<const phys::PolygonShape*>(b->shape), b->transform);
            }
            else if (typeA == phys::Shape::e_polygon && typeB == phys::Shape::e_circle) {
                phys::CollidePolygonAndCircle(&manifold,
                    static_cast<const phys::PolygonShape*>(a->shape), a->transform,
                    static_cast<const phys::CircleShape*>(b->shape), b->transform);
            }
            else if (typeA == phys::Shape::e_circle && typeB == phys::Shape::e_polygon) {
                phys::CollidePolygonAndCircle(&manifold,
                    static_cast<const phys::PolygonShape*>(b->shape), b->transform,
                    static_cast<const phys::CircleShape*>(a->shape), a->transform);
            }
            else if (isEdgeA) { // Edge/Chain vs Polygon/Circle
                phys::EdgeShape edge; phys::Transform edgeXf;
                // Берем 0-й child (для простых EdgeShape это единственный)
                GetEdgeFromShape(a->shape, 0, edge, a->transform, edgeXf);

                if (typeB == phys::Shape::e_circle) {
                    phys::CollideEdgeAndCircle(&manifold, &edge, edgeXf,
                        static_cast<const phys::CircleShape*>(b->shape), b->transform);
                }
                else {
                    phys::CollideEdgeAndPolygon(&manifold, &edge, edgeXf,
                        static_cast<const phys::PolygonShape*>(b->shape), b->transform);
                }
            }
            else if (isEdgeB) { // Polygon/Circle vs Edge/Chain
                phys::EdgeShape edge; phys::Transform edgeXf;
                GetEdgeFromShape(b->shape, 0, edge, b->transform, edgeXf);

                if (typeA == phys::Shape::e_circle) {
                    phys::CollideEdgeAndCircle(&manifold, &edge, edgeXf,
                        static_cast<const phys::CircleShape*>(a->shape), a->transform);
                }
                else {
                    phys::CollideEdgeAndPolygon(&manifold, &edge, edgeXf,
                        static_cast<const phys::PolygonShape*>(a->shape), a->transform);
                }
                // Если B — Edge, то нормаль в манифолде будет смотреть от Edge к A.
                // Нам нужно инвертировать её, чтобы она смотрела от A к B (как принято в Box2D)
                manifold.localNormal = -manifold.localNormal;
            }

            if (manifold.pointCount > 0) {
                // Будим тела, если они спали
                if (a->isSleeping) { a->isSleeping = false; a->sleepTimer = 0.0f; }
                if (b->isSleeping) { b->isSleeping = false; b->sleepTimer = 0.0f; }

                ContactConstraint cc{};
                // Обратите внимание: для circle-polygon мы меняли местами A и B, 
                // но Box2D возвращает нормаль от Poly к Circle. Нам нужно, 
                // чтобы нормаль всегда смотрела от A к B.
                // (Это можно опустить для краткости, но лучше сделать правильно)
                if (typeA == phys::Shape::e_circle && typeB == phys::Shape::e_polygon)
                {
                    cc.bodyA = b; // <--- ПОЛИГОН
                    cc.bodyB = a; // <--- КРУГ
                }
                else
                {
                    cc.bodyA = a;
                    cc.bodyB = b;
                }                
                cc.manifold = manifold;
                prepareContact(cc);
                contacts.push_back(cc);
            }
        }
    }
}

size_t PhysicsWorld::addStaticBox(glm::vec2 center, glm::vec2 halfExtents) {
    Rigidbody2D body;
    body.transform.p = phys::Vec2(center.x, center.y);
    body.transform.q.SetIdentity();

    body.type = BodyType::Static;
    body.mass = 0.0f;
    body.invMass = 0.0f;
    body.inertia = 0.0f;
    body.invInertia = 0.0f;

    phys::PolygonShape* shape = new phys::PolygonShape();
    shape->SetAsBox(halfExtents.x, halfExtents.y);
    body.shape = shape;

    bodies.push_back(body);

    // Возвращаем индекс только что добавленного тела
    return bodies.size() - 1;
}

void PhysicsWorld::matchAndTransferImpulses(std::vector<ContactConstraint>& newContacts) {
    for (auto& newCc : newContacts) {
        // Ищем такой же контакт в прошлом кадре
        for (auto& oldCc : m_persistentContacts) {
            // Проверяем, что это те же самые два тела
            if (oldCc.bodyA == newCc.bodyA && oldCc.bodyB == newCc.bodyB) {
                // Проходим по точкам контакта
                for (int i = 0; i < newCc.manifold.pointCount; ++i) {
                    phys::ContactID newId = newCc.manifold.points[i].id;
                    for (int j = 0; j < oldCc.manifold.pointCount; ++j) {
                        phys::ContactID oldId = oldCc.manifold.points[j].id;

                        // Если ID совпадают, значит это та же самая точка
                        if (newId.key == oldId.key) {
                            newCc.points[i].normalImpulse = oldCc.points[j].normalImpulse;
                            newCc.points[i].tangentImpulse = oldCc.points[j].tangentImpulse;
                            break;
                        }
                    }
                }
                break; // Нашли пару, переходим к следующему контакту
            }
        }
    }
}