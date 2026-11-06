#version 450

// Входные данные из вершинного шейдера
layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 localPos;

// Выходной цвет
layout(location = 0) out vec4 outColor;

void main() {
    // localPos находится в диапазоне [-1, 1] по обеим осям.
    // Проверяем расстояние от центра (0,0) до текущей точки.
    // Если оно больше 1.0 — точка находится за пределами круга.
    if (dot(localPos, localPos) > 1.0) {
        discard; // Отбрасываем пиксель
    }
    
    outColor = fragColor;
}