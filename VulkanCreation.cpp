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

    resources.createDescriptorSetLayouts();

    pipelines.createPipelineLayout(resources.descriptorSetLayoutGraphics);
    pipelines.createComputePipelineLayout(resources.descriptorSetLayoutCompute);
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
    LineConfig lineConfig;
    pipelines.lineGraphicsPipeline = pipelines.createGraphicsPipeline(
        lineConfig,
        pipelines.pipelineLayout, // Используем тот же layout (там есть push-константа для матрицы)
        swapchain.swapChainImageFormat,
        VK_FORMAT_UNDEFINED,
        context.MSAAEnabled() ? context.msaaSamples : VK_SAMPLE_COUNT_1_BIT
    );

    pipelines.createComputePipeline();

    renderer.createCommandPool(context.queueFamilyIndices);
    renderer.createCommandBuffers();

    resources.createRequiredBuffers(context.physicalDevice);
    resources.createDescriptorPool();
    resources.createDescriptorSets();

    renderer.createSyncObjects();
    swapchain.createSwapchainSemaphores();
    renderer.createPresentCommandResources(context.queueFamilyIndices, swapchain);
    scene.AddQuad({ 100,100 }, { 200,120 }, { 200,60 }, { 100,70 }, { 0,1,0.4,0 });
    scene.AddSineWave(
        0.0f,                   // startX (от левого края)
        static_cast<float>(WIDTH), // endX (до правого края)
        1.0f,                   // step (каждые 5 пикселей новая точка)
        100.0f,                 // amplitude (высота волны)
        HEIGHT / 2.0f,          // offsetY (сдвиг по центру экрана)
        glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), // color (красный)
        20.0f                    // thickness (толщина)
    );

    // Можно добавить ещё одну волну с другими параметрами
    scene.AddSineWave(
        0.0f, static_cast<float>(WIDTH), 10.0f, 50.0f, HEIGHT / 3.0f,
        glm::vec4(0.0f, 1.0f, 1.0f, 1.0f), 2.0f
    );
    std::vector<glm::vec2> angle;
    angle.push_back({ WIDTH / 4,HEIGHT / 4 * 3 });
    angle.push_back({ WIDTH / 4*3,HEIGHT / 4 * 3 });
    angle.push_back({ WIDTH / 4,HEIGHT / 4 });
    angle.push_back({ WIDTH / 4 * 3,HEIGHT / 4 });
    scene.AddCurve(angle, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), 10.0);
    // Создаём контекст Tracy для GPU-профилирования
    tracyVkCtx = TracyVkContext(context.physicalDevice, context.device, context.graphicsQueue, renderer.graphicsCommandBuffers[0]);
    // Можно дать контексту понятное имя
    TracyVkContextName(tracyVkCtx, "Main Vulkan Context", 19);
}




























































