#include "vulkan-utils.hpp"

std::vector<const char*> VulkanUtils::getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    return {
        glfw_extensions,
        glfw_extensions + glfwExtensionCount
    };
}

std::vector<char> VulkanUtils::readFile(const std::string& filename) {
    std::ifstream file {filename, std::ios::ate | std::ios::binary};

    if (!file.is_open()) throw std::runtime_error("failed to open file!");

    const size_t fileSize = file.tellg();
    std::vector<char> buffer (fileSize);

    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));

    file.close();

    return buffer;
}

bool VulkanUtils::checkDeviceExtensionSupport(const vk::PhysicalDevice& device) {
    std::set<std::string> required {DeviceExtensions.begin(), DeviceExtensions.end()};

    for (const auto& extension : device.enumerateDeviceExtensionProperties()) {
        required.erase(extension.extensionName);
    }

    return required.empty();
}

vk::SurfaceFormatKHR VulkanUtils::chooseFormat(const std::vector<vk::SurfaceFormatKHR>& available_formats) {
    if (
        available_formats.size() == 1 &&
        available_formats[0].format == vk::Format::eUndefined
    ) return { vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear };

    for (const auto& available : available_formats) {
        if (
            available.format == vk::Format::eB8G8R8A8Unorm &&
            available.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear
        ) return available;
    }

    return available_formats[0];
}

vk::PresentModeKHR VulkanUtils::chooseMode(const std::vector<vk::PresentModeKHR>& available_modes) {
    auto best = vk::PresentModeKHR::eFifo;

    for (const auto& available_mode : available_modes) {
        if (available_mode == vk::PresentModeKHR::eMailbox) {
            return available_mode;
        }

        if (available_mode == vk::PresentModeKHR::eImmediate) {
            best = available_mode;
        }
    }

    return best;
}