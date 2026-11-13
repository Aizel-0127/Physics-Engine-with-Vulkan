#pragma once
#include "Config.h"

// Подключаем остальные классы
#include "VulkanContext.h"
#include "SwapchainManager.h"
#include "ResourceManager.h"
#include "PipelineManager.h"
#include "Renderer.h"
#include "Scene.h"
#include "PipelineConfigs.h"
#include "PhysicsWorld.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_glfw.h"
#include "imgui.h"

class MainApp
{
public:
    void run()
    {
        initWindow();
        initVulkan();
        mainLoop();
        vkDeviceWaitIdle(context.device);   //гарантирует, что все операции GPU завершены до уничтожения ресурсов
        cleanup();
    }
private:
    VulkanContext context;
    SwapchainManager swapchain;
    ResourceManager resources;
    PipelineManager pipelines;
    Renderer renderer;
    Scene scene;
    PhysicsWorld physicsWorld;
    HWND hwnd = nullptr;
    
    TracyVkCtx tracyVkCtx = nullptr;

    std::thread renderThread;
    std::atomic<bool> isRunning{ true };

    GLFWwindow* window = nullptr;
    
    void initWindow();
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    void initVulkan();
    

    void initScene();
    
    
    void renderLoop();
    void mainLoop();

    void cleanup()
    {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        // 0. Уничтожаем контекст Tracy (освобождает VkQueryPool и другие внутренние ресурсы)
        if (tracyVkCtx != nullptr) {
            TracyVkDestroy(tracyVkCtx);
            tracyVkCtx = nullptr;
        }
        
        if (window) {
            glfwDestroyWindow(window);
            window = nullptr;
        }
        glfwTerminate();
    }
};



