#pragma once
#include "Config.h"

class VulkanContext
{
public:
	const std::vector<const char*> validationLayers = //Возможно подключить слои, которые будут  логировать все вызовы Vulkan API,
		//захватывать кадры для последующего воспроизведения
	{ "VK_LAYER_KHRONOS_validation","VK_LAYER_LUNARG_monitor" };
	const std::vector<const char*> deviceExtensions =
	{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	const bool enableValidationLayers = true;
	bool enableMSAA = false;
	VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;

	VkInstance instance;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	// TODO (При необходимости):Добавить VkPhysicalDeviceFeatures, заполнить его при 
	// создании физического устройства и использовать для проверки доступности фич
	VkSurfaceKHR surface;
	VkDevice device;
	QueueFamilyIndices queueFamilyIndices;
	VkQueue graphicsQueue;
	VkQueue presentQueue;
	VkQueue computeQueue;
	VkQueue transferQueue;

	bool MSAAEnabled() { return enableMSAA && (msaaSamples != VK_SAMPLE_COUNT_1_BIT); }
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice physicalDevice);
	void createInstance();
	bool checkValidationLayerSupport();
	std::vector<const char*> getRequiredExtensions();
	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
	void pickPhysicalDevice();
	int chooseFromListedPhysicalDevices(std::vector<VkPhysicalDevice>& devices);
	bool checkDeviceExtensionSupport(VkPhysicalDevice device);
	void createSurface(GLFWwindow* window);
	void createLogicalDevice();
	VkSampleCountFlagBits getMaxUsableSampleCount();

	~VulkanContext() {
		if (device != VK_NULL_HANDLE)
			vkDestroyDevice(device, nullptr);
		if (surface != VK_NULL_HANDLE)
			vkDestroySurfaceKHR(instance, surface, nullptr);
		if (instance != VK_NULL_HANDLE)
			vkDestroyInstance(instance, nullptr);
	}
};

