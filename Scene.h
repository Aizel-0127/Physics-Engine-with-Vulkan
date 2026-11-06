#pragma once
#include "Config.h"
#include "Primitives.h"

class Scene
{
public:
    std::vector<CircleData> circles = { {
    glm::vec2(WIDTH / 2.0f, HEIGHT / 2.0f),  // center
    240.0f,                                     // radius
    0.0f,                                      // padding
    glm::vec4(1.0f, 0.5f, 0.0f, 1.0f)          // color
        },{
    glm::vec2(WIDTH / 4.0f, 3.0 * HEIGHT / 4.0f),  // center
    100.0f,                                     // radius
    0.0f,                                      // padding
    glm::vec4(0.0f, 0.5f, 0.7f, 1.0f)          // color
        },{
    glm::vec2(WIDTH * 5.0 / 7.0f, HEIGHT * 6.0 / 8.0f),  // center
    30.0f,                                     // radius
    0.0f,                                      // padding
    glm::vec4(0.2f, 0.2f, 0.8f, 1.0f)          // color 
        } };
    std::vector<Quad> quads;
    std::vector<Vertex> quadVertices;

    size_t AddQuad(glm::vec2 topLeft, glm::vec2 topRight, glm::vec2 bottomRight, glm::vec2 bottomLeft, glm::vec4 color);
    size_t AddQuad(const Quad&);
    void UpdateQuadVertices();
};