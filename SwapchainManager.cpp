#include "SwapchainManager.h"
#include "VulkanContext.h"
#include "Renderer.h"

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        //VK_PRESENT_MODE_MAILBOX_KHR - Означает, что если изображение было отрендерено, но в очереди для изображений нет места, 
        // то старые изображения заменяются новыми
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;    //Единственный гарантированно поддерживаемый формат презентации
}

VkExtent2D SwapchainManager::chooseSwapExtent(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
    VkSurfaceCapabilitiesKHR capabilities_2;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities_2);
    if (capabilities_2.currentExtent.width != std::numeric_limits<uint32_t>::max()) { //специальное значение
        return capabilities_2.currentExtent;
    }
    else {
        VkExtent2D actualExtent = {
            static_cast<uint32_t>(currentWidth),
            static_cast<uint32_t>(currentHeight)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities_2.minImageExtent.width, capabilities_2.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities_2.minImageExtent.height, capabilities_2.maxImageExtent.height);

        return actualExtent;
    }
}

SwapchainManager::SwapChainSupportDetails SwapchainManager::querySwapChainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &details.capabilities);
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, details.formats.data());
    }
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, details.presentModes.data());
    }
    return details;
}

void SwapchainManager::createColorResources(VulkanContext& context)
{
    VkFormat colorFormat = swapChainImageFormat;
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = swapChainExtent.width;
    imageInfo.extent.height = swapChainExtent.height;
    imageInfo.extent.depth = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.mipLevels = 1;
    imageInfo.format = colorFormat;
    imageInfo.samples = context.msaaSamples;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(context.device, &imageInfo, nullptr, &colorImage) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(context.device, colorImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(context.physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(context.device, &allocInfo, nullptr, &colorImageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    //vkBindImageMemory(device, colorImage, colorImageMemory, 0);
    if (vkBindImageMemory(context.device, colorImage, colorImageMemory, 0) != VK_SUCCESS) {
        throw std::runtime_error("failed to bind ImageMemory!");
    }
    colorImageView = createImageView(colorImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
}

void SwapchainManager::createSwapChain(VulkanContext& context)
//Тут происходит выбор формата, режима презентации и разрешения отрендереных изображений
{
    //Заполнение структуры с характеристиками физического устройства (capabilities, форматы показа и режимы презентации)
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(context.physicalDevice,context.surface);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);



    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) { // 0 - специальное значение для отсутствия предела
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainKHR oldSwapchain = swapChain;

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = context.surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    swapChainImageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;

    createInfo.imageArrayLayers = 1;    //The imageArrayLayers specifies the amount of layers each image consists of. This is always 1 unless you are developing a stereoscopic 3D application.
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; //display directly or to a secondary buffer for postprocessing
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;    //Права на изображения должны передаваться вручную
    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;   //Для (не)применения трансформации ко всем изображениям в swapchain'е
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; //if alpha channel should be used for blending with other windows in the window system
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;   //Если необходимо считывать цвет пикселей, которые не видны на экране (например, из-за других окон), то нужно поставить VK_FALSE
    createInfo.oldSwapchain = oldSwapchain;   //Для пересоздания Cwapchain (например, из-за изменения расрешения окна)
    VkSwapchainKHR newSwapchain;
    VkResult result;
    do
    {
        VkExtent2D extent = chooseSwapExtent(context.physicalDevice, context.surface);
        createInfo.imageExtent = extent;
        swapChainExtent = extent;
        result = vkCreateSwapchainKHR(context.device, &createInfo, nullptr, &newSwapchain);
        //Иногда при изменении размера окна появляются ошибки слоя валидации, т.к. размер успел снова измениться перед vkCreateSwapchainKHR
        //Это вроде бы не мешает, поэтомя я оставлю, как есть, но если будет мешать, то можно добавить 
        /*
        VkSwapchainPresentScalingCreateInfoKHR scalingInfo{};
        scalingInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_SCALING_CREATE_INFO_KHR;
        scalingInfo.scalingBehavior = VK_PRESENT_SCALING_ONE_TO_ONE_BIT; // или ASPECT_RATIO_STRETCH
        createInfo.pNext = &scalingInfo;
        */
    } while (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR);
    // 4. Если успех — обновляем член класса
    swapChain = newSwapchain;

    // 5. Уничтожаем старый swapchain, чтобы освободить память
    if (oldSwapchain != VK_NULL_HANDLE)
        vkDestroySwapchainKHR(context.device, oldSwapchain, nullptr);
    vkGetSwapchainImagesKHR(context.device, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(context.device, swapChain, &imageCount, swapChainImages.data());
}

void SwapchainManager::recreateSwapChain(VulkanContext& context, Renderer& renderer) {

    int width = currentWidth, height = currentHeight;
    // 2. Если окно свернуто (размер 0x0), просто выходим.
   // Мы не пересоздаем swapchain, и drawFrame просто ничего не нарисует.
    if (width == 0 || height == 0) {
        return;
    }
    vkDeviceWaitIdle(context.device);
    cleanupSwapChain();

    createSwapChain(context);
    createImageViews();
    if (context.MSAAEnabled()) {
        createColorResources(context);
    }
    // Уничтожаем renderFinishedSemaphores — их количество может измениться
    for (auto sem : renderFinishedSemaphores) {
        vkDestroySemaphore(context.device, sem, nullptr);
    }
    renderFinishedSemaphores.clear();
    renderFinishedSemaphores.resize(swapChainImages.size());
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreInfo.pNext = nullptr;
    semaphoreInfo.flags = 0;
    for (size_t i = 0; i < swapChainImages.size(); i++) {
        if (vkCreateSemaphore(context.device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
    if (context.queueFamilyIndices.graphicsFamily.value() != context.queueFamilyIndices.presentFamily.value())
    {
        vkResetCommandPool(context.device, renderer.presentCommandPool, 0);
        for (size_t i = 0; i < swapChainImages.size(); i++) {
            renderer.recordPresentOwnershipRelease(context.queueFamilyIndices,*this,static_cast<uint32_t>(i));
        }
    }
}

void SwapchainManager::cleanupSwapChain()
{
    if (colorImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, colorImageView, nullptr);
        colorImageView = VK_NULL_HANDLE;
    }
    if (colorImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, colorImage, nullptr);
        colorImage = VK_NULL_HANDLE;
    }
    if (colorImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, colorImageMemory, nullptr);
        colorImageMemory = VK_NULL_HANDLE;
    }

    for (auto imageView : swapChainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }
    swapChainImageViews.clear();
}

VkImageView SwapchainManager::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;  //allows you to treat images as 1D textures, 2D textures, 3D textures and cube maps
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    /*
    If you were working on a stereographic 3D application, then you would create a swap chain with multiple layers.
    You could then create multiple image views for each image representing the views for the left and right eyes by accessing different layers.
    */
    viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;  //example: you can map all of the channels to the red channel for a monochrome texture
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image view!");
    }

    return imageView;
}

void SwapchainManager::createImageViews()
{
    swapChainImageViews.resize(swapChainImages.size());

    for (uint32_t i = 0; i < swapChainImages.size(); i++) {
        swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    }
}

uint32_t SwapchainManager::findMemoryType(VkPhysicalDevice physicalDevice,  uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

void SwapchainManager::createSwapchainSemaphores()
{
    renderFinishedSemaphores.resize(swapChainImages.size());
    graphicsReleaseSemaphores.resize(swapChainImages.size());

    // Описание семафора.
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreInfo.pNext = nullptr;
    semaphoreInfo.flags = 0;

    for (size_t i = 0; i < swapChainImages.size(); i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &graphicsReleaseSemaphores[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
}