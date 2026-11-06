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
    while (isRunning)
    {
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

        renderer.drawFrame(context, swapchain, pipelines, resources, scene, tracyVkCtx);
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