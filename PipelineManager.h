#pragma once
#include "Config.h"
#include "PipelineConfigs.h"

class PipelineManager
{
private:
    VkDevice device = VK_NULL_HANDLE;
public:
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkPipeline circlePipeline = VK_NULL_HANDLE;
	VkPipeline quadPipeline = VK_NULL_HANDLE;

	void createPipelineLayout(VkDescriptorSetLayout descriptorSetLayout);
	VkPipeline createGraphicsPipeline(const BasePipelineConfig& config, VkPipelineLayout layout, VkFormat colorFormat, VkFormat depthFormat, VkSampleCountFlagBits samples);
	VkShaderModule createShaderModule(const std::vector<char>& code);
	VkShaderStageFlagBits stageFromExtension(const std::string& path);

    static std::vector<char> readFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("failed to open file!");
        }
        size_t fileSize = (size_t)file.tellg();
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();
        return buffer;
    }
    void setDevice(VkDevice device) { this->device = device; }

    PipelineManager() {};
    PipelineManager(VkDevice device) { this->device = device; }
    ~PipelineManager() {
        if (device == VK_NULL_HANDLE) return;

        if (circlePipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device, circlePipeline, nullptr);
        if (quadPipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device, quadPipeline, nullptr);
        if (pipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    }
};

