#include "Config.h"
#include "MainWindow.h"


void MainApp::mainLoop()
{
    // 1. Запускаем поток рендеринга перед началом основного цикла
    isRunning = true;
    renderThread = std::thread(&MainApp::renderLoop, this);

    // 2. Основной цикл теперь отвечает ТОЛЬКО за события окна
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Здесь мы можем добавить sleep, чтобы основной поток не жрал 100% CPU,
        // если окно просто висит на фоне. Но для начала можно и так.
    }

    // 3. Когда окно закрывается, сообщаем потоку рендеринга остановиться
    isRunning = false;

    // 4. Ждем, пока поток рендеринга завершит свою работу
    if (renderThread.joinable()) {
        renderThread.join();
    }

    // 5. vkDeviceWaitIdle(device); - уже сделано в MainApp::run()
    
}

// Новая функция, которая выполняется в отдельном потоке
void MainApp::renderLoop()
{
    auto lastTime = std::chrono::high_resolution_clock::now();
    while (isRunning)
    {
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;
        RECT rect;
        if (GetClientRect(hwnd, &rect)) // Получаем актуальный размер клиентской области
        {
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;

            // Если размер изменился по сравнению с текущим swapchain
            if (width > 0 && height > 0 && (width != swapchain.currentWidth || height != swapchain.currentHeight))
            {
                swapchain.currentWidth = width;
                swapchain.currentHeight = height;
                swapchain.framebufferResized = true; // Сигнализируем о необходимости пересоздать swapchain
            }
        }

        if (swapchain.currentWidth == 0 || swapchain.currentHeight == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        // 1. Обновляем физику
        physicsWorld.update(deltaTime);

        // 2. Синхронизируем визуальные объекты с физическими
        const auto& bodies = physicsWorld.getBodies();

        // --- Синхронизация КРУГОВ (просто перенос позиции) ---
        for (auto& circle : scene.circles) {
            if (circle.physicsIndex != UINT32_MAX && circle.physicsIndex < bodies.size()) {
                const auto& body = bodies[circle.physicsIndex];
                circle.center = { body.transform.p.x, body.transform.p.y };
            }
        }

        // --- Синхронизация КВАДОВ (позиция + вращение) ---
        for (auto& quad : scene.polygons) {
            if (quad.physicsIndex != UINT32_MAX && quad.physicsIndex < bodies.size()) {
                const auto& body = bodies[quad.physicsIndex];

                // ВНИМАНИЕ: Мы НЕ меняем quad.points напрямую!
                // Вместо этого мы передаем трансформацию в ваш метод UpdateQuadVertices,
                // который сгенерирует мировые вершины для GPU.
            }
        }

        // --- Синхронизация ЛИНИЙ (позиция + вращение) ---
        for (auto& line : scene.lines) {
            if (line.physicsIndex != UINT32_MAX && line.physicsIndex < bodies.size()) {
                const auto& body = bodies[line.physicsIndex];
                // То же самое: не меняем line.points, а применяем трансформацию при рендере.
            }
        }
        renderer.drawFrame(context, swapchain, pipelines, resources, scene,physicsWorld, tracyVkCtx);
        FrameMark;
    }
}

void MainApp::initWindow()
{
    if (!glfwInit())
        throw std::runtime_error("Failed to initialize GLFW");
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); //Not using OpenGL

    window = glfwCreateWindow(WIDTH, HEIGHT, "DaGame", nullptr, nullptr);
    if (!window) {
        throw std::runtime_error("Failed to create GLFW window");
    }

    // Получаем HWND и сохраняем в поле класса
    hwnd = glfwGetWin32Window(window);

    glfwSetWindowUserPointer(window, this);
    //glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
}

void MainApp::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto app = reinterpret_cast<MainApp*>(glfwGetWindowUserPointer(window));
    app->swapchain.currentWidth = width;
    app->swapchain.currentHeight = height;
    app->swapchain.framebufferResized = true;
}



void MainApp::initScene() {
    // =========================================================
    // 1. Создаем стены (Статические объекты)
    // =========================================================
    float wallThickness = 50.0f;
    glm::vec4 wallColor = { 0.3f, 0.3f, 0.3f, 1.0f };

    // Пол (нижняя стена)
    scene.addStaticQuad(physicsWorld,
        { WIDTH / 2.0f, HEIGHT + wallThickness / 2.0f },
        { WIDTH / 2.0f, wallThickness / 2.0f },
        wallColor);

    // Потолок (верхняя стена)
    scene.addStaticQuad(physicsWorld,
        { WIDTH / 2.0f, -wallThickness / 2.0f },
        { WIDTH / 2.0f, wallThickness / 2.0f },
        wallColor);

    // Левая стена
    scene.addStaticQuad(physicsWorld,
        { -wallThickness / 2.0f, HEIGHT / 2.0f },
        { wallThickness / 2.0f, HEIGHT / 2.0f },
        wallColor);

    // Правая стена
    scene.addStaticQuad(physicsWorld,
        { WIDTH + wallThickness / 2.0f, HEIGHT / 2.0f },
        { wallThickness / 2.0f, HEIGHT / 2.0f },
        wallColor);

    // =========================================================
    // 2. Создаем тестовые круги (Динамические объекты)
    // =========================================================
    scene.addPhysicalCircle(physicsWorld,
        { WIDTH / 2.0f, HEIGHT / 2.0f }, 100.0f,
        { 1.0f, 0.5f, 0.0f, 1.0f }, 10.0f);

    scene.addPhysicalCircle(physicsWorld,
        { WIDTH / 4.0f, HEIGHT / 4.0f }, 50.0f,
        { 0.0f, 0.5f, 0.7f, 1.0f }, 2.0f);

    scene.addPhysicalCircle(physicsWorld,
        { WIDTH * 3.0f / 4.0f, HEIGHT / 4.0f }, 30.0f,
        { 0.2f, 0.2f, 0.8f, 1.0f }, 1.0f);

    // =========================================================
    // 3. Создаем произвольный выпуклый четырёхугольник (Трапеция)
    // =========================================================
    std::vector<glm::vec2> trapezoidVerts = {
        glm::vec2(-60.0f,  30.0f),
        glm::vec2(60.0f,  30.0f),
        glm::vec2(30.0f, -30.0f),
        glm::vec2(-30.0f, -30.0f)
    };

    scene.addPhysicalPolygon(physicsWorld,
        { WIDTH / 2.0f, HEIGHT / 3.0f },
        trapezoidVerts,
        { 0.2f, 0.8f, 0.2f, 1.0f },
        1.0f
    );
}

int main() {
    MainApp app;

    try {
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}