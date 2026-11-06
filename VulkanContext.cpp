#include "VulkanContext.h"


QueueFamilyIndices VulkanContext::findQueueFamilies(VkPhysicalDevice physicalDevice) {
    QueueFamilyIndices indices;
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    // Helper: семейство поддерживает present?
    auto supportsPresent = [&](uint32_t i) {
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
        return presentSupport == VK_TRUE;
        };

    // 1. Graphics — первое семейство с GRAPHICS_BIT
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
            break;
        }
    }

    // 2. Present — первое семейство, поддерживающее present,
    //    желательно отличное от graphics
    //    Проход 1: ищем отличное от graphics
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (i == indices.graphicsFamily.value_or(UINT32_MAX)) continue;
        if (supportsPresent(i)) {
            indices.presentFamily = i;
            break;
        }
    }
    //    Fallback: если не нашли — берём graphics, если он поддерживает present
    if (!indices.presentFamily.has_value() && indices.graphicsFamily.has_value()) {
        if (supportsPresent(indices.graphicsFamily.value())) {
            indices.presentFamily = indices.graphicsFamily;
        }
    }

    // 3. Compute — предпочитаем отличное от graphics и present
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (i == indices.graphicsFamily.value_or(UINT32_MAX)) continue;
        if (i == indices.presentFamily.value_or(UINT32_MAX)) continue;
        if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            indices.computeFamily = i;
            break;
        }
    }
    //    Fallback: если не нашли — берём graphics или present, если поддерживают compute
    if (!indices.computeFamily.has_value()) {
        for (uint32_t candidate : { indices.graphicsFamily.value_or(UINT32_MAX),
            indices.presentFamily.value_or(UINT32_MAX) }) {
            if (candidate != UINT32_MAX &&
                (queueFamilies[candidate].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
                indices.computeFamily = candidate;
                break;
            }
        }
    }

    // 4. Transfer — аналогично
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (i == indices.graphicsFamily.value_or(UINT32_MAX)) continue;
        if (i == indices.presentFamily.value_or(UINT32_MAX)) continue;
        if (i == indices.computeFamily.value_or(UINT32_MAX)) continue;
        if (queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
            indices.transferFamily = i;
            break;
        }
    }
    //    Fallback: graphics, present или compute, если поддерживают transfer
    if (!indices.transferFamily.has_value()) {
        for (uint32_t candidate : { indices.graphicsFamily.value_or(UINT32_MAX),
            indices.computeFamily.value_or(UINT32_MAX),
            indices.presentFamily.value_or(UINT32_MAX) }) {
            if (candidate != UINT32_MAX &&
                (queueFamilies[candidate].queueFlags & VK_QUEUE_TRANSFER_BIT)) {
                indices.transferFamily = candidate;
                break;
            }
        }
    }

    return indices;
}

void VulkanContext::createInstance()
{
    if (enableValidationLayers && !checkValidationLayerSupport()) {
        throw std::runtime_error("validation layers requested, but not available!");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "GameA";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    auto extensions = getRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    }
    else {
        createInfo.enabledLayerCount = 0;

        createInfo.pNext = nullptr;
    }

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("failed to create instance!");
    }
}

bool VulkanContext::checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

std::vector<const char*> VulkanContext::getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

void VulkanContext::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity =
        //VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT;
        //VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType =
        //VK_DEBUG_UTILS_MESSAGE_TYPE_FLAG_BITS_MAX_ENUM_EXT;
        //VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContext::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    /*
    pCallbackData — указатель на структуру, содержащую само сообщение и дополнительные данные (Если понадобятся) :
        pMessage — текст сообщения(строка с описанием ошибки / предупреждения).
        pMessageIdName — строковый идентификатор сообщения.
        messageIdNumber — числовой идентификатор.
        pObjects — массив объектов Vulkan, связанных с сообщением(полезно для отладки).
        objectCount — количество объектов в массиве.
        pQueueLabels и pCmdBufLabels — информация о метках очередей и командных буферов.
    */
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}

void VulkanContext::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);

    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
    int index = chooseFromListedPhysicalDevices(devices);
    assert(index >= 0 && "Chosen wrong physical device in function");
    physicalDevice = devices[index];
    if (enableMSAA)
        msaaSamples = getMaxUsableSampleCount();
    if (physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

int VulkanContext::chooseFromListedPhysicalDevices(std::vector<VkPhysicalDevice>& devices)
{
    std::vector<uint32_t> indicesOfUnusableDevices;
    for (int i = 0; i < devices.size(); i++)
    {
        if (!checkDeviceExtensionSupport(devices[i]))
        {
            indicesOfUnusableDevices.push_back(i);
            continue;
        }
        QueueFamilyIndices indices = findQueueFamilies(devices[i]);
        if (!indices.isComplete())
        {
            indicesOfUnusableDevices.push_back(i);
            continue;
        }
        VkPhysicalDeviceProperties deviceProperties;
        //VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceProperties(devices[i], &deviceProperties);
        //vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
        std::cout << "Device " << i << ": " << deviceProperties.deviceName << std::endl;
    }
    int choise = -1;
    bool validChoise = true;
    return 1;
   /* if (devices.size() >= indicesOfUnusableDevices.size())
        do
        {
            validChoise = true;
            std::cout << "Input chosen device's index: ";
            std::cin >> choise;
            for (int illegalChoise : indicesOfUnusableDevices)
            {
                if (choise == illegalChoise)
                    validChoise = false;
            }
        } while (choise < 0 || choise >= devices.size() || !validChoise);*/
    /*if (choise >= 0 || choise < devices.size())
    {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(devices[choise], &deviceProperties);
        std::cout << "You have chosen device: " << deviceProperties.deviceName << std::endl;
    }*/
    //return choise;
}

bool VulkanContext::checkDeviceExtensionSupport(VkPhysicalDevice physicalDevice) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

void VulkanContext::createSurface(GLFWwindow* window) {
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }
}

void VulkanContext::createLogicalDevice()
{
    //Заполняет queueFamilyIndices индексами семейств очередей. По возможности выбирает разные индексы для разных семейств
    queueFamilyIndices = findQueueFamilies(physicalDevice);
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    //Оставляем только уникальные элементы
    std::set<uint32_t> uniqueQueueFamilies = { queueFamilyIndices.graphicsFamily.value(), queueFamilyIndices.presentFamily.value(),
        queueFamilyIndices.computeFamily.value(), queueFamilyIndices.transferFamily.value() };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }
    VkPhysicalDeviceFeatures deviceFeatures{};
    if (enableMSAA)
        deviceFeatures.samplerAnisotropy = VK_TRUE;
    else
        deviceFeatures.samplerAnisotropy = VK_FALSE;
    deviceFeatures.sampleRateShading = VK_FALSE; // enable sample shading feature for the device

    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = VK_TRUE;

    VkPhysicalDeviceHostQueryResetFeatures hostQueryResetFeatures{};
    hostQueryResetFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES;
    hostQueryResetFeatures.hostQueryReset = VK_TRUE;
    hostQueryResetFeatures.pNext = &features13;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();
    createInfo.enabledLayerCount = 0; //always 0 - now a legacy feature
    createInfo.pNext = &hostQueryResetFeatures;  // цепочка расширений
    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device!");
    }
    vkGetDeviceQueue(device, queueFamilyIndices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, queueFamilyIndices.presentFamily.value(), 0, &presentQueue);
    vkGetDeviceQueue(device, queueFamilyIndices.computeFamily.value(), 0, &computeQueue);
    vkGetDeviceQueue(device, queueFamilyIndices.transferFamily.value(), 0, &transferQueue);
}

VkSampleCountFlagBits VulkanContext::getMaxUsableSampleCount() {
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

    VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
    if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
    if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
    if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
    if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
    if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

    return VK_SAMPLE_COUNT_1_BIT;
}