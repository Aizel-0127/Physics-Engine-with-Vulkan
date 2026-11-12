#pragma once
#include "ResourceManager.h"
#include "SwapchainManager.h"
#include "Scene.h"
void ResourceManager::updateCircleUniformBuffer(Scene& scene, uint32_t currentImage)
{
    // 1. Копируем данные кругов в mapped UBO текущего кадра
    memcpy(uniformBuffersMapped[currentImage],
        scene.circles.data(),
        scene.circles.size() * sizeof(CircleData));
}

void ResourceManager::createBuffer(VkPhysicalDevice physicalDevice,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer& buffer,
    VkDeviceMemory& bufferMemory)
{
    // 1. Создание буфера
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }

    // 2. Требования к памяти
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    // 3. Выделение памяти
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = SwapchainManager::findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    // 4. Привязка памяти к буферу
    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

void ResourceManager::createRequiredBuffers(VkPhysicalDevice physicalDevice)
{
    // Размер одного UBO: все круги для одного кадра
    VkDeviceSize bufferSize = sizeof(CircleData) * MAX_CIRCLES;
    VkDeviceSize inputSSBOSize = MAX_TOTAL_POINTS * sizeof(GPULinePoint);
    VkDeviceSize outputSSBOSize = MAX_TOTAL_POINTS * 12 * sizeof(LineVertex);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        createBuffer(physicalDevice,
            bufferSize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            //HOST_VISIBLE — CPU может замапить память и писать в неё.
            //HOST_COHERENT — записи CPU автоматически видны GPU без вызова vkFlushMappedMemoryRanges.
            // Если бы этого флага не было, пришлось бы флашить после каждой записи, что дорого.
            uniformBuffers[i],
            uniformBuffersMemory[i]
        );

        // Замапить память ОДИН РАЗ и сохранить указатель.
        // Каждый кадр писать в uniformBuffersMapped[currentFrame] через memcpy.
        vkMapMemory(device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);

        createBuffer(physicalDevice,
            inputSSBOSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            //HOST_VISIBLE — CPU может замапить память и писать в неё.
            //HOST_COHERENT — записи CPU автоматически видны GPU без вызова vkFlushMappedMemoryRanges.
            linesInputSSBO[i],
            linesInputSSBOMemory[i]
        );
        vkMapMemory(device, linesInputSSBOMemory[i], 0, inputSSBOSize, 0, &linesInputSSBOMapped[i]);

        createBuffer(physicalDevice,
            outputSSBOSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            linesOutputSSBO[i],
            linesOutputSSBOMemory[i]
        );

        VkDeviceSize indirectSize = sizeof(VkDrawIndirectCommand); // 16 байт

        createBuffer(physicalDevice, indirectSize,
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, // <-- Оба флага!
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            linesIndirectBuffer[i], linesIndirectBufferMemory[i]);
    }
}

void ResourceManager::createDescriptorPool()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes{};

    // 1. Uniform Buffers (для графики: круги, квады)
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    // 2. Storage Buffers (для Compute Shader линий)
    // Умножаем на 3, так как каждый сет использует 3 SSBO (Input, Output, Indirect)
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 3);

    // Описание пула.
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.pNext = nullptr;
    poolInfo.flags = 0;  // без флагов — sets не сбрасываются индивидуально
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data(); // <--- ИСПОЛЬЗУЕМ .data()
    poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 2);
    //maxSets × количество дескрипторов в layout'е = descriptorCount

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

void ResourceManager::ensureQuadBufferCapacity(VkPhysicalDevice physicalDevice, size_t quadCount)
{
    VkDeviceSize requiredSize = quadCount * 6 * sizeof(Vertex);

    // Если места хватает, ничего не делаем
    if (requiredSize <= quadBufferSize) {
        return;
    }

    // Выделяем с запасом (например, на 500 квадов вперед)
    quadBufferSize = requiredSize + (sizeof(Vertex) * 6 * 500);

    // Ждем, пока GPU закончит работу, прежде чем удалять старые буферы
    vkDeviceWaitIdle(device);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        // 1. Уничтожаем старые буферы
        if (quadVertexBuffers[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, quadVertexBuffers[i], nullptr);
            vkFreeMemory(device, quadVertexBufferMemory[i], nullptr);
        }
        if (quadStagingBuffers[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, quadStagingBuffers[i], nullptr);
            vkFreeMemory(device, quadStagingBufferMemory[i], nullptr);
        }

        // 2. Создаем новые буферы в VRAM
        createBuffer(
            physicalDevice,
            quadBufferSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            quadVertexBuffers[i],
            quadVertexBufferMemory[i]
        );

        // 3. Создаем новые Staging-буферы в RAM
        createBuffer(
            physicalDevice,
            quadBufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            quadStagingBuffers[i],
            quadStagingBufferMemory[i]
        );
    }
}

void ResourceManager::createDescriptorSets()
{
    // 1. Схема для каждого набора — одна и та же (descriptorSetLayout).
    //    Для каждого кадра создаётся свой набор по этой схеме.
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayoutGraphics);

    // 2. Описание аллокации.
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.pNext = nullptr;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();

    // 3. Выделение наборов.
    if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSetsGraphics.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    std::vector<VkDescriptorSetLayout> layoutsCompute(MAX_FRAMES_IN_FLIGHT, descriptorSetLayoutCompute);
    // Выделение сетов для Compute
    VkDescriptorSetAllocateInfo computeAllocInfo{};
    computeAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    computeAllocInfo.descriptorPool = descriptorPool;
    computeAllocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    computeAllocInfo.pSetLayouts = layoutsCompute.data(); // Layout с SSBO
    if (vkAllocateDescriptorSets(device, &computeAllocInfo, descriptorSetsCompute.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    // 4. Привязка UBO к каждому набору.
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        // Описание буфера для binding'а.
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i];                      // ← конкретный UBO
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(CircleData) * MAX_CIRCLES;         // весь буфер

        // Запись в descriptor set.
        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.pNext = nullptr;
        descriptorWrite.dstSet = descriptorSetsGraphics[i];                  // ← в какой набор
        descriptorWrite.dstBinding = 0;                              // ← в какой binding (совпадает с шейдером)
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.pImageInfo = nullptr;
        descriptorWrite.pBufferInfo = &bufferInfo;
        descriptorWrite.pTexelBufferView = nullptr;

        // Применяем запись.
        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {

        // --- Настройка Input SSBO (Binding 0) ---
        VkDescriptorBufferInfo inputBufferInfo{};
        inputBufferInfo.buffer = linesInputSSBO[i];
        inputBufferInfo.offset = 0;
        inputBufferInfo.range = VK_WHOLE_SIZE;

        // --- Настройка Output SSBO (Binding 1) ---
        VkDescriptorBufferInfo outputBufferInfo{};
        outputBufferInfo.buffer = linesOutputSSBO[i];
        outputBufferInfo.offset = 0;
        outputBufferInfo.range = VK_WHOLE_SIZE;

        // --- Настройка indirect buffer (Binding 2) ---
        VkDescriptorBufferInfo indirectBufferInfo{};
        indirectBufferInfo.buffer = linesIndirectBuffer[i];
        indirectBufferInfo.offset = 0;
        indirectBufferInfo.range = VK_WHOLE_SIZE;

        // Массив структур для записи (сразу две штуки)
        std::array<VkWriteDescriptorSet, 3> descriptorWrites{};

        // Запись для Input SSBO
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSetsCompute[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &inputBufferInfo;

        // Запись для Output SSBO
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSetsCompute[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &outputBufferInfo;

        // Запись для Output SSBO
        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = descriptorSetsCompute[i];
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &indirectBufferInfo;

        // Выполняем обе записи за один вызов
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
}

void ResourceManager::createDescriptorSetLayouts()
{
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr; // Optional

    std::array<VkDescriptorSetLayoutBinding, 1> bindings = { uboLayoutBinding };
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayoutGraphics) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }

    VkDescriptorSetLayoutBinding inputBinding{};
    inputBinding.binding = 0;
    inputBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    inputBinding.descriptorCount = 1;
    inputBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    inputBinding.pImmutableSamplers = nullptr; // Optional

    VkDescriptorSetLayoutBinding outputBinding{};
    outputBinding.binding = 1;
    outputBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    outputBinding.descriptorCount = 1;
    outputBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    outputBinding.pImmutableSamplers = nullptr; // Optional

    VkDescriptorSetLayoutBinding indirectBinding{};
    indirectBinding.binding = 2;
    indirectBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    indirectBinding.descriptorCount = 1;
    indirectBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    indirectBinding.pImmutableSamplers = nullptr; // Optional

    std::array<VkDescriptorSetLayoutBinding, 3> linesBindings = { inputBinding, outputBinding, indirectBinding};
    VkDescriptorSetLayoutCreateInfo layoutInfoCompute{};
    layoutInfoCompute.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfoCompute.bindingCount = static_cast<uint32_t>(linesBindings.size());
    layoutInfoCompute.pBindings = linesBindings.data();
    if (vkCreateDescriptorSetLayout(device, &layoutInfoCompute, nullptr, &descriptorSetLayoutCompute) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
}