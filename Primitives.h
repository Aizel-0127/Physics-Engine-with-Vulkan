#pragma once
#include "Config.h"

// Данные одного круга (в C++)
struct CircleData {
    glm::vec2 center;   // 8 байт (offset 0)
    float radius;       // 4 байта (offset 8)
    float padding;      // 4 байта (offset 12) — чтобы следующий vec4 был выровнен
    glm::vec4 color;    // 16 байт (offset 16)
};                      // Итого: 32 байта на круг

struct Vertex {
    glm::vec2 pos;
    glm::vec4 color;
};

struct Quad
{
    // Порядок вершин: 0 - слева сверху и дальше по часовой
    std::array<glm::vec2, 4> points;
    glm::vec4 color;
};