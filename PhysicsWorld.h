#pragma once
#include "Config.h"

enum class BodyType {
    Static,     // Неподвижное (стены, земля). inverseMass = 0
    Kinematic,  // Движется по скрипту, силы игнорируются (для будущих платформ)
    Dynamic     // Полноценная физика
};

enum class ColliderType {
    Circle,
    Box,      // Для будущих квадов
    Segment   // Для будущих линий
};

struct Collider2D {
    ColliderType type = ColliderType::Circle;

    union {
        struct { float radius; } circle;
        struct { glm::vec2 halfExtents; } box;
        struct { glm::vec2 start, end; float thickness; } segment;
    };
};

struct GravitySettings {
    // 1. Нисходящая (однородная) гравитация
    bool enableDownward = true;
    glm::vec2 downwardDirection = { 0.0f, 1.0f }; // Y вниз (как в экранных координатах)
    float downwardStrength = 980.0f;            // пикселей в секунду^2

    // 2. Взаимная (ньютоновская) гравитация между телами
    bool enableMutual = true;
    float gravitationalConstant = 1000.0f;       // Подбирается для геймплея (не реальное G)

    float mutualSoftening = 50.0f; // Радиус смягчения в пикселях
};

struct Rigidbody2D {
    // Кинематика
    glm::vec2 position = { 0.0f, 0.0f };
    glm::vec2 velocity = { 0.0f, 0.0f };
    glm::vec2 forceAccumulator = { 0.0f, 0.0f }; // Сбрасывается каждый шаг

    // Масса
    float mass = 1.0f;
    float inverseMass = 1.0f; // Храним обратную величину (удобно для импульсов)

    // Материал (пригодится на следующих этапах)
    float restitution = 0.5f;
    float friction = 0.3f;

    // Конфигурация
    BodyType type = BodyType::Dynamic;
    bool isActive = true;

    Collider2D collider;

    // Удобные сеттеры
    void setMass(float m) {
        mass = m;
        inverseMass = (m > 0.0f) ? (1.0f / m) : 0.0f;
    }
    void makeStatic() {
        type = BodyType::Static;
        setMass(0.0f);
        velocity = glm::vec2(0.0f);
    }
};

class PhysicsWorld {
public:
    GravitySettings gravitySettings;

    // Управление телами
    size_t addBody(const Rigidbody2D& body);
    Rigidbody2D& getBody(size_t index) { return bodies[index]; }
    const std::vector<Rigidbody2D>& getBodies() const { return bodies; }
    size_t getBodyCount() const { return bodies.size(); }
    void clearBodies();
    // Симуляция
    void update(float realDeltaTime, float currentWidth, float currentHeight); // Вызывается каждый кадр

private:
    void resolveBoundaryCollisions(float width, float height);
    bool checkCollision(const Rigidbody2D& a, const Rigidbody2D& b, glm::vec2& normal, float& penetration);
    void resolveCollision(Rigidbody2D& a, Rigidbody2D& b, const glm::vec2& normal, float penetration);
    void resolveBodyCollisions();
    void step(float fixedDt, float currentWidth, float currentHeight);          // Один шаг физики
    void applyGravity();               // Применение гравитации к силам
    void integrate(float dt);          // Полу-неявный Эйлер

    std::vector<Rigidbody2D> bodies;
    float accumulator = 0.0f;

    static constexpr float FIXED_TIMESTEP = 1.0f / 120.0f; // 120 Гц
    static constexpr float MAX_FRAME_TIME = 0.25f;          // Защита от spiral of death
};