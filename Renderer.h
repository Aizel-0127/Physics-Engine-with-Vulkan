#pragma once
#include "Config.h"
class VulkanContext;
class SwapchainManager;
class PipelineManager;
class ResourceManager;
class Scene;

class Renderer
{
private:
    VkDevice device = VK_NULL_HANDLE;
public:
    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> graphicsCommandBuffers;
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> imageAvailableSemaphores;
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> inFlightFences;
    // Пул и буферы для очереди презентации
    VkCommandPool presentCommandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> presentCommandBuffers; // По одному на каждое изображение swapchain

    // Семафоры для синхронизации передачи владения
    std::vector<VkSemaphore> imageOwnershipSemaphores; // По одному на каждое изображение
    uint32_t currentFrame = 0;


    void recordPresentOwnershipRelease(QueueFamilyIndices queueFamilyIndices, SwapchainManager& swapchain, uint32_t imageIndex);
    void createPresentCommandResources(QueueFamilyIndices queueFamilyIndices, SwapchainManager& swapchain);
    void drawFrame(
        VulkanContext& context,
        SwapchainManager& swapchain,
        PipelineManager& pipelines,
        ResourceManager& resources,
        Scene& scene,
        TracyVkCtx tracyVkCtx = nullptr
    );
    void createCommandPool(QueueFamilyIndices queueFamilyIndices);
    void createCommandBuffers();
    void createSyncObjects();
    void setDevice(VkDevice device) { this->device = device; }

    Renderer() 
    {
        graphicsCommandBuffers.fill(VK_NULL_HANDLE);
        imageAvailableSemaphores.fill(VK_NULL_HANDLE);
        inFlightFences.fill(VK_NULL_HANDLE);
    };
    Renderer(VkDevice device) 
    {
        graphicsCommandBuffers.fill(VK_NULL_HANDLE);
        imageAvailableSemaphores.fill(VK_NULL_HANDLE);
        inFlightFences.fill(VK_NULL_HANDLE);
        this->device = device;
    }
    ~Renderer() {
        if (device == VK_NULL_HANDLE) return;

        // Семафоры и fence'ы
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (imageAvailableSemaphores[i] != VK_NULL_HANDLE)
                vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            if (inFlightFences[i] != VK_NULL_HANDLE)
                vkDestroyFence(device, inFlightFences[i], nullptr);
        }
        for (auto sem : imageOwnershipSemaphores) {
            vkDestroySemaphore(device, sem, nullptr);
        }

        // Командные пулы (буферы удалятся автоматически)
        if (commandPool != VK_NULL_HANDLE)
            vkDestroyCommandPool(device, commandPool, nullptr);
        if (presentCommandPool != VK_NULL_HANDLE)
            vkDestroyCommandPool(device, presentCommandPool, nullptr);
    }
};