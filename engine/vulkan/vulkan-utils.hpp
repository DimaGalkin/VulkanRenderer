#pragma once

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <array>
#include <optional>
#include <set>
#include <string>

const std::vector<const char*> DeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
};

struct SwapChainSupportDetails {
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> presentModes;
};

struct UniformBufferObject {
    glm::mat4 proj;
};

class Vertex {
    public:
        Vertex(const glm::vec3& pos, const glm::vec3& color) : pos {pos}, color {color} {}

        glm::vec3 pos;
        glm::vec3 color;

        static vk::VertexInputBindingDescription getBindingDescription() {
            vk::VertexInputBindingDescription bindingDescription = {};
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(Vertex);
            bindingDescription.inputRate = vk::VertexInputRate::eVertex;

            return bindingDescription;
        }

        static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions() {
            std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions = {};
            attributeDescriptions[0].binding = 0;
            attributeDescriptions[0].location = 0;
            attributeDescriptions[0].format = vk::Format::eR32G32Sfloat;
            attributeDescriptions[0].offset = offsetof(Vertex, pos);

            attributeDescriptions[1].binding = 0;
            attributeDescriptions[1].location = 1;
            attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
            attributeDescriptions[1].offset = offsetof(Vertex, color);

            return attributeDescriptions;
        }
};

const std::vector<Vertex> verts_lst = {
    {{0.0f, 0.0f, -5.0f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
    {{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
    {{0.0f, 0.5f, -0.0f}, {1.0f, 0.0f, 0.0f}},
    {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}}
};

template <typename T> class vlkn {
    public:
        void run() {
            initWindow();
            initVulkan();
            mainLoop();
            cleanup();
        }

    private:
        GLFWwindow* window;
        std::string title_ = "ThreeDL App";
        int width_ = 800;
        int height_ = 600;

        vk::UniqueInstance instance_;
        vk::SurfaceKHR surface_;
        vk::PhysicalDevice physical_device_;
        vk::UniqueDevice device_;
        vk::Queue graphics_queue_;
        vk::Queue present_queue_;
        vk::SwapchainKHR swapchain_;
        std::vector<vk::Image> images_;
        vk::Format format_;
        vk::Extent2D extent_;
        std::vector<vk::ImageView> image_views_;
        std::vector<vk::Framebuffer> framebuffers_;
        vk::RenderPass render_pass_;
        vk::PipelineLayout pipeline_layout_;
        vk::Pipeline graphics_pipeline_;
        vk::CommandPool command_pool_;
        vk::Buffer vertex_buffer_;
        vk::DeviceMemory vertex_buffer_memory_;
        std::vector<vk::CommandBuffer> command_buffers_;
        std::vector<vk::Semaphore> image_available_;
        std::vector<vk::Semaphore> render_finished_;
        std::vector<vk::Fence> fences_;
        size_t current_frame_ = 0;
        bool resized_ = false;
        int max_f_frames_ = 2;
        int vertex_count_ = 0;

        void initWindow() {
            glfwInit();
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // no opengl

            window = glfwCreateWindow(width_, height_, title_.c_str(), nullptr, nullptr);

            glfwSetWindowUserPointer(window, this);
            glfwSetFramebufferSizeCallback(window, onResize);
        }

        static void onResize(GLFWwindow* window, const int width, const int height) {
            const auto instance = reinterpret_cast<vlkn*>(glfwGetWindowUserPointer(window));

            instance->width_ = width;
            instance->height_ = height;
            instance->resized_ = true;
        }

        void mainLoop() {
            while (!glfwWindowShouldClose(window)) {
                glfwPollEvents();
                drawFrame();
            }

            device_->waitIdle();
        }

        void cleanupSwapChain() {
            for (const auto& framebuffer : framebuffers_) device_->destroyFramebuffer(framebuffer);
            for (const auto& image_view : image_views_) device_->destroyImageView(image_view);

            device_->destroySwapchainKHR(swapchain_);
            device_->freeCommandBuffers(command_pool_, command_buffers_);
            device_->destroyPipeline(graphics_pipeline_);
            device_->destroyPipelineLayout(pipeline_layout_);
            device_->destroyRenderPass(render_pass_);
        }

        void cleanup() {
            cleanupSwapChain();

            device_->destroyBuffer(vertex_buffer_);
            device_->destroyCommandPool(command_pool_);
            device_->freeMemory(vertex_buffer_memory_);

            device_->destroyDescriptorPool(descriptorPool);
            device_->destroyPipelineLayout(pipelineLayout);
            device_->destroyDescriptorSetLayout(descriptorSetLayout);

            for (size_t i = 0; i < max_f_frames_; ++i) {
                device_->destroyFence(fences_[i]);
                device_->destroySemaphore(render_finished_[i]);
                device_->destroySemaphore(image_available_[i]);

                device_->unmapMemory(uniformBuffersMemory[i]);
                device_->destroyBuffer(uniformBuffers[i]);
                device_->freeMemory(uniformBuffersMemory[i]);
            }

            instance_->destroySurfaceKHR(surface_);

            device_->destroyDescriptorSetLayout(descriptorSetLayout);

            glfwDestroyWindow(window);
            glfwTerminate();
        }

        void recreateSwapChain() {
            width_ = 0; height_ = 0;
            while (width_ == 0 || height_ == 0) {
                glfwGetFramebufferSize(window, &width_, &height_);
                glfwWaitEvents();
            }
            device_->waitIdle();

            cleanupSwapChain();
            createSwapChain();
            createImageViews();
            createRenderPass();
            createGraphicsPipeline();
            createFramebuffers();
            createCommandBuffers();
        }

        void createInstance() {
            const vk::ApplicationInfo application_info = {
                title_.c_str(),
                VK_MAKE_VERSION(0, 0, 0),
                "ThreeDL Engine",
                VK_MAKE_VERSION(0, 0, 0),
                VK_API_VERSION_1_0
            };

            const std::vector<const char*> extensions = getRequiredExtensions();
            const vk::InstanceCreateInfo instance_info = {
                {},
                &application_info,
                0, nullptr,
                static_cast<uint32_t>(extensions.size()), extensions.data()
            };

            try {
                instance_ = vk::createInstanceUnique(instance_info, nullptr);
            } catch (const vk::SystemError& err) {
                throw std::runtime_error("ERR 1");
            }
        }

        void createSurface() {
            VkSurfaceKHR surface;
            if (glfwCreateWindowSurface(instance_.get(), window, nullptr, &surface) != VK_SUCCESS) {
                throw std::runtime_error("ERR 2");
            }

            surface_ = surface;
        }

        void pickPhysicalDevice() {
            std::vector<vk::PhysicalDevice> devices = instance_->enumeratePhysicalDevices();
            if (devices.empty()) throw std::runtime_error("ERR 3");

            for (const auto& device : devices) {
                if (isDeviceSuitable(device)) { physical_device_ = device; break; }
            }
            if (!physical_device_) throw std::runtime_error("ERR 4");
        }

        void createLogicalDevice() {
            const auto [graphics, present] = findQueueFamilies(physical_device_);

            std::vector<vk::DeviceQueueCreateInfo> info_group;
            const std::set<uint32_t> unique = { graphics.value(), present.value() };

            float priority = 1.0f;
            for (const uint32_t family : unique) {
                info_group.emplace_back( vk::DeviceQueueCreateFlags {}, family, 1, &priority);
            }

            static constexpr vk::PhysicalDeviceFeatures features = {};
            const vk::DeviceCreateInfo device_info = {
                {},
                static_cast<uint32_t>(info_group.size()), info_group.data(),
                0, nullptr,
                static_cast<uint32_t>(DeviceExtensions.size()), DeviceExtensions.data(),
                &features
            };

            try {
                device_ = physical_device_.createDeviceUnique(device_info);
            }
            catch (const vk::SystemError& err) {
                throw std::runtime_error("ERR 5");
            }

            graphics_queue_ = device_->getQueue(graphics.value(), 0);
            present_queue_ = device_->getQueue(present.value(), 0);
        }

        void createSwapChain() {
            const SwapChainSupportDetails sc_support = {
                physical_device_.getSurfaceCapabilitiesKHR(surface_),
                physical_device_.getSurfaceFormatsKHR(surface_),
                physical_device_.getSurfacePresentModesKHR(surface_)
            };
            const vk::SurfaceFormatKHR surfaceFormat = chooseFormat(sc_support.formats);
            const vk::PresentModeKHR presentMode = chooseMode(sc_support.presentModes);
            const vk::Extent2D extent = chooseExtent(sc_support.capabilities);

            uint32_t imageCount = sc_support.capabilities.minImageCount + 1;
            if (
                sc_support.capabilities.maxImageCount > 0 &&
                imageCount > sc_support.capabilities.maxImageCount
            ) { imageCount = sc_support.capabilities.maxImageCount; }

            vk::SwapchainCreateInfoKHR info {
                {},
                surface_,
                imageCount,
                surfaceFormat.format,
                surfaceFormat.colorSpace,
                extent,
                1,
                vk::ImageUsageFlagBits::eColorAttachment
            };

            const auto [graphics, present] = findQueueFamilies(physical_device_);
            const uint32_t queueFamilyIndices[] = { graphics.value(), present.value() };

            if (graphics != present) {
                info.imageSharingMode = vk::SharingMode::eConcurrent;
                info.queueFamilyIndexCount = 2;
                info.pQueueFamilyIndices = queueFamilyIndices;
            } else {
                info.imageSharingMode = vk::SharingMode::eExclusive;
            }
            info.presentMode = presentMode;
            info.clipped = VK_TRUE;
            info.preTransform = sc_support.capabilities.currentTransform;
            info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
            info.oldSwapchain = vk::SwapchainKHR {nullptr};

            try {
                swapchain_ = device_->createSwapchainKHR(info);
            } catch (const vk::SystemError& err) {
                throw std::runtime_error("ERR 6");
            }

            images_ = device_->getSwapchainImagesKHR(swapchain_);
            extent_ = extent;
            format_ = surfaceFormat.format;
        }

    void createImageViews() {
            image_views_.resize(images_.size());

            for (size_t i = 0; i < images_.size(); i++) {
                try {
                    image_views_[i] = device_->createImageView({
                            {},
                            images_[i],
                            vk::ImageViewType::e2D,
                            format_,
                            {},
                            {
                                vk::ImageAspectFlagBits::eColor,
                                0,
                                1,
                                0,
                                1
                            }
                        }
                    );
                } catch (const vk::SystemError& err) {
                    throw std::runtime_error("failed to create image views!");
                }
            }
        }

        void createRenderPass() {
            const vk::AttachmentDescription color {
                {},
                format_,
                vk::SampleCountFlagBits::e1,
                vk::AttachmentLoadOp::eClear,
                vk::AttachmentStoreOp::eStore,
                vk::AttachmentLoadOp::eDontCare,
                vk::AttachmentStoreOp::eDontCare,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::ePresentSrcKHR
            };

            static constexpr vk::AttachmentReference color_ref {
                0,
                vk::ImageLayout::eColorAttachmentOptimal
            };
            static constexpr vk::SubpassDescription subpass {
                {},
                vk::PipelineBindPoint::eGraphics,
                0, nullptr,
                1, &color_ref
            };
            static constexpr vk::SubpassDependency dependency {
                VK_SUBPASS_EXTERNAL,
                0,
                vk::PipelineStageFlagBits::eColorAttachmentOutput,
                vk::PipelineStageFlagBits::eColorAttachmentOutput,
                {},
                vk::AccessFlagBits::eColorAttachmentWrite
            };

            try {
                render_pass_ = device_->createRenderPass({
                    {},
                    1,
                    &color,
                    1,
                    &subpass,
                    1,
                    &dependency
                });
            } catch (const vk::SystemError& e) {
                throw std::runtime_error("ERR 8");
            }
        }

        void createFramebuffers() {
            framebuffers_.resize(image_views_.size());

            for (size_t i = 0; i < image_views_.size(); i++) {
                const vk::ImageView attachments[] = { image_views_[i] };

                vk::FramebufferCreateInfo framebufferInfo = {
                    {},
                    render_pass_,
                    1,
                    attachments,
                    extent_.width,
                    extent_.height,
                    1
                };

                try {
                    framebuffers_[i] = device_->createFramebuffer(framebufferInfo);
                } catch (const vk::SystemError& err) {
                    throw std::runtime_error("ERR 11");
                }
            }
        }

        void createCommandPool() {
            const auto [graphics, _] = findQueueFamilies(physical_device_);
            const vk::CommandPoolCreateInfo pool_info = {
                {},
                graphics.value()
            };

            try {
                command_pool_ = device_->createCommandPool(pool_info);
            } catch (const vk::SystemError& err) {
                throw std::runtime_error("ERR 12");
            }
        }

        void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Buffer& buffer, vk::DeviceMemory& bufferMemory) {
            vk::BufferCreateInfo bufferInfo = {};
            bufferInfo.size = size;
            bufferInfo.usage = usage;
            bufferInfo.sharingMode = vk::SharingMode::eExclusive;

            try {
                buffer = device_->createBuffer(bufferInfo);
            }
            catch (const vk::SystemError& err) {
                throw std::runtime_error("ERR 13");
            }

            vk::MemoryRequirements memRequirements = device_->getBufferMemoryRequirements(buffer);

            vk::MemoryAllocateInfo allocInfo = {};
            allocInfo.allocationSize = memRequirements.size;
            allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

            try {
                bufferMemory = device_->allocateMemory(allocInfo);
            }
            catch (const vk::SystemError& err) {
                throw std::runtime_error("ERR 14");
            }

            device_->bindBufferMemory(buffer, bufferMemory, 0);
        }

        void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
            vk::CommandBufferAllocateInfo allocInfo = {};
            allocInfo.level = vk::CommandBufferLevel::ePrimary;
            allocInfo.commandPool = command_pool_;
            allocInfo.commandBufferCount = 1;

            vk::CommandBuffer commandBuffer = device_->allocateCommandBuffers(allocInfo)[0];

            vk::CommandBufferBeginInfo beginInfo = {};
            beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

            commandBuffer.begin(beginInfo);

            vk::BufferCopy copyRegion = {};
            copyRegion.srcOffset = 0; // Optional
            copyRegion.dstOffset = 0; // Optional
            copyRegion.size = size;
            commandBuffer.copyBuffer(srcBuffer, dstBuffer, copyRegion);

            commandBuffer.end();

            vk::SubmitInfo submitInfo = {};
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;

            graphics_queue_.submit(submitInfo, nullptr);
            graphics_queue_.waitIdle();

            device_->freeCommandBuffers(command_pool_, commandBuffer);
        }

        uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
            vk::PhysicalDeviceMemoryProperties memProperties = physical_device_.getMemoryProperties();

            for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
                if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                    return i;
                }
            }

            throw std::runtime_error("ERR 15");
        }

        void createCommandBuffers() {
            command_buffers_.resize(framebuffers_.size());

            const vk::CommandBufferAllocateInfo alloc_info = {
                command_pool_,
                vk::CommandBufferLevel::ePrimary,
                static_cast<uint32_t>(command_buffers_.size())
            };

            try {
                command_buffers_ = device_->allocateCommandBuffers(alloc_info);
            } catch (const vk::SystemError& err) {
                throw std::runtime_error("ERR 16");
            }

            for (size_t i = 0; i < command_buffers_.size(); i++) {
                static constexpr vk::CommandBufferBeginInfo begin_info {
                    vk::CommandBufferUsageFlagBits::eSimultaneousUse
                };

                try {
                    command_buffers_[i].begin(begin_info);
                } catch (const vk::SystemError& err) {
                    throw std::runtime_error("ERR 17");
                }

                vk::ClearValue clearColor = { std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f } };

                const vk::RenderPassBeginInfo pass_info = {
                        render_pass_,
                        framebuffers_[i],
                        {
                        {0, 0},
                        extent_
                    },
                    1,
                    &clearColor
                };

                vk::Buffer buffers[] = { vertex_buffer_ };
                vk::DeviceSize offsets[] = { 0 };

                command_buffers_[i].beginRenderPass(pass_info, vk::SubpassContents::eInline);
                command_buffers_[i].bindPipeline(vk::PipelineBindPoint::eGraphics, graphics_pipeline_);
                command_buffers_[i].bindVertexBuffers(0, 1, buffers, offsets);
                command_buffers_[i].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, 1, &descriptorSets[current_frame_], 0, nullptr);
                command_buffers_[i].draw(static_cast<uint32_t>(vertex_count_), 1, 0, 0);
                command_buffers_[i].endRenderPass();

                try {
                    command_buffers_[i].end();
                } catch (const vk::SystemError& err) {
                    throw std::runtime_error("ERR 18");
                }
            }
        }

        void createSyncObjects() {
            image_available_.resize(max_f_frames_);
            render_finished_.resize(max_f_frames_);
            fences_.resize(max_f_frames_);

            try {
                for (size_t i = 0; i < max_f_frames_; i++) {
                    fences_[i] = device_->createFence({vk::FenceCreateFlagBits::eSignaled});
                    image_available_[i] = device_->createSemaphore({});
                    render_finished_[i] = device_->createSemaphore({});
                }
            } catch (const vk::SystemError& err) {
                throw std::runtime_error("ERR 19");
            }
        }

        void drawFrame() {
            updateUBO();

            if (
                device_->waitForFences(
                    1, &fences_[current_frame_],
                    VK_TRUE, std::numeric_limits<uint64_t>::max()
                ) != vk::Result::eSuccess
            ) throw std::runtime_error("ERR 20");

            uint32_t idx;
            try {
                const vk::ResultValue result = device_->acquireNextImageKHR(
                    swapchain_,
                    std::numeric_limits<uint64_t>::max(),
                    image_available_[current_frame_], nullptr
                );
                idx = result.value;
            } catch (const vk::OutOfDateKHRError& err) {
                recreateSwapChain();
                return;
            } catch (const vk::SystemError& err) {
                throw std::runtime_error("ERR 21");
            }

            vk::PipelineStageFlags waitStages = vk::PipelineStageFlagBits::eColorAttachmentOutput;
            vk::SubmitInfo submit_info = {
                1,
                &image_available_[current_frame_],
                &waitStages,
                1,
                &command_buffers_[idx],
                1,
                &render_finished_[current_frame_]
            };

            if (
                device_->resetFences(
                    1,
                    &fences_[current_frame_]
                ) != vk::Result::eSuccess
            ) throw std::runtime_error("ERR 22");

            try {
                graphics_queue_.submit(submit_info, fences_[current_frame_]);
            } catch (const vk::SystemError& err) {
                throw std::runtime_error("ERR 23");
            }

            const vk::PresentInfoKHR present_info {
                1,
                &render_finished_[current_frame_],
                1,
                &swapchain_,
                &idx
            };

            vk::Result r_present;
            try {
                r_present = present_queue_.presentKHR(present_info);
            } catch (const vk::OutOfDateKHRError& err) {
                r_present = vk::Result::eErrorOutOfDateKHR;
            } catch (const vk::SystemError& err) {
                throw std::runtime_error("ERR 24");
            }

            if (r_present == vk::Result::eSuboptimalKHR || resized_) {
                resized_ = false;
                recreateSwapChain();
                return;
            }

            current_frame_ = (current_frame_ + 1) % max_f_frames_;
        }

        vk::UniqueShaderModule createShaderModule(const std::vector<char>& code) {
            try {
                return device_->createShaderModuleUnique({
                    {},
                    code.size(),
                    reinterpret_cast<const uint32_t*>(code.data())
                });
            } catch (const vk::SystemError& err) {
                throw std::runtime_error("ERR 25");
            }
        }

        vk::SurfaceFormatKHR chooseFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
            if (availableFormats.size() == 1 && availableFormats[0].format == vk::Format::eUndefined) {
                return { vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear };
            }

            for (const auto& availableFormat : availableFormats) {
                if (availableFormat.format == vk::Format::eB8G8R8A8Unorm && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
                    return availableFormat;
                }
            }

            return availableFormats[0];
        }

        vk::PresentModeKHR chooseMode(const std::vector<vk::PresentModeKHR> availablePresentModes) {
            vk::PresentModeKHR bestMode = vk::PresentModeKHR::eFifo;

            for (const auto& availablePresentMode : availablePresentModes) {
                if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
                    return availablePresentMode;
                }
                else if (availablePresentMode == vk::PresentModeKHR::eImmediate) {
                    bestMode = availablePresentMode;
                }
            }

            return bestMode;
        }

        vk::Extent2D chooseExtent(const vk::SurfaceCapabilitiesKHR& capabilities) {
            if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
                return capabilities.currentExtent;
            }
            else {
                int width, height;
                glfwGetFramebufferSize(window, &width, &height);

                vk::Extent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };

                actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
                actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

                return actualExtent;
            }
        }

        SwapChainSupportDetails querySwapChainSupport(const vk::PhysicalDevice& device) {
            SwapChainSupportDetails details;
            details.capabilities = device.getSurfaceCapabilitiesKHR(surface_);
            details.formats = device.getSurfaceFormatsKHR(surface_);
            details.presentModes = device.getSurfacePresentModesKHR(surface_);

            return details;
        }

        bool isDeviceSuitable(const vk::PhysicalDevice& device) {
            QueueFamilyIndices indices = findQueueFamilies(device);

            bool extensionsSupported = checkDeviceExtensionSupport(device);

            bool swapChainAdequate = false;
            if (extensionsSupported) {
                SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
                swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
            }

            return indices.graphicsFamily.has_value() && indices.presentFamily.has_value() && extensionsSupported && swapChainAdequate;
        }

        bool checkDeviceExtensionSupport(const vk::PhysicalDevice& device) {
            std::set<std::string> requiredExtensions(DeviceExtensions.begin(), DeviceExtensions.end());

            for (const auto& extension : device.enumerateDeviceExtensionProperties()) {
                requiredExtensions.erase(extension.extensionName);
            }

            return requiredExtensions.empty();
        }

        QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device) {
            QueueFamilyIndices indices;

            auto queueFamilies = device.getQueueFamilyProperties();

            int i = 0;
            for (const auto& queueFamily : queueFamilies) {
                if (queueFamily.queueCount > 0 && queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) {
                    indices.graphicsFamily = i;
                }

                if (queueFamily.queueCount > 0 && device.getSurfaceSupportKHR(i, surface_)) {
                    indices.presentFamily = i;
                }

                if (indices.graphicsFamily.has_value() && indices.presentFamily.has_value()) {
                    break;
                }

                i++;
            }

            return indices;
        }

        std::vector<const char*> getRequiredExtensions() {
            uint32_t glfwExtensionCount = 0;
            const char** glfwExtensions;
            glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

            std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

            return extensions;
        }

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

        void initVulkan() {
            createInstance();
            createSurface();
            pickPhysicalDevice();
            createLogicalDevice();
            createSwapChain();
            createImageViews();
            createRenderPass();
            createGraphicsPipeline();
            createFramebuffers();
            createCommandPool();
            createVertexBuffer(verts_lst);
            setUBOLayout();
            memUBO();
            createDescriptorPool();
            createPoolsets();
            createCommandBuffers();
            createSyncObjects();
        };

        void createGraphicsPipeline() {
            const vk::UniqueShaderModule vert = createShaderModule(readFile("../shaders/vert.spv"));
            const vk::UniqueShaderModule frag = createShaderModule(readFile("../shaders/frag.spv"));

            const vk::PipelineShaderStageCreateInfo stages[] = {
                {
                    {},
                    vk::ShaderStageFlagBits::eVertex,
                    vert.get(),
                    "main"
                },
                {
                    {},
                    vk::ShaderStageFlagBits::eFragment,
                    frag.get(),
                    "main"
                }
            };

            const vk::VertexInputBindingDescription binding = T::getBindingDescription();
            const std::array<vk::VertexInputAttributeDescription, 2> attributes = T::getAttributeDescriptions();

            const vk::PipelineVertexInputStateCreateInfo vertex_info {
                {},
                1,
                &binding,
                static_cast<uint32_t>(attributes.size()),
                attributes.data()
            };

            static constexpr vk::PipelineInputAssemblyStateCreateInfo assembly {
                {},
                vk::PrimitiveTopology::eTriangleList,
                VK_FALSE
            };

            const vk::Viewport viewport = {
                0.0f, 0.0f,
                static_cast<float>(extent_.width),
                static_cast<float>(extent_.height),
                0.0f, 1.0f
            };

            const vk::Rect2D scissor = {
                {0, 0},
                extent_
            };

            vk::PipelineViewportStateCreateInfo viewport_info = {
                {},
                1,
                &viewport,
                1,
                &scissor
            };

            static constexpr vk::PipelineRasterizationStateCreateInfo rasterizer {
                {},
                VK_FALSE,
                VK_FALSE,
                vk::PolygonMode::eFill,
                vk::CullModeFlagBits::eBack,
                vk::FrontFace::eClockwise,
                VK_FALSE,
                0.0f,
                0.0f,
                0.0f,
                1.0f
            };

            static constexpr vk::PipelineMultisampleStateCreateInfo multisampling {
                {},
                vk::SampleCountFlagBits::e1,
                VK_FALSE,
                1.0f,
                nullptr,
                VK_FALSE,
                VK_FALSE
            };

            static constexpr vk::PipelineColorBlendAttachmentState blending_attach {
                VK_FALSE,
                vk::BlendFactor::eOne,
                vk::BlendFactor::eZero,
                vk::BlendOp::eAdd,
                vk::BlendFactor::eOne,
                vk::BlendFactor::eZero,
                vk::BlendOp::eAdd,
                vk::ColorComponentFlags(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA)
            };

            static constexpr vk::PipelineColorBlendStateCreateInfo blending = {
                {},
                VK_FALSE,
                vk::LogicOp::eCopy,
                1,
                &blending_attach,
                {0.0f, 0.0f, 0.0f, 0.0f}
            };

            static constexpr vk::PipelineLayoutCreateInfo pipe_info = {
                {},
                0, nullptr,
                0, nullptr
            };

            try {
                pipeline_layout_ = device_->createPipelineLayout(pipe_info);
            } catch (const vk::SystemError& err) {
                throw std::runtime_error("ERR 9");
            }

            const vk::GraphicsPipelineCreateInfo info {
                {},
                2,
                stages,
                &vertex_info,
                &assembly,
                nullptr,
                &viewport_info,
                &rasterizer,
                &multisampling,
                nullptr,
                &blending,
                nullptr,
                pipeline_layout_,
                render_pass_,
                0,
                nullptr,
                -1
            };

            try {
                graphics_pipeline_ = device_->createGraphicsPipeline(nullptr, info).value;
            } catch (const vk::SystemError& err) {
                throw std::runtime_error("ERR 10");
            }
        };

        void createVertexBuffer(std::vector<T> vertices) {
            const vk::DeviceSize buffer_size = sizeof(vertices[0]) * vertices.size();
            vertex_count_ = vertices.size();

            vk::Buffer buffer;
            vk::DeviceMemory buffer_mem;
            createBuffer(
                buffer_size,
                vk::BufferUsageFlagBits::eTransferSrc,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                buffer,
                buffer_mem
            );

            void* data = device_->mapMemory(buffer_mem, 0, buffer_size);
            memcpy(data, vertices.data(), buffer_size);
            device_->unmapMemory(buffer_mem);

            createBuffer(
                buffer_size,
                vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
                vk::MemoryPropertyFlagBits::eDeviceLocal,
                vertex_buffer_,
                vertex_buffer_memory_
            );

            copyBuffer(buffer, vertex_buffer_, buffer_size);

            device_->destroyBuffer(buffer);
            device_->freeMemory(buffer_mem);
        };

        vk::DescriptorSetLayout descriptorSetLayout;
        vk::PipelineLayout pipelineLayout;

        void setUBOLayout() {
            vk::DescriptorSetLayoutBinding ubo_layout {
                0,
                vk::DescriptorType::eUniformBuffer,
                1,
                vk::ShaderStageFlagBits::eVertex,
                nullptr
            };

            vk::DescriptorSetLayoutCreateInfo layout_info {
                {},
                1,
                &ubo_layout
            };

            try {
                descriptorSetLayout = device_->createDescriptorSetLayout(layout_info);
            } catch (const vk::SystemError& err) {
                throw std::runtime_error("ERR 26");
            }

            vk::PipelineLayoutCreateInfo pipeline_info {
                {},
                1,
                &descriptorSetLayout
            };

            try {
                pipelineLayout = device_->createPipelineLayout(pipeline_info);
            } catch (const vk::SystemError& err) {
                throw std::runtime_error("ERR 27");
            }
        }

        vk::Buffer indexBuffer;
        vk::DeviceMemory indexBufferMemory;

        std::vector<vk::Buffer> uniformBuffers;
        std::vector<vk::DeviceMemory> uniformBuffersMemory;
        std::vector<void*> uniformBuffersMapped;

        void memUBO() {
            vk::DeviceSize bufferSize = sizeof(UniformBufferObject);

            uniformBuffers.resize(images_.size());
            uniformBuffersMemory.resize(images_.size());
            uniformBuffersMapped.resize(images_.size());

            for (size_t i = 0; i < images_.size(); i++) {
                createBuffer(
                    bufferSize,
                    vk::BufferUsageFlagBits::eUniformBuffer,
                    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                    uniformBuffers[i],
                    uniformBuffersMemory[i]
                );

                uniformBuffersMapped[i] = device_->mapMemory(uniformBuffersMemory[i], 0, bufferSize);
            }
        }

        void updateUBO() {
            UniformBufferObject ubo {};
            ubo.proj = glm::identity<glm::mat4>();

            memcpy(uniformBuffersMapped[current_frame_], &ubo, sizeof(ubo));
        }

        vk::DescriptorPool descriptorPool;
        std::vector<vk::DescriptorSet> descriptorSets;

        void createDescriptorPool() {
            vk::DescriptorPoolSize pool_size {
                vk::DescriptorType::eUniformBuffer,
                static_cast<uint32_t>(max_f_frames_)
            };

            vk::DescriptorPoolCreateInfo pool_info {
                {},
                static_cast<uint32_t>(max_f_frames_),
                1,
                &pool_size
            };

            try {
                descriptorPool = device_->createDescriptorPool(pool_info);
            } catch (const vk::SystemError& err) {
                throw std::runtime_error("ERR 28");
            }
        }

        void createPoolsets() {
            std::vector<vk::DescriptorSetLayout> layouts (max_f_frames_, descriptorSetLayout);

            vk::DescriptorSetAllocateInfo alloc_info {
                descriptorPool,
                static_cast<uint32_t>(max_f_frames_),
                layouts.data()
            };

            descriptorSets.resize(max_f_frames_);

            try {
                descriptorSets = device_->allocateDescriptorSets(alloc_info);
            } catch (const vk::SystemError& err) {
                throw std::runtime_error("ERR 29");
            }

            for (size_t i = 0; i < max_f_frames_; i++) {
                vk::DescriptorBufferInfo buffer_info {
                    uniformBuffers.at(i),
                    0,
                    sizeof(UniformBufferObject)
                };

                vk::WriteDescriptorSet descriptor_write {
                    descriptorSets.at(i),
                    0,
                    0,
                    1,
                    vk::DescriptorType::eUniformBuffer,
                    nullptr,
                    &buffer_info,
                    nullptr
                };

                device_->updateDescriptorSets(1, &descriptor_write, 0, nullptr);
            }
        }
};