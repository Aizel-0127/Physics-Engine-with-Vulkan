#pragma once
#include "Config.h"

class VulkanContext;
class Renderer;

class SwapchainManager
{
private:
    VkDevice device = VK_NULL_HANDLE;
public:
    
    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };
    // Флаг изменения размера должен быть атомарным, так как пишется из 
   // колбэка (основной поток), а читается в потоке рендеринга.
    std::atomic<bool> framebufferResized{ false };
    std::atomic<int> currentWidth{ WIDTH };
    std::atomic<int> currentHeight{ HEIGHT };
	VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    VkImage colorImage = VK_NULL_HANDLE; // мультисэмплированное изображение.
    VkDeviceMemory colorImageMemory = VK_NULL_HANDLE;    //память, привязанная к нему.
    VkImageView colorImageView = VK_NULL_HANDLE; // view для привязки к pipeline'у и командному буферу.

    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkSemaphore> graphicsReleaseSemaphores;

    static uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void createImageViews();
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
    void createColorResources(VulkanContext& context);
    void createSwapChain(VulkanContext& context);
    void recreateSwapChain(VulkanContext& context, Renderer& renderer);
    void cleanupSwapChain();
    VkExtent2D chooseSwapExtent(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
    void createSwapchainSemaphores();
    void setDevice(VkDevice device) { this->device = device; }

    SwapchainManager() {};
    SwapchainManager(VkDevice device){ this->device = device; }
    ~SwapchainManager() {
        if (device == VK_NULL_HANDLE) return;

        // Семафоры
        for (auto sem : renderFinishedSemaphores)
            vkDestroySemaphore(device, sem, nullptr);
        for (auto sem : graphicsReleaseSemaphores)
            vkDestroySemaphore(device, sem, nullptr);

        cleanupSwapChain(); // Удаляет MSAA и ImageViews

        if (swapChain != VK_NULL_HANDLE)
            vkDestroySwapchainKHR(device, swapChain, nullptr);
    }
};

