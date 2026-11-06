#include "Config.h"
#include "MainWindow.h"



void MainApp::initVulkan()
{
    ZoneScopedN("Init Vulkan");

    context.createInstance();
    context.createSurface(window);
    context.pickPhysicalDevice();
    context.createLogicalDevice();  //по возможности создаёт устройство с разными семействами очередей для всех операций
    // 2. Передаем VkDevice остальным классам, чтобы они знали, через какой девайс им удаляться
    swapchain.setDevice(context.device);
    resources.setDevice(context.device);
    pipelines.setDevice(context.device);
    renderer.setDevice(context.device);

    swapchain.createSwapChain(context);
    swapchain.createImageViews();
    if (context.MSAAEnabled())
        swapchain.createColorResources(context);

    resources.createDescriptorSetLayout();

    pipelines.createPipelineLayout(resources.descriptorSetLayout);
    CircleConfig circleConfig;
    pipelines.circlePipeline = pipelines.createGraphicsPipeline(
        circleConfig, 
        pipelines.pipelineLayout, 
        swapchain.swapChainImageFormat, 
        VK_FORMAT_UNDEFINED, 
        context.MSAAEnabled() ? context.msaaSamples : VK_SAMPLE_COUNT_1_BIT);
    QuadConfig quadConfig;
    pipelines.quadPipeline = pipelines.createGraphicsPipeline(
        quadConfig,
        pipelines.pipelineLayout,
        swapchain.swapChainImageFormat,
        VK_FORMAT_UNDEFINED,  // depth не используется в 2D
        context.MSAAEnabled() ? context.msaaSamples : VK_SAMPLE_COUNT_1_BIT
    );

    renderer.createCommandPool(context.queueFamilyIndices);
    renderer.createCommandBuffers();

    resources.createUniformBuffers(context.physicalDevice);
    resources.createDescriptorPool();
    resources.createDescriptorSets();

    renderer.createSyncObjects();
    swapchain.createSwapchainSemaphores();
    renderer.createPresentCommandResources(context.queueFamilyIndices, swapchain);
    scene.AddQuad({ 100,100 }, { 200,120 }, { 200,60 }, { 100,70 }, { 0,1,0.4,0 });
    // Создаём контекст Tracy для GPU-профилирования
    tracyVkCtx = TracyVkContext(context.physicalDevice, context.device, context.graphicsQueue, renderer.graphicsCommandBuffers[0]);
    // Можно дать контексту понятное имя
    TracyVkContextName(tracyVkCtx, "Main Vulkan Context", 19);
}




























































