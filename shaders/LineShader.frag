#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragColor;
layout(location = 2) in float fragIsCircle;

layout(location = 0) out vec4 outColor;

void main() {
    float dist;
    if (fragIsCircle > 0.5) {
        dist = length(fragUV);
    } else {
        dist = abs(fragUV.y);
    }
    
    if (dist > 1.0) discard;
    
    // Идеальное сглаживание на основе производных
    float w = fwidth(dist);
    float alpha = 1.0 - smoothstep(1.0 - w, 1.0, dist);
    
    outColor = vec4(fragColor.rgb, fragColor.a * alpha);
}