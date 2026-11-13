#include "PhysicsWorld.h"

void PhysicsWorld::clearBodies()
{
    bodies.clear();
}

void PhysicsWorld::update(float realDeltaTime, float currentWidth, float currentHeight) {
    // Защита от огромных dt (например, если вкладка была свернута)
    if (realDeltaTime > MAX_FRAME_TIME) realDeltaTime = MAX_FRAME_TIME;

    accumulator += realDeltaTime;

    // Выполняем фиксированные шаги
    while (accumulator >= FIXED_TIMESTEP) {
        step(FIXED_TIMESTEP, currentWidth, currentHeight);
        accumulator -= FIXED_TIMESTEP;
    }
}

void PhysicsWorld::step(float dt, float currentWidth, float currentHeight) {
    // 1. Сбрасываем накопленные силы
    for (auto& body : bodies) {
        body.forceAccumulator = glm::vec2(0.0f);
    }

    // 2. Применяем гравитацию (настраиваемую)
    applyGravity();

    // 3. Интегрируем скорости и позиции
    integrate(dt);
    // 4. Коллизии с границами экрана
    resolveBoundaryCollisions(currentWidth, currentHeight);
    // 5. Коллизии между телами
    resolveBodyCollisions();
}

void PhysicsWorld::applyGravity() {
    // --- 1. Нисходящая гравитация ---
    if (gravitySettings.enableDownward) {
        for (auto& body : bodies) {
            if (body.type != BodyType::Dynamic) continue;
            glm::vec2 gravityForce = gravitySettings.downwardDirection *
                gravitySettings.downwardStrength *
                body.mass;
            body.forceAccumulator += gravityForce;
        }
    }

    // --- 2. Взаимная гравитация ---
    if (gravitySettings.enableMutual) {
        for (size_t i = 0; i < bodies.size(); ++i) {
            // Статические тела не притягиваются, но могут притягивать (удобно для планет)
            if (bodies[i].type != BodyType::Dynamic) continue;

            for (size_t j = i + 1; j < bodies.size(); ++j) {
                if (bodies[j].type == BodyType::Kinematic) continue; // Не притягивает и не притягивается

                glm::vec2 delta = bodies[j].position - bodies[i].position;
                float distSq = glm::dot(delta, delta);
                float dist = std::sqrt(distSq);
                if (dist > 0.0001f) { // Защита от деления на 0
                    glm::vec2 dir = delta / dist;
                    // Смягчение: добавляем квадрат константы к квадрату расстояния
                    float softDistSq = distSq + (gravitySettings.mutualSoftening * gravitySettings.mutualSoftening);

                    float forceMagnitude = gravitySettings.gravitationalConstant *
                        bodies[i].mass * bodies[j].mass / softDistSq;
                    glm::vec2 force = dir * forceMagnitude;

                    // Тело i притягивается к j, тело j — к i
                    if (bodies[i].type == BodyType::Dynamic)
                        bodies[i].forceAccumulator += force;
                    if (bodies[j].type == BodyType::Dynamic)
                        bodies[j].forceAccumulator -= force;
                }
            }
        }
    }
}

void PhysicsWorld::integrate(float dt) {
    for (auto& body : bodies) {
        if (body.type != BodyType::Dynamic) continue;

        // a = F / m
        glm::vec2 acceleration = body.forceAccumulator * body.inverseMass;

        // v = v + a * dt
        body.velocity += acceleration * dt / 2.0f;
        body.position += body.velocity * dt;
        body.velocity += acceleration * dt / 2.0f;
    }
}

size_t PhysicsWorld::addBody(const Rigidbody2D& body) {
    bodies.push_back(body);
    return bodies.size() - 1;
}

void PhysicsWorld::resolveBoundaryCollisions(float width, float height) {
    for (auto& body : bodies) {
        if (body.type != BodyType::Dynamic) continue;

        float radius = 0.0f;
        if (body.collider.type == ColliderType::Circle) {
            radius = body.collider.circle.radius;
        }
        else {
            continue; // Пока границы поддерживают только круги
        }

        // Левая стена
        if (body.position.x - radius < 0.0f) {
            body.velocity.x = abs(body.velocity.x) * body.restitution; // Отскок
            body.position.x = radius; // Выталкиваем
        }
        // Правая стена
        if (body.position.x + radius > width) {
            body.velocity.x = -abs(body.velocity.x) * body.restitution;
            body.position.x = width - radius;
        }
        // Верхняя стена
        if (body.position.y - radius < 0.0f) {
            body.velocity.y = body.restitution * sqrt(abs(body.velocity.y * body.velocity.y - 2 * gravitySettings.downwardStrength * (radius - body.position.y)));
            body.position.y = radius;
        }
        // Нижняя стена
        if (body.position.y + radius > height) {    //v1^2 = v0^2 + 2gh
            body.velocity.y = - body.restitution * sqrt(abs(body.velocity.y * body.velocity.y - 2 * gravitySettings.downwardStrength * (body.position.y + radius - height)));
            body.position.y = height - radius;
        }
    }
}

bool PhysicsWorld::checkCollision(const Rigidbody2D& a, const Rigidbody2D& b, glm::vec2& normal, float& penetration) {
    // Пока мы поддерживаем только Circle vs Circle
    if (a.collider.type == ColliderType::Circle && b.collider.type == ColliderType::Circle) {
        float rA = a.collider.circle.radius;
        float rB = b.collider.circle.radius;

        glm::vec2 delta = b.position - a.position;
        float distSq = glm::dot(delta, delta);
        
        float radiusSum = rA + rB;

        if (distSq >= radiusSum * radiusSum) return false;

        float dist = std::sqrt(distSq);
        if (dist < 0.0001f) {
            normal = { 1.0f, 0.0f };
            penetration = radiusSum;
        }
        else {
            normal = delta / dist;
            penetration = radiusSum - dist;
        }
        return true;
    }

    // В будущем здесь добавятся проверки Box vs Circle, Box vs Box и т.д.
    return false;
}

void PhysicsWorld::resolveCollision(Rigidbody2D& a, Rigidbody2D& b, const glm::vec2& normal, float penetration) {
    // 1. Позиционная коррекция (убираем проникновение)
    float totalInvMass = a.inverseMass + b.inverseMass;
    if (totalInvMass > 0.0f) {
        glm::vec2 correction = normal * (penetration / totalInvMass);
        a.position -= correction * a.inverseMass;
        b.position += correction * b.inverseMass;
    }

    // 2. Относительная скорость вдоль нормали
    glm::vec2 relativeVel = b.velocity - a.velocity;
    float velAlongNormal = glm::dot(relativeVel, normal);

    // Если скорости направлены в разные стороны — они уже разлетаются, не трогаем
    if (velAlongNormal > 0.0f) return;

    // 3. Вычисляем импульс
    float e = std::min(a.restitution, b.restitution); // Берем меньшую упругость
    float j = -(1.0f + e) * velAlongNormal / totalInvMass;

    glm::vec2 impulse = normal * j;
    a.velocity -= impulse * a.inverseMass;
    b.velocity += impulse * b.inverseMass;
}

void PhysicsWorld::resolveBodyCollisions() {
    for (size_t i = 0; i < bodies.size(); ++i) {
        for (size_t j = i + 1; j < bodies.size(); ++j) {
            if (bodies[i].type != BodyType::Dynamic && bodies[j].type != BodyType::Dynamic) continue;
            if (!bodies[i].isActive || !bodies[j].isActive) continue;

            glm::vec2 normal;
            float penetration;
            if (checkCollision(bodies[i], bodies[j], normal, penetration)) {
                resolveCollision(bodies[i], bodies[j], normal, penetration);
            }
        }
    }
}