#include "Scene.h"
#include <cmath> // Для std::sin
#include <random>
#include "PhysicsWorld.h"

size_t Scene::AddQuad(glm::vec2 topLeft, glm::vec2 topRight, glm::vec2 bottomRight, glm::vec2 bottomLeft, glm::vec4 color)
{
    Quad quad;
    quad.color = color;
    quad.points[0] = topLeft;
    quad.points[1] = topRight;
    quad.points[2] = bottomRight;
    quad.points[3] = bottomLeft;
    return AddQuad(quad);
}

size_t Scene::AddQuad(const Quad& quad)
{
    size_t index = quads.size();
    quads.push_back(quad);

    //VkDeviceSize requiredSize = quads.size() * 6 * sizeof(Vertex);

    //if (requiredSize > quadBufferSize) {

    //    quadBufferSize = requiredSize + (sizeof(Vertex) * 6 * 500); // Запас на 500 квадов 

    //    vkDeviceWaitIdle(device);
    //    // 1. Уничтожаем старые буферы и Создаем буфер в видеопамяти (DEVICE_LOCAL)
    //    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    //        vkDestroyBuffer(device, quadVertexBuffers[i], nullptr);
    //        vkFreeMemory(device, quadVertexBufferMemory[i], nullptr);
    //        createBuffer(
    //            quadBufferSize,
    //            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, // <-- Важно: TRANSFER_DST
    //            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,                                  // <-- Важно: DEVICE_LOCAL
    //            quadVertexBuffers[i],
    //            quadVertexBufferMemory[i]
    //        );
    //        vkDestroyBuffer(device, quadStagingBuffers[i], nullptr);
    //        vkFreeMemory(device, quadStagingBufferMemory[i], nullptr);
    //        createBuffer(
    //            quadBufferSize,
    //            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, // <-- Важно: TRANSFER_SRC
    //            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    //            quadStagingBuffers[i],
    //            quadStagingBufferMemory[i]
    //        );
    //    }
    //}
    return index;
}

void Scene::UpdateQuadVertices()
{
    quadVertices.clear();
    quadVertices.reserve(quads.size() * 6); // 6 вершин на квад

    for (const auto& quad : quads) {
        // Треугольник 1: 0, 2, 3
        quadVertices.push_back({ quad.points[0], quad.color });
        quadVertices.push_back({ quad.points[2], quad.color });
        quadVertices.push_back({ quad.points[3], quad.color });

        // Треугольник 2: 0, 1, 2
        quadVertices.push_back({ quad.points[0], quad.color });
        quadVertices.push_back({ quad.points[1], quad.color });
        quadVertices.push_back({ quad.points[2], quad.color });
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
    // 1. Создаем визуал
    CircleData c{}; c.center = pos; c.radius = radius; c.color = color;
    size_t visualIdx = circles.size();
    circles.push_back(c);

    // 2. Создаем тело
    Rigidbody2D body{};
    body.position = pos;
    body.setMass(mass);
    body.restitution = 0.9f;
    // Настраиваем коллайдер
    body.collider.type = ColliderType::Circle;
    body.collider.circle.radius = radius;

    size_t bodyIdx = physics.addBody(body);

    // 3. Связываем: визуал знает про тело
    circles[visualIdx].physicsIndex = static_cast<uint32_t>(bodyIdx);
};

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