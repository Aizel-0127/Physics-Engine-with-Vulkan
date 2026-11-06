#include "Scene.h"

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