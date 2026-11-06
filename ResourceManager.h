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

    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets;

    VkDeviceSize quadBufferSize = 0;

    void createBuffer(VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void createUniformBuffers(VkPhysicalDevice physicalDevice);
    void createDescriptorPool();
    void createDescriptorSets();
    void createDescriptorSetLayout();
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
         quadVertexBufferMemory.fill(VK_NULL_HANDLE);
         quadStagingBufferMemory.fill(VK_NULL_HANDLE);
         descriptorSetLayout = VK_NULL_HANDLE;
         descriptorPool = VK_NULL_HANDLE;
         descriptorSets.fill(VK_NULL_HANDLE);
    }
    ResourceManager(VkDevice device)
    {
        this->device = device;
        uniformBuffers.fill(VK_NULL_HANDLE);
        uniformBuffersMemory.fill(VK_NULL_HANDLE);
        uniformBuffersMapped.fill(nullptr);
        quadVertexBuffers.fill(VK_NULL_HANDLE);
        quadStagingBuffers.fill(VK_NULL_HANDLE);
        quadVertexBufferMemory.fill(VK_NULL_HANDLE);
        quadStagingBufferMemory.fill(VK_NULL_HANDLE);
        descriptorSetLayout = VK_NULL_HANDLE;
        descriptorPool = VK_NULL_HANDLE;
        descriptorSets.fill(VK_NULL_HANDLE);
    }
    ~ResourceManager() {
        if (device == VK_NULL_HANDLE) return;

        // Уничтожаем пул и layout
        if (descriptorPool != VK_NULL_HANDLE)
            vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        if (descriptorSetLayout != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

        // Уничтожаем буферы UBO
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (uniformBuffers[i] != VK_NULL_HANDLE) {
                vkDestroyBuffer(device, uniformBuffers[i], nullptr);
                vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
            }
            // Уничтожаем буферы квадов (и staging)
            if (quadVertexBuffers[i] != VK_NULL_HANDLE) {
                vkDestroyBuffer(device, quadVertexBuffers[i], nullptr);
                vkFreeMemory(device, quadVertexBufferMemory[i], nullptr);
                vkDestroyBuffer(device, quadStagingBuffers[i], nullptr);
                vkFreeMemory(device, quadStagingBufferMemory[i], nullptr);
            }
        }
    }
};