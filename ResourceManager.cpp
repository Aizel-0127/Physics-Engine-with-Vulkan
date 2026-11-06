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

void ResourceManager::createUniformBuffers(VkPhysicalDevice physicalDevice)
{
    // Размер одного UBO: все круги для одного кадра
    VkDeviceSize bufferSize = sizeof(CircleData) * MAX_CIRCLES;

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
    }
}

void ResourceManager::createDescriptorPool()
{
    // Описание количества дескрипторов каждого типа.
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    // Описание пула.
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.pNext = nullptr;
    poolInfo.flags = 0;  // без флагов — sets не сбрасываются индивидуально
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
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
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);

    // 2. Описание аллокации.
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.pNext = nullptr;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();

    // 3. Выделение наборов.
    if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
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
        descriptorWrite.dstSet = descriptorSets[i];                  // ← в какой набор
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
}

void ResourceManager::createDescriptorSetLayout()
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

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
}