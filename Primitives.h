#pragma once
#include "Config.h"

// Данные одного круга (в C++)
struct CircleData {
    glm::vec2 center;   // 8 байт (offset 0)
    float radius;       // 4 байта (offset 8)
    uint32_t physicsIndex = UINT32_MAX;
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
    uint32_t physicsIndex = UINT32_MAX;
};

struct Point {
    glm::vec2 position;
};

struct Curve {
    std::vector<Point> points;
    glm::vec4 color;
    float thickness;
    uint32_t physicsIndex = UINT32_MAX;
};

struct GPULinePoint {    //CPU -> Compute Shader
    glm::vec2 position;  // 8 байт
    float thickness;     // 4 байта
    uint32_t isEndOfLine;// 4 байта (вместо padding1)
    glm::vec4 color;     // 16 байт
};                       // Итого: 32 байта

struct LineVertex {      //Compute Shader -> Vertex Shader
    glm::vec2 pos;       // 8 байт (итоговая позиция на экране)
    glm::vec2 uv;        // 8 байт (локальные координаты от -1 до 1 для SDF)
    glm::vec4 color;     // 16 байт
    float isCircle;      // 4 байта (0.0 или 1.0)
    float padding[3];    // 12 байт (выравнивание до 48 байт)
};                       // Итого: 48 байт

enum LinePointFlags : uint32_t {
    None = 0,
    IsEndOfLine = 1 << 0, // 1
    IsSelected = 1 << 1,  // 2 (если захотите выделять точки мышкой)
    HasTexture = 1 << 2,  // 4 (если позже решите добавить текстуры)
};