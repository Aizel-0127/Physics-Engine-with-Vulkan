#pragma once
#include "Config.h"
#include "Physics/PhysicsMath.h"
#include "Physics/PhysicsSettings.h"
#include "Physics/Shape.h"


const int ITERATIONS_FOR_COLLISIONS = 4; // Для 100 шаров хватит 4-8

enum class BodyType {
    Static,     // Неподвижное (стены, земля). invMass = 0
    Kinematic,  // Движется по скрипту, силы игнорируются (для будущих платформ)
    Dynamic     // Полноценная физика
};



struct GravitySettings {
    // 1. Нисходящая (однородная) гравитация
    bool enableDownward = true;
    phys::Vec2 downwardDirection = { 0.0f, 1.0f }; // Y вниз (как в экранных координатах)
    float downwardStrength = 980.0f;            // пикселей в секунду^2

    // 2. Взаимная (ньютоновская) гравитация между телами
    bool enableMutual = false;
    float gravitationalConstant = 1000.0f;       // Подбирается для геймплея (не реальное G)

    float mutualSoftening = 50.0f; // Радиус смягчения в пикселях
};

struct Rigidbody2D {
    phys::Transform transform; // Заменяет position и angle
    phys::Vec2 velocity{0.0f, 0.0f};
    float angularVelocity = 0.0f;

    phys::Vec2 forceAccumulator = phys::Vec2(0.0f, 0.0f);
    float torque = 0.0f;             // Накопленный момент сил
    float angularDamping = 0.0f;     // Коэффициент затухания вращения (0 = нет затухания)

    float mass = 1.0f;
    float invMass = 1.0f;
    float inertia = 1.0f;
    float invInertia = 1.0f;

    float restitution = 0.5f;
    float friction = 0.3f;

    bool isSleeping = false;
    float sleepTimer = 0.0f; // Сколько времени объект почти неподвижен

    phys::Shape* shape = nullptr; // Указатель на геометрию
    BodyType type = BodyType::Dynamic;

    // Удобные сеттеры
    void setMass(float m) {
        mass = m;
        invMass = (m > 0.0f) ? (1.0f / m) : 0.0f;
    }
    void makeStatic() {
        type = BodyType::Static;
        setMass(0.0f);
        velocity = phys::Vec2(0.0f,0.0f);
    }
};

// Структура для хранения контакта (аналог b2Contact)
struct ContactConstraint {
    Rigidbody2D* bodyA;
    Rigidbody2D* bodyB;
    phys::Manifold manifold;
    phys::Vec2 normal;         // Нормаль в мировых координатах
    float friction;
    float restitution;

    // Данные для точек контакта (для Warm Starting)
    struct PointConstraint {
        phys::Vec2 rA;
        phys::Vec2 rB;
        float separation = 0.0f;
        float normalImpulse = 0.0f;
        float tangentImpulse = 0.0f;
        float normalMass = 0.0f;
        float tangentMass = 0.0f;
        float velocityBias = 0.0f;
    } points[2];
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
    size_t addStaticBox(glm::vec2 center, glm::vec2 halfExtents);
    // Симуляция
    void update(float realDeltaTime); // Вызывается каждый кадр

    ~PhysicsWorld();

private:
    void findCollisions(std::vector<ContactConstraint>& contacts);
    
    void matchAndTransferImpulses(std::vector<ContactConstraint>& newContacts);
    void step(float fixedDt);          // Один шаг физики
    void applyGravity();               // Применение гравитации к силам
    void integrate(float dt);          // Полу-неявный Эйлер

    void prepareContact(ContactConstraint& cc);

    void warmStartContact(ContactConstraint& cc);

    void solveContactVelocity(ContactConstraint& cc);

    bool solveContactPosition(ContactConstraint& cc);

    std::vector<ContactConstraint> m_persistentContacts;
    std::vector<Rigidbody2D> bodies;
    float accumulator = 0.0f;

    static constexpr float FIXED_TIMESTEP = 1.0f / 120.0f; // 120 Гц
    static constexpr float MAX_FRAME_TIME = 0.25f;          // Защита от spiral of death
};