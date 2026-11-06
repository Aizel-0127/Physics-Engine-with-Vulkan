#include "PipelineManager.h"

VkShaderModule PipelineManager::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }
    return shaderModule;
}

VkShaderStageFlagBits PipelineManager::stageFromExtension(const std::string& path) {
    // Извлекаем имя файла без пути
    std::filesystem::path p(path);
    std::string filename = p.filename().string();

    // Отбрасываем расширение .spv, если оно есть
    // (шaders компилируются в CircleShader.vert.spv, CircleShader.frag.spv и т.д.)
    const std::string spvExt = ".spv";
    if (filename.size() > spvExt.size() &&
        filename.compare(filename.size() - spvExt.size(), spvExt.size(), spvExt) == 0) {
        filename = filename.substr(0, filename.size() - spvExt.size());
    }

    // Извлекаем расширение шейдера (.vert, .frag, .geom, ...)
    std::string ext = std::filesystem::path(filename).extension().string();

    // Таблица соответствия расширений и стадий
    static const std::unordered_map<std::string, VkShaderStageFlagBits> extensionMap = {
        {".vert",  VK_SHADER_STAGE_VERTEX_BIT},
        {".frag",  VK_SHADER_STAGE_FRAGMENT_BIT},
        {".geom",  VK_SHADER_STAGE_GEOMETRY_BIT},
        {".tesc",  VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT},
        {".tese",  VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT},
        {".comp",  VK_SHADER_STAGE_COMPUTE_BIT},
        {".rgen",  VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {".rahit", VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
        {".rchit", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
        {".rmiss", VK_SHADER_STAGE_MISS_BIT_KHR},
        {".rint",  VK_SHADER_STAGE_INTERSECTION_BIT_KHR},
        {".rcall", VK_SHADER_STAGE_CALLABLE_BIT_KHR},
        {".mesh",  VK_SHADER_STAGE_MESH_BIT_EXT},
        {".task",  VK_SHADER_STAGE_TASK_BIT_EXT},
    };

    auto it = extensionMap.find(ext);
    if (it == extensionMap.end()) {
        throw std::runtime_error(
            "Unknown shader extension '" + ext + "' in file: " + path
        );
    }
    return it->second;
}

void PipelineManager::createPipelineLayout(VkDescriptorSetLayout descriptorSetLayout)
{
    // 1. Push constant range
   //    Описывает, какие push-константы доступны шейдерам.
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;  // доступно только в вершинном шейдере
    pushConstantRange.offset = 0;                                // смещение в буфере push-констант
    pushConstantRange.size = sizeof(glm::mat4);                  // 64 байта для матрицы проекции

    // 2. Pipeline layout create info
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pNext = nullptr;
    pipelineLayoutInfo.flags = 0;

    // Дескрипторные сеты: массив layout'ов, которые будут доступны шейдерам.
    // У нас один layout — для UBO с массивом кругов.
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

    // Push-константы: массив диапазонов.
    // У нас один — mat4 projection.
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    // 3. Создание
    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }
}

VkPipeline PipelineManager::createGraphicsPipeline(const BasePipelineConfig& config, VkPipelineLayout layout, VkFormat colorFormat, VkFormat depthFormat, VkSampleCountFlagBits samples)
{
    std::vector<VkPipelineShaderStageCreateInfo> stages;
    std::vector<VkShaderModule> modules;  // для уничтожения после создания pipeline
    stages.reserve(config.shaderPaths.size());
    modules.reserve(config.shaderPaths.size());

    for (const auto& path : config.shaderPaths) {
        auto code = readFile(path);
        VkShaderModule module = createShaderModule(code);
        modules.push_back(module);

        VkPipelineShaderStageCreateInfo stageInfo{};
        stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.stage = stageFromExtension(path);
        stageInfo.module = module;
        stageInfo.pName = config.entryPoint;
        stages.push_back(stageInfo);
    }

    VkPipelineMultisampleStateCreateInfo multisampling = config.multisampling;
    multisampling.rasterizationSamples = samples;

    VkPipelineColorBlendAttachmentState blendAttachment = config.makeBlendAttachment();
    VkPipelineColorBlendStateCreateInfo colorBlending = config.colorBlending;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &blendAttachment;

    VkPipelineRenderingCreateInfoKHR renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &colorFormat;
    renderingInfo.depthAttachmentFormat = depthFormat;         // VK_FORMAT_UNDEFINED для 2D
    renderingInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    // 5. Собираем VkGraphicsPipelineCreateInfo из конфига + локальных копий
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &renderingInfo;
    pipelineInfo.flags = 0;

    pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages = stages.data();

    pipelineInfo.pVertexInputState = &config.vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &config.inputAssembly;
    pipelineInfo.pTessellationState = nullptr;  // для 2D не используется
    pipelineInfo.pViewportState = &config.viewportState;
    pipelineInfo.pRasterizationState = &config.rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;    // локальная копия
    pipelineInfo.pDepthStencilState = &config.depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;    // локальная копия
    pipelineInfo.pDynamicState = &config.dynamicState;

    pipelineInfo.layout = layout;                  // параметр функции
    pipelineInfo.renderPass = VK_NULL_HANDLE;      // dynamic rendering
    pipelineInfo.subpass = 0;

    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    // 6. Создание pipeline
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult result = vkCreateGraphicsPipelines(
        device,
        VK_NULL_HANDLE,           // pipeline cache (пока нет)
        1, &pipelineInfo,
        nullptr,
        &pipeline
    );

    // 7. Уничтожение шейдерных модулей (они больше не нужны)
    for (VkShaderModule module : modules) {
        vkDestroyShaderModule(device, module, nullptr);
    }

    if (result != VK_SUCCESS) {
        throw std::runtime_error(
            "failed to create graphics pipeline! VkResult = " +
            std::to_string(static_cast<int>(result))
        );
    }

    return pipeline;
}