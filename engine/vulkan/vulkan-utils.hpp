/**
 * @author: Dima Galkin
 * @version 1.0
 *
 * Contains all helper classes and functions for the Vulkan SDK.
*/

#pragma once

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <optional>
#include <string>

#include "buffers.hpp"
#include "../lighting.hpp"
#include "../objects.hpp"

namespace tdl {
    inline std::vector DEVICE_EXTENSIONS {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    struct GraphicsPresentInfo {
        std::optional<uint32_t> graphics_family;
        std::optional<uint32_t> present_family;
    };

    struct SwapChainSupportDetails {
        vk::SurfaceCapabilitiesKHR capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> present_modes;
    };

    /**
     * @breif: Contains data needed for shader stages.
     *
     * Contains matracies for projection, camera, and rotation. Needed to render objects.
    */
    struct UniformBufferObject {
        glm::mat4 proj;
        glm::mat4 camera;
        glm::mat4 rotation;
    };

    /**
     * @breif: Describes and contains vital information about the renderer.
     *
     * Contains admin data for the Vlkn class.
    */
    class RendererInfo {
        public:
            // 1200p default
            int width_ = 1280;
            int height_ = 720;

            std::string title_ = "ThreeDL App"; // Default title

            GLFWwindow* window_ = nullptr;
    };

    class Vlkn {
        public:
            explicit Vlkn (
                RendererInfo* const renderer_info
            ) : info_ { renderer_info },
                mem_vert_ { nullptr },
                format_ { vk::Format::eUndefined }
            {}

            void init();

            void add (
                const shared_model<Model>& object
            ) { objects_.push_back(object); }

            void add (
                const std::shared_ptr<LightInterface>& light
            ) {
                lights_.push_back(light);
            }

            void newFrame (
                const UniformBufferObject& ubo
            );

            ~Vlkn() {
                cleanup();
                delete mem_vert_;

                for (const auto& u : light_ubos_) delete u;
            }

            RendererInfo* const info_;
            bool resized_ = false;

        private:
            MemoryBuffer* mem_vert_;
            std::vector<MemoryBuffer*> uniform_buffers_;

            std::vector<shared_model<Model>> objects_;
            std::vector<std::shared_ptr<LightInterface>> lights_;

            std::vector<MemoryBuffer*> light_ubos_;
            std::vector<vk::DescriptorSet> light_descriptor_sets_;

            vk::SurfaceKHR surface_;
            vk::SwapchainKHR swapchain_;

            vk::UniqueInstance instance_;

            vk::Sampler sampler_;

            vk::Queue present_queue_;

            vk::PhysicalDevice physical_device_;
            vk::Device device_;
            vk::Queue graphics_queue_;
            vk::CommandPool command_pool_;

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

            vk::Image z_buffer_;
            vk::DeviceMemory z_buffer_memory_;
            vk::ImageView z_buffer_view_;

            vk::RenderPass render_pass_;
            vk::Pipeline graphics_pipeline_;

            vk::DescriptorSetLayout ubo_layout_;
            vk::DescriptorSetLayout object_layout_;
            vk::DescriptorSetLayout model_layout_;
            vk::DescriptorSetLayout texture_layout_;
            vk::DescriptorSetLayout lights_layout_;
            vk::PipelineLayout pipeline_layout_;

            vk::DescriptorPool descriptor_pool_;

            size_t current_frame_ = 0;
            int max_f_frames_ = 2;

            void submitForDraw (
                const vk::CommandBuffer& buffer,
                uint32_t idx
            );

            void cleanupSwapchain();
            void cleanup();
            void recreateSwapchain();
            void createInstance();
            void createSurface();
            void pickPhysicalDevice();
            void createLogicalDevice();
            void createSwapchain();
            void createImageViews();
            void createSampler();
            void createRenderPass();
            void createFramebuffers();
            void createCommandPool();
            void loadModels();
            void startCommandBuffers();
            void createSyncObjects();
            void createGraphicsPipeline();
            void createDescriptorSetLayout();
            void createZBuffer();
            void createUniformBuffers();
            void createDescriptorPool();
            void createDescriptorSets();

            void regenUBOs (
                const UniformBufferObject& ubo
            ) const;

            static std::vector<const char*> getRequiredExtensions();

            static std::vector<char> readFile (
                const std::string& filename
            );

            static bool checkDeviceExtensionSupport (
                const vk::PhysicalDevice& device
            );

            static vk::SurfaceFormatKHR chooseFormat (
                const std::vector<vk::SurfaceFormatKHR>& available_formats
            );

            static vk::PresentModeKHR chooseMode (
                const std::vector<vk::PresentModeKHR>& available_modes
            );

            [[nodiscard]] vk::UniqueShaderModule createShaderModule (
                const std::vector<char>& code
            ) const;

            [[nodiscard]] vk::Extent2D chooseExtent (
                const vk::SurfaceCapabilitiesKHR& capabilities
            ) const;

            [[nodiscard]] SwapChainSupportDetails querySwapChainSupport (
                const vk::PhysicalDevice& device
            ) const;

            [[nodiscard]] bool isDeviceSuitable (
                const vk::PhysicalDevice& device
            ) const;

            [[nodiscard]] GraphicsPresentInfo findQueueFamilies (
                const vk::PhysicalDevice& device
            ) const;
    };
};