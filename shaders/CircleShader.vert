#version 450

// Данные для каждого круга (индексируется по gl_InstanceIndex)
struct CircleData {
    vec2 center;
    float radius;
    float padding;
    vec4 color;
};

// UBO с массивом кругов
layout(std140, binding = 0) uniform CircleBuffer {
    CircleData circles[100];
};

// Push-константы для глобальных данных (например, матрица проекции)
layout(push_constant) uniform PushConstants {
    mat4 projection;
} pc;

// Выходные данные для фрагментного шейдера
layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 localPos;

void main() {
    // Получаем данные текущего круга по индексу инстанса
    CircleData circle = circles[gl_InstanceIndex];
    
    // Генерируем позиции квадрата от -1 до 1 по обеим осям
    vec2 quadPos = vec2(
        float((gl_VertexIndex << 1) & 2) - 1.0,
        float(gl_VertexIndex & 2) - 1.0
    );
    
    // Масштабируем квадрат до радиуса круга и смещаем к его центру
    vec2 worldPos = circle.center + quadPos * circle.radius;
    
    // Преобразуем в clip space
    gl_Position = pc.projection * vec4(worldPos, 0.0, 1.0);
    
    // Передаём данные во фрагментный шейдер
    fragColor = circle.color;
    localPos = quadPos; // Координаты в диапазоне [-1, 1]
}