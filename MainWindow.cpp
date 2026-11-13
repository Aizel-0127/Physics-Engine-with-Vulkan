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
        physicsWorld.update(deltaTime, swapchain.currentWidth, swapchain.currentHeight);

        // 2. Синхронизируем визуальные данные с физическими
        // Синхронизация кругов (простая)
        for (auto& c : scene.circles) {
            if (c.physicsIndex != UINT32_MAX) {
                c.center = physicsWorld.getBody(c.physicsIndex).position;
            }
        }

        // Синхронизация квадов (с учетом вращения)
        for (auto& q : scene.quads) {
            if (q.physicsIndex != UINT32_MAX) {
                const auto& body = physicsWorld.getBody(q.physicsIndex);
                // Трансформируем локальные углы квада через позицию и поворот тела
                // (пока поворота нет — просто сдвигаем)
                for (auto& p : q.points) {
                    // p_local + body.position (упрощенно)
                }
            }
        }

        // Синхронизация линий (если мягкая — двигаем каждую точку)
        for (auto& line : scene.lines) {
            // В зависимости от логики
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

    // Крупный мяч в центре
    scene.addPhysicalCircle(physicsWorld, { WIDTH / 2.0f, HEIGHT / 2.0f }, 240.0f, { 1.0f, 0.5f, 0.0f, 1.0f }, 1000.0f);

    // Меньший мяч
    scene.addPhysicalCircle(physicsWorld, { WIDTH / 4.0f, 3.0f * HEIGHT / 4.0f }, 100.0f, { 0.0f, 0.5f, 0.7f, 1.0f }, 200.0f);

    // Мелкий мяч
    scene.addPhysicalCircle(physicsWorld, { WIDTH * 5.0f / 7.0f, HEIGHT * 6.0f / 8.0f }, 30.0f, { 0.2f, 0.2f, 0.8f, 1.0f }, 100.0f);
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