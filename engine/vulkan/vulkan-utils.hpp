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
#include <array>
#include <memory>
#include <optional>
#include <set>
#include <string>

#include "buffers.hpp"
#include "../objects.hpp"

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
    glm::mat4 model;
};

inline std::vector<Vertex> verts_lst = OBJLoader::load("../MiG35.obj");

class RendererInfo {
    public:
        explicit RendererInfo(GLFWwindow* window) : window_ {window} {};

        // 720p default
        int width_ = 1280;
        int height_ = 720;

        std::string title_ = "ThreeDL App"; // Default title

        GLFWwindow* window_ = nullptr;
};

class VulkanUtils {
    public:
        static std::vector<const char*> getRequiredExtensions();
        static std::vector<char> readFile(const std::string& filename);
        static bool checkDeviceExtensionSupport(const vk::PhysicalDevice& device);
        static vk::SurfaceFormatKHR chooseFormat(const std::vector<vk::SurfaceFormatKHR>& available_formats);
        static vk::PresentModeKHR chooseMode(const std::vector<vk::PresentModeKHR>& available_modes);
};

template <typename T> class vlkn {
    public:
        explicit vlkn<T>(GLFWwindow* window)
            : info_ {window},
              format_ {vk::Format::eUndefined}
        {
            glfwSetWindowUserPointer(info_.window_, this);
            glfwSetFramebufferSizeCallback(info_.window_, onResize);

            initVulkan();
        };

        static void onResize(GLFWwindow* window, const int width, const int height) {
            const auto instance = static_cast<vlkn*>(glfwGetWindowUserPointer(window));

            instance->info_.width_ = width;
            instance->info_.height_ = height;
            instance->resized_ = true;
        }

        void newFrame() {
            newUBO();

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

        ~vlkn() {
            cleanup();
            delete mem_vert_;
        }

    private:
        RendererInfo info_;

        MemoryBuffer<T>* mem_vert_;
        std::vector<MemoryBuffer<UniformBufferObject>*> uniform_buffers_;

        vk::SurfaceKHR surface_;
        vk::SwapchainKHR swapchain_;

        vk::UniqueInstance instance_;
        vk::PhysicalDevice physical_device_;
        std::shared_ptr<vk::Device> device_;

        vk::Queue graphics_queue_;
        vk::Queue present_queue_;

        std::vector<vk::Image> images_;
        std::vector<vk::ImageView> image_views_;
        std::vector<vk::Framebuffer> framebuffers_;
        std::vector<vk::CommandBuffer> command_buffers_;
        std::vector<vk::Semaphore> image_available_;
        std::vector<vk::Semaphore> render_finished_;
        std::vector<vk::Fence> fences_;
        std::vector<vk::DescriptorSet> descriptor_sets_;

        vk::Format format_;
        vk::Extent2D extent_;

        vk::RenderPass render_pass_;
        vk::Pipeline graphics_pipeline_;

        vk::DescriptorSetLayout ubo_layout_;
        vk::PipelineLayout pipeline_layout_;

        vk::CommandPool command_pool_;
        vk::DescriptorPool descriptor_pool_;

        size_t current_frame_ = 0;
        bool resized_ = false;
        int max_f_frames_ = 2;
        int vertex_count_ = 0;

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

            device_->destroyCommandPool(command_pool_);
            device_->destroyDescriptorSetLayout(ubo_layout_);
            device_->destroyDescriptorPool(descriptor_pool_);

            for (size_t i = 0; i < max_f_frames_; ++i) {
                device_->destroyFence(fences_[i]);
                device_->destroySemaphore(render_finished_[i]);
                device_->destroySemaphore(image_available_[i]);

                delete uniform_buffers_[i];
            }

            instance_->destroySurfaceKHR(surface_);

            glfwDestroyWindow(info_.window_);
            glfwTerminate();
        }

        void recreateSwapChain() {
            info_.width_ = 0; info_.height_ = 0;
            while (info_.width_ == 0 || info_.height_ == 0) {
                glfwGetFramebufferSize(info_.window_, &info_.width_, &info_.height_);
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
                info_.title_.c_str(),
                VK_MAKE_VERSION(0, 0, 0),
                "ThreeDL Engine",
                VK_MAKE_VERSION(0, 0, 0),
                VK_API_VERSION_1_0
            };

            const std::vector<const char*> extensions = VulkanUtils::getRequiredExtensions();
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
            if (glfwCreateWindowSurface(instance_.get(), info_.window_, nullptr, &surface) != VK_SUCCESS) {
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
                device_ = std::make_shared<vk::Device>(physical_device_.createDevice(device_info));
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
            const vk::SurfaceFormatKHR surfaceFormat = VulkanUtils::chooseFormat(sc_support.formats);
            const vk::PresentModeKHR presentMode = VulkanUtils::chooseMode(sc_support.presentModes);
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

                const vk::Buffer buffers[] = { mem_vert_->getBuffer() };
                static constexpr vk::DeviceSize offsets[] = { 0 };

                command_buffers_[i].beginRenderPass(pass_info, vk::SubpassContents::eInline);
                command_buffers_[i].bindPipeline(vk::PipelineBindPoint::eGraphics, graphics_pipeline_);
                command_buffers_[i].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout_, 0, 1, &descriptor_sets_[current_frame_], 0, nullptr);
                command_buffers_[i].bindVertexBuffers(0, 1, buffers, offsets);
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

        [[nodiscard]] vk::UniqueShaderModule createShaderModule(const std::vector<char>& code) const {
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

        [[nodiscard]] vk::Extent2D chooseExtent(const vk::SurfaceCapabilitiesKHR& capabilities) const  {
            if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
                return capabilities.currentExtent;
            }

            int width, height;
            glfwGetFramebufferSize(info_.window_, &width, &height);

            vk::Extent2D extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };

            extent.width = std::max(
                capabilities.minImageExtent.width,
                std::min(capabilities.maxImageExtent.width, extent.width)
            );
            extent.height = std::max(
                capabilities.minImageExtent.height,
                std::min(capabilities.maxImageExtent.height, extent.height)
            );

            return extent;
        }

        [[nodiscard]] SwapChainSupportDetails querySwapChainSupport(const vk::PhysicalDevice& device) const  {
            return {
                device.getSurfaceCapabilitiesKHR(surface_),
                device.getSurfaceFormatsKHR(surface_),
                device.getSurfacePresentModesKHR(surface_)
            };
        }

        [[nodiscard]] bool isDeviceSuitable(const vk::PhysicalDevice& device) const {
            const auto [graphics, present] = findQueueFamilies(device);
            const bool supported_extensions = VulkanUtils::checkDeviceExtensionSupport(device);
            bool swapChainAdequate = false;

            if (supported_extensions) {
                const SwapChainSupportDetails sc_support = querySwapChainSupport(device);
                swapChainAdequate = !sc_support.formats.empty() && !sc_support.presentModes.empty();
            }

            return graphics.has_value() && present.has_value() && supported_extensions && swapChainAdequate;
        }

        [[nodiscard]] QueueFamilyIndices findQueueFamilies(const vk::PhysicalDevice& device) const {
            QueueFamilyIndices indices;

            std::vector<vk::QueueFamilyProperties> families = device.getQueueFamilyProperties();

            int i = 0;
            for (const auto& queueFamily : families) {
                if (
                    queueFamily.queueCount > 0 &&
                    queueFamily.queueFlags & vk::QueueFlagBits::eGraphics
                ) indices.graphicsFamily = i;

                if (
                    queueFamily.queueCount > 0 &&
                    device.getSurfaceSupportKHR(i, surface_)
                ) indices.presentFamily = i;

                if (
                    indices.graphicsFamily.has_value() &&
                    indices.presentFamily.has_value()
                ) break;

                ++i;
            }

            return indices;
        }

        void initVulkan() {
            createInstance();
            createSurface();
            pickPhysicalDevice();
            createLogicalDevice();
            createSwapChain();
            createImageViews();
            createRenderPass();
            createDescriptorSetLayout();
            createGraphicsPipeline();
            createFramebuffers();
            createCommandPool();
            setRenderList(verts_lst);
            createUniformBuffers();
            createDescriptorPool();
            createDescriptorSets();
            createCommandBuffers();
            createSyncObjects();
        };

        void createGraphicsPipeline() {
            const vk::UniqueShaderModule vert = createShaderModule(VulkanUtils::readFile("../shaders/vert.spv"));
            const vk::UniqueShaderModule frag = createShaderModule(VulkanUtils::readFile("../shaders/frag.spv"));

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
                vk::PolygonMode::eLine,
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

            const vk::PipelineLayoutCreateInfo pipe_info = {
                {},
                1,
                &ubo_layout_
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

        void setRenderList(std::vector<T> vertices) {
             const vk::DeviceSize buffer_size = sizeof(vertices[0]) * vertices.size();
             vertex_count_ = vertices.size();

             MemoryBuffer<T> buff (
                buffer_size,
                vk::BufferUsageFlagBits::eTransferSrc,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                device_,
                &graphics_queue_,
                &command_pool_,
                &physical_device_
             );

             buff.set(vertices, buffer_size);

             mem_vert_ = new MemoryBuffer<T> (
                buffer_size,
                vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
                vk::MemoryPropertyFlagBits::eDeviceLocal,
                device_,
                &graphics_queue_,
                &command_pool_,
                &physical_device_
             );

             mem_vert_->copy(buff, buffer_size);
        };

        void createDescriptorSetLayout() {
            static constexpr vk::DescriptorSetLayoutBinding ubo_binding {
                0,
                vk::DescriptorType::eUniformBuffer,
                1,
                vk::ShaderStageFlagBits::eVertex,
                nullptr
            };

            static constexpr vk::DescriptorSetLayoutCreateInfo layout_info = {
                {},
                1,
                &ubo_binding
            };

            if (
                device_->createDescriptorSetLayout(
                    &layout_info,
                    nullptr,
                    &ubo_layout_
                ) != vk::Result::eSuccess
            ) throw std::runtime_error("ERR 14");
        }

        void createUniformBuffers() {
            static constexpr vk::DeviceSize buffer_size = sizeof(UniformBufferObject);

            uniform_buffers_.resize(max_f_frames_);

            for (size_t i = 0; i < max_f_frames_; ++i) {
                uniform_buffers_[i] = new MemoryBuffer<UniformBufferObject> (
                    buffer_size,
                    vk::BufferUsageFlagBits::eUniformBuffer,
                    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                    device_,
                    &graphics_queue_,
                    &command_pool_,
                    &physical_device_
                );
            }
        }

        void createDescriptorPool() {
            const vk::DescriptorPoolSize pool_size = {
                vk::DescriptorType::eUniformBuffer,
                static_cast<uint32_t>(max_f_frames_)
            };

            const vk::DescriptorPoolCreateInfo pool_info = {
                {},
                static_cast<uint32_t>(max_f_frames_),
                1,
                &pool_size
            };

            if (
                device_->createDescriptorPool(
                    &pool_info,
                    nullptr,
                    &descriptor_pool_
                ) != vk::Result::eSuccess
            ) throw std::runtime_error("ERR 13");
        }

        void createDescriptorSets() {
            std::vector<vk::DescriptorSetLayout> layouts(max_f_frames_, ubo_layout_);

            const vk::DescriptorSetAllocateInfo alloc_info = {
                descriptor_pool_,
                static_cast<uint32_t>(max_f_frames_),
                layouts.data()
            };

            descriptor_sets_.resize(max_f_frames_);

            if (
                device_->allocateDescriptorSets(
                    &alloc_info,
                    descriptor_sets_.data()
                ) != vk::Result::eSuccess
            ) throw std::runtime_error("ERR 14");

            for (size_t i = 0; i < max_f_frames_; i++) {
                vk::DescriptorBufferInfo buffer_info = {
                    uniform_buffers_[i]->getBuffer(),
                    0,
                    sizeof(UniformBufferObject)
                };

                vk::WriteDescriptorSet descriptor_write = {
                    descriptor_sets_[i],
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

        void newUBO() const {
            UniformBufferObject ubo = {};

            float aspect = extent_.width / static_cast<float>(extent_.height);
            float fovy = glm::radians(75.0f);
            float n = 0.01f;
            float f = 10000.0f;

            ubo.proj = glm::perspective(fovy, aspect, n, f);

            uniform_buffers_[current_frame_]->set({ ubo }, sizeof(ubo));
        }
};