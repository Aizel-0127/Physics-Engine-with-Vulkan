#include "Scene.h"
#include <cmath> // Для std::sin
#include <random>
#include "PhysicsWorld.h"
#include "Physics/PolygonShape.h"
#include "Physics/CircleShape.h"

void Scene::UpdatePolygonVertices(const PhysicsWorld& physicsWorld) {
    polygonVertices.clear();
    const auto& bodies = physicsWorld.getBodies();
    for (const auto& poly : polygons) {
        std::array<phys::Vec2, 8> worldPoints;

        // Получаем мировые координаты точек
        if (poly.physicsIndex != UINT32_MAX && poly.physicsIndex < bodies.size()) {
            const auto& body = bodies[poly.physicsIndex];
            for (int i = 0; i < poly.vertexCount; ++i) {
                phys::Vec2 localPt(poly.points[i].x, poly.points[i].y);
                phys::Vec2 rotated = phys::Mul(body.transform.q, localPt);
                worldPoints[i] = rotated + body.transform.p;
            }
        }
        else {
            for (int i = 0; i < poly.vertexCount; ++i) {
                worldPoints[i] = phys::Vec2(poly.points[i].x, poly.points[i].y);
            }
        }

        // ВЕЕРНАЯ ТРИАНГУЛЯЦИЯ (Triangle Fan)
        // Для N вершин создаем N-2 треугольников
        for (int i = 1; i < poly.vertexCount - 1; ++i) {
            // Треугольник: (0, i, i+1)
            polygonVertices.push_back({ {worldPoints[0].x, worldPoints[0].y}, poly.color });
            polygonVertices.push_back({ {worldPoints[i].x, worldPoints[i].y}, poly.color });
            polygonVertices.push_back({ {worldPoints[i + 1].x, worldPoints[i + 1].y}, poly.color });
        }
    }
}

size_t Scene::AddCurve(const std::vector<glm::vec2>& points, glm::vec4 color, float thickness) {
    Curve curve;
    curve.color = color;
    curve.thickness = thickness;

    // Резервируем память и переносим точки
    curve.points.reserve(points.size());
    for (const auto& p : points) {
        curve.points.push_back({ p });
    }

    lines.push_back(curve);
    return lines.size() - 1; // Возвращаем индекс созданной линии
}

void Scene::AddSineWave(float startX, float endX, float step, float amplitude, float offsetY, glm::vec4 color, float thickness) {
    std::vector<glm::vec2> points;

    // Вычисляем количество точек, чтобы сразу зарезервировать память
    size_t count = static_cast<size_t>((endX - startX) / step) + 1;
    points.reserve(count);

    // Генерируем точки по формуле y = A * sin(x) + offsetY
    for (float x = startX; x <= endX; x += step) {
        // Умножаем x на 0.05f, чтобы волны не были слишком частыми
        float y = offsetY + std::sin(x * 0.05f) * amplitude;
        points.push_back({ x, y });
    }

    AddCurve(points, color, thickness);
}

void Scene::addPhysicalCircle(PhysicsWorld& physics, glm::vec2 pos, float radius, glm::vec4 color, float mass) {
    // 1. Создаем физическое тело
    Rigidbody2D body;
    body.type = BodyType::Dynamic;
    body.transform.p = phys::Vec2(pos.x, pos.y);
    body.transform.q.SetIdentity(); // Начальный угол поворота — 0

    // 2. Создаем форму (круг)
    phys::CircleShape* shape = new phys::CircleShape();
    shape->m_radius = radius;
    shape->m_p.SetZero(); // Центр масс совпадает с центром тела
    body.shape = shape;

    // 3. Вычисляем массу и момент инерции
    // В Box2D ComputeMass принимает плотность (density), а не массу.
    // Поэтому мы сначала вычисляем площадь, а затем находим плотность: density = mass / area.
    float area = phys::pi * radius * radius;
    float density = (area > 0.0f) ? (mass / area) : 0.0f;

    phys::MassData massData;
    shape->ComputeMass(&massData, density);

    body.mass = massData.mass;
    body.invMass = (body.mass > 0.0f) ? 1.0f / body.mass : 0.0f;

    body.inertia = massData.I;
    body.invInertia = (body.inertia > 0.0f) ? 1.0f / body.inertia : 0.0f;

    // 4. Задаем физические свойства
    body.friction = 0.3f;
    body.restitution = 0.9f;      // Высокая упругость для мячей
    body.angularDamping = 0.1f;   // Затухание вращения
    body.isSleeping = false;
    body.sleepTimer = 0.0f;

    // 5. Добавляем тело в мир
    size_t bodyIdx = physics.addBody(body);

    // 6. Создаем визуальный объект
    CircleData c{};
    c.center = pos;
    c.radius = radius;
    c.color = color;
    c.physicsIndex = static_cast<uint32_t>(bodyIdx); // Связываем визуал с физикой

    circles.push_back(c);
}

void Scene::addStaticQuad(PhysicsWorld& physics, glm::vec2 center, glm::vec2 halfExtents, glm::vec4 color)
{
    // 1. Создаем физическое тело и получаем его индекс
    size_t bodyIdx = physics.addStaticBox(center, halfExtents);

    // 2. Создаем визуальный квад (ЛОКАЛЬНЫЕ координаты)
    PolygonVk q;
    q.points[0] = { -halfExtents.x, -halfExtents.y }; // TopLeft
    q.points[1] = { halfExtents.x, -halfExtents.y }; // TopRight
    q.points[2] = { halfExtents.x,  halfExtents.y }; // BottomRight
    q.points[3] = { -halfExtents.x,  halfExtents.y }; // BottomLeft

    q.color = color;
    // Привязываем к физическому телу! Теперь UpdateQuadVertices сам применит трансформацию.
    q.physicsIndex = static_cast<uint32_t>(bodyIdx);

    polygons.push_back(q);
}

void Scene::addRandomCircle(PhysicsWorld& physics) {
    // Инициализируем генератор один раз (static), чтобы не тратить ресурсы каждый вызов
    static std::random_device rd;
    static std::mt19937 gen(rd());

    // 1. Случайный радиус от 10 до 40 пикселей
    std::uniform_real_distribution<float> radiusDist(10.0f, 40.0f);
    float radius = radiusDist(gen);

    // 2. Случайная позиция (X и Y)
    // Важно: вычитаем радиус из краев, чтобы шар не появлялся наполовину за экраном
    std::uniform_real_distribution<float> posXDist(radius, WIDTH - radius);
    std::uniform_real_distribution<float> posYDist(radius, HEIGHT - radius);
    glm::vec2 pos = { posXDist(gen), posYDist(gen) };

    // 3. Случайный цвет (RGB от 0.0 до 1.0, альфа = 1.0)
    std::uniform_real_distribution<float> colorDist(0.0f, 1.0f);
    glm::vec4 color = {
        colorDist(gen),
        colorDist(gen),
        colorDist(gen),
        1.0f
    };
    // 4. Случайная масса от 1.0 до 10.0 (чтобы шары по-разному отталкивались)
    std::uniform_real_distribution<float> massDist(1.0f, 10.0f);
    float mass = massDist(gen);

    // 5. Вызываем вашу существующую функцию
    addPhysicalCircle(physics, pos, radius, color, mass);
}

void Scene::addPhysicalPolygon(PhysicsWorld& physics, glm::vec2 pos,
    const std::vector<glm::vec2>& localVerts,
    glm::vec4 color, float density) {

    if (localVerts.size() < 3 || localVerts.size() > maxPolygonVerticesVulkan) {
        throw std::runtime_error("Polygon must have between 3 and {maxPolygonVerticesVulkan} vertices");
    }
    // 1. Создаем физическое тело
    Rigidbody2D body;
    body.type = BodyType::Dynamic;
    body.transform.p = phys::Vec2(pos.x, pos.y);
    body.transform.q.SetIdentity(); // Угол поворота теперь определяется самими вершинами

    // 2. Создаем форму (произвольный выпуклый четырёхугольник)
    phys::PolygonShape* shape = new phys::PolygonShape();

    // Конвертируем glm::vec2 в phys::Vec2

    phys::Vec2 physVerts[8];
    for (size_t i = 0; i < localVerts.size(); ++i) {
        physVerts[i] = phys::Vec2(localVerts[i].x, localVerts[i].y);
    }

    // Передаем вершины в Box2D (он сам сделает их выпуклыми и найдет центр)
    shape->Set(physVerts, static_cast<int32_t>(localVerts.size()));
    body.shape = shape;

    // 3. Вычисляем массу и момент инерции
    phys::MassData massData;
    shape->ComputeMass(&massData, density);

    body.mass = massData.mass;
    body.invMass = (body.mass > 0.0f) ? 1.0f / body.mass : 0.0f;
    body.inertia = massData.I;
    body.invInertia = (body.inertia > 0.0f) ? 1.0f / body.inertia : 0.0f;

    // 4. Физические свойства
    body.friction = 0.3f;
    body.restitution = 0.2f;
    body.angularDamping = 0.1f;
    body.isSleeping = false;
    body.velocity = { 0,0 };

    // 5. Добавляем в мир
    size_t bodyIdx = physics.addBody(body);

    // 6. Создаем визуальный объект
    PolygonVk q;
    for (size_t i = 0; i < localVerts.size(); ++i) {
        q.points[i] = localVerts[i];
    }
    q.vertexCount = static_cast<int>(localVerts.size());
    q.color = color;
    q.physicsIndex = static_cast<uint32_t>(bodyIdx);

    polygons.push_back(q);
}

void Scene::addRandomPolygon(PhysicsWorld& physics) {
    static std::random_device rd;
    static std::mt19937 gen(rd());

    // 1. Случайная позиция центра
    std::uniform_real_distribution<float> posXDist(100.0f, WIDTH - 100.0f);
    std::uniform_real_distribution<float> posYDist(100.0f, HEIGHT - 100.0f);
    glm::vec2 pos = { posXDist(gen), posYDist(gen) };

    // 2. Случайное количество вершин (от 3 до 8)
    std::uniform_int_distribution<int> vertexCountDist(3, 8);
    int numVertices = vertexCountDist(gen);

    // 3. Генерируем вершины (локально вокруг 0,0)
    std::uniform_real_distribution<float> sizeDist(15.0f, 50.0f);

    // Уменьшаем разброс угла в зависимости от количества вершин, 
    // чтобы фигура не получилась самопересекающейся
    float jitterAmount = (phys::pi / numVertices) * 0.4f;
    std::uniform_real_distribution<float> angleJitterDist(-jitterAmount, jitterAmount);

    std::vector<glm::vec2> localVerts(numVertices);
    bool generate_again = false;
    do
    {
        generate_again = false;
        for (int i = 0; i < numVertices; ++i) {
            // Равномерно распределяем базовые углы
            float baseAngle = (2.0f * phys::pi * i) / numVertices;
            float angle = baseAngle + angleJitterDist(gen);
            float r = sizeDist(gen);
            localVerts[i] = { r * std::cos(angle), r * std::sin(angle) };
        }
        for (int i = 0; i < numVertices; ++i)   //используем знак векторного произведения, чтобы отсечь невыпуклые многоугольники
        {
            glm::vec2 a = { localVerts[(i + numVertices - 1) % numVertices].x - localVerts[(i - 2 + numVertices) % numVertices].x, localVerts[(i - 1 + numVertices) % numVertices].y - localVerts[(i - 2 + numVertices) % numVertices].y };
            glm::vec2 b = { localVerts[i].x - localVerts[(i - 1 + numVertices) % numVertices].x, localVerts[i].y - localVerts[(i - 1 + numVertices) % numVertices].y };
            if (a.x * b.y - a.y * b.x < 0)
            {
                generate_again = true;
                break;
            }
        }
    } while (generate_again);
    
    // 4. Случайный цвет
    std::uniform_real_distribution<float> colorDist(0.0f, 1.0f);
    glm::vec4 color = { colorDist(gen), colorDist(gen), colorDist(gen), 1.0f };

    // 5. Случайная масса и вычисление плотности
    std::uniform_real_distribution<float> massDist(1.0f, 5.0f);
    float mass = massDist(gen);

    // Площадь выпуклого N-угольника (Формула шнуровки / Shoelace formula)
    float area = 0.0f;
    for (int i = 0; i < numVertices; ++i) {
        int j = (i + 1) % numVertices;
        area += localVerts[i].x * localVerts[j].y - localVerts[j].x * localVerts[i].y;
    }
    area = std::abs(area) * 0.5f;

    // Вычисляем плотность (масса / площадь)
    float density = (area > 0.0f) ? (mass / area) : 1.0f;

    // 6. Вызываем общую функцию создания полигона
    addPhysicalPolygon(physics, pos, localVerts, color, density);
}