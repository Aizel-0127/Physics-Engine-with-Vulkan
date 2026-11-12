#version 450
// Входные данные из вершинного шейдера
layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 localPos; // <-- Вот она, переменная

// Выходной цвет
layout(location = 0) out vec4 outColor;

void main() {
    float dist = length(localPos); // <-- ИСПРАВЛЕНО: используем localPos вместо localUV
    
    // Отсекаем всё, что снаружи
    if (dist > 1.0) discard;

    // Плавный переход на границе (ширина = 1 пиксель)
    float w = fwidth(dist);
    float alpha = 1.0 - smoothstep(1.0 - w, 1.0, dist);
    
    outColor = vec4(fragColor.rgb, fragColor.a * alpha);
}