#pragma once
#include "Config.h"
#include "Primitives.h"
class Scene;
class ResourceManager
{
private:
    VkDevice device = VK_NULL_HANDLE;
public:
    std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> uniformBuffers;
    std::array<VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> uniformBuffersMemory;
    std::array<void*, MAX_FRAMES_IN_FLIGHT> uniformBuffersMapped;

    std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> quadVertexBuffers;
    std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> quadStagingBuffers;
    std::array <VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> quadVertexBufferMemory;
    std::array <VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> quadStagingBufferMemory;

    std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> linesInputSSBO;
    std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> linesOutputSSBO;
    std::array <VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> linesInputSSBOMemory;
    std::array <VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> linesOutputSSBOMemory;
    std::array<void*, MAX_FRAMES_IN_FLIGHT> linesInputSSBOMapped;

    VkDescriptorSetLayout descriptorSetLayoutGraphics;
    VkDescriptorSetLayout descriptorSetLayoutCompute;
    VkDescriptorPool descriptorPool;
    VkDescriptorPool descriptorPoolImGUI;
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSetsGraphics;
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSetsCompute;

    std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> linesIndirectBuffer;
    std::array<VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> linesIndirectBufferMemory;

    VkDeviceSize quadBufferSize = 0;

    void createBuffer(VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void createRequiredBuffers(VkPhysicalDevice physicalDevice);
    void createDescriptorPools();
    void createDescriptorSets();
    void createDescriptorSetLayouts();
    void updateCircleUniformBuffer(Scene& scene,uint32_t currentImage);
    void setDevice(VkDevice device) { this->device = device; }
    void ensureQuadBufferCapacity(VkPhysicalDevice physicalDevice, size_t quadCount);

    ResourceManager()
    {
         uniformBuffers.fill(VK_NULL_HANDLE);
         uniformBuffersMemory.fill(VK_NULL_HANDLE);
         uniformBuffersMapped.fill(nullptr);
         quadVertexBuffers.fill(VK_NULL_HANDLE);
         quadStagingBuffers.fill(VK_NULL_HANDLE);
         linesInputSSBO.fill(VK_NULL_HANDLE);
         linesOutputSSBO.fill(VK_NULL_HANDLE);
         linesInputSSBOMemory.fill(VK_NULL_HANDLE);
         linesOutputSSBOMemory.fill(VK_NULL_HANDLE);
         linesIndirectBuffer.fill(VK_NULL_HANDLE);
         linesIndirectBufferMemory.fill(VK_NULL_HANDLE);
         linesInputSSBOMapped.fill(nullptr);
         quadVertexBufferMemory.fill(VK_NULL_HANDLE);
         quadStagingBufferMemory.fill(VK_NULL_HANDLE);
         descriptorSetLayoutGraphics = VK_NULL_HANDLE;
         descriptorSetLayoutCompute = VK_NULL_HANDLE;
         descriptorPool = VK_NULL_HANDLE;
         descriptorPoolImGUI = VK_NULL_HANDLE;
         descriptorSetsGraphics.fill(VK_NULL_HANDLE);
         descriptorSetsCompute.fill(VK_NULL_HANDLE);
    }
    ResourceManager(VkDevice device)
    {
        this->device = device;
        uniformBuffers.fill(VK_NULL_HANDLE);
        uniformBuffersMemory.fill(VK_NULL_HANDLE);
        uniformBuffersMapped.fill(nullptr);
        quadVertexBuffers.fill(VK_NULL_HANDLE);
        quadStagingBuffers.fill(VK_NULL_HANDLE);
        linesInputSSBO.fill(VK_NULL_HANDLE);
        linesOutputSSBO.fill(VK_NULL_HANDLE);
        linesInputSSBOMemory.fill(VK_NULL_HANDLE);
        linesOutputSSBOMemory.fill(VK_NULL_HANDLE);
        linesIndirectBuffer.fill(VK_NULL_HANDLE);
        linesIndirectBufferMemory.fill(VK_NULL_HANDLE);
        linesInputSSBOMapped.fill(nullptr);
        quadVertexBufferMemory.fill(VK_NULL_HANDLE);
        quadStagingBufferMemory.fill(VK_NULL_HANDLE);
        descriptorSetLayoutGraphics = VK_NULL_HANDLE;
        descriptorSetLayoutCompute = VK_NULL_HANDLE;
        descriptorPool = VK_NULL_HANDLE;
        descriptorPoolImGUI = VK_NULL_HANDLE;
        descriptorSetsGraphics.fill(VK_NULL_HANDLE);
        descriptorSetsCompute.fill(VK_NULL_HANDLE);
    }
    ~ResourceManager() {
        if (device == VK_NULL_HANDLE) return;

        // 1. Уничтожаем пул дескрипторов.
        // ВАЖНО: При уничтожении пула все descriptorSetsGraphics и descriptorSetsCompute
        // освобождаются автоматически. Вызывать vkFreeDescriptorSets не нужно!
        if (descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        }

        if (descriptorPoolImGUI != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, descriptorPoolImGUI, nullptr);
        }

        // 2. Уничтожаем Layout'ы дескрипторов
        if (descriptorSetLayoutGraphics != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, descriptorSetLayoutGraphics, nullptr);
        }
        if (descriptorSetLayoutCompute != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, descriptorSetLayoutCompute, nullptr);
        }

        // 3. Уничтожаем Uniform Buffers (UBO для кругов/квадов)
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (uniformBuffers[i] != VK_NULL_HANDLE) {
                vkDestroyBuffer(device, uniformBuffers[i], nullptr);
                vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
            }
        }

        // 4. Уничтожаем буферы квадов (Vertex и Staging)
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (quadVertexBuffers[i] != VK_NULL_HANDLE) {
                vkDestroyBuffer(device, quadVertexBuffers[i], nullptr);
                vkFreeMemory(device, quadVertexBufferMemory[i], nullptr);
            }
            if (quadStagingBuffers[i] != VK_NULL_HANDLE) {
                vkDestroyBuffer(device, quadStagingBuffers[i], nullptr);
                vkFreeMemory(device, quadStagingBufferMemory[i], nullptr);
            }
        }
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (linesIndirectBuffer[i] != VK_NULL_HANDLE) {
                vkDestroyBuffer(device, linesIndirectBuffer[i], nullptr);
                vkFreeMemory(device, linesIndirectBufferMemory[i], nullptr);
            }
        }

        // 5. Уничтожаем SSBO для линий (Input и Output)
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (linesInputSSBO[i] != VK_NULL_HANDLE) {
                vkDestroyBuffer(device, linesInputSSBO[i], nullptr);
                vkFreeMemory(device, linesInputSSBOMemory[i], nullptr);
            }
            if (linesOutputSSBO[i] != VK_NULL_HANDLE) {
                vkDestroyBuffer(device, linesOutputSSBO[i], nullptr);
                vkFreeMemory(device, linesOutputSSBOMemory[i], nullptr);
            }
        }
    }
};