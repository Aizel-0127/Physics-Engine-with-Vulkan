#pragma once

// Отключаем макросы Windows, которые ломают std::min/max
#define NOMINMAX 
#define WIN32_LEAN_AND_MEAN 
#define GLFW_INCLUDE_VULKAN
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <vector>
#include <array>
#include <optional>
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <thread>
#include <atomic>
#include <cassert>
#include <set>
#include <algorithm> // Necessary for std::clamp
#include <string>

#include <windows.h>
// Tracy
#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>
// Обязательно определите этот макрос ДО подключения glfw3native.h
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
static constexpr uint32_t MAX_CIRCLES = 100;

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    std::optional<uint32_t> computeFamily;
    std::optional<uint32_t> transferFamily;

    bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value() &&
            computeFamily.has_value() && transferFamily.has_value();
    }
};