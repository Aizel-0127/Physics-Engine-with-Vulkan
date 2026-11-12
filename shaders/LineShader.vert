#version 450

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inColor;
layout(location = 3) in float inIsCircle;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec4 fragColor;
layout(location = 2) out float fragIsCircle;

layout(push_constant) uniform PushConstants {
    mat4 projection;
} pc;

void main() {
    gl_Position = pc.projection * vec4(inPos, 0.0, 1.0);
    fragUV = inUV;
    fragColor = inColor;
    fragIsCircle = inIsCircle;
}