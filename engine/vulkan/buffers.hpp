/**
 * @author: Dima Galkin
 * @version: 1.0
 *
 * A collection of helper classes and functions for the Vulkan SDK for managing buffers.
*/

#pragma once

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <vulkan/vulkan.hpp>

/**
 * @breif Helper functions to create a vk::CommandBuffer.
 *
 * CommandBuffer provides static methods to create and end a command buffer.
*/
class CommandBuffer {
    public:
        static vk::CommandBuffer begin (
            vk::Device device,
            vk::CommandPool command_pool
        );

        static void end (
            vk::Device device,
            vk::CommandBuffer command_buffer,
            vk::CommandPool command_pool,
            vk::Queue graphics_queue
        );
};

/**
 * @breif Helper class for vulkan image allocation
 *
 * Loads a stbi_uc* into an image and creates an image view for it. Helper methods can be subsequently used to create a
 * sampler and bind them to a given command buffer.
*/
class Image {
    public:
        Image() = default;

        Image (
            const stbi_uc* image,
            uint32_t width, uint32_t height,
            vk::Device device,
            vk::CommandPool command_pool,
            vk::Queue graphics_queue,
            vk::PhysicalDevice p_device
        );

        static void setImageLayout (
            vk::Image image,
            vk::Format format,
            vk::ImageLayout old_layout,
            vk::ImageLayout new_layout,
            vk::Device device,
            vk::CommandPool command_pool,
            vk::Queue graphics_queue
        );

        void loadImage (
            const stbi_uc* image,
            uint32_t width,
            uint32_t height,
            vk::Device device,
            vk::CommandPool command_pool,
            vk::Queue graphics_queue,
            vk::PhysicalDevice p_device
        );


        void updateDescriptor (
            vk::DescriptorSetLayout layout,
            vk::DescriptorPool descriptor_pool
        );

        void render(
            vk::CommandBuffer command_buffer,
            vk::PipelineLayout pipeline_layout
        ) const;

        void createSampler();

        ~Image();
    private:
        vk::Device device_;
        vk::PhysicalDevice physical_device_;
        vk::Image image_;
        vk::ImageView image_view_;
        vk::DeviceMemory image_memory_;
        vk::Sampler sampler_;
        vk::DescriptorSet descriptor_set_;
};

/**
 * @breif Class that encapsulates vk::Buffer and vk::DeviceMemory.
 *
 * MemoryBuffer allocates a vk::Buffer and vk::DeviceMemory. It provides methods to copy data from another MemoryBuffer,
 * set data from a void*.
*/
class MemoryBuffer {
    public:
        MemoryBuffer() = default;

        MemoryBuffer(
            const vk::DeviceSize size,
            const vk::BufferUsageFlags usage,
            const vk::MemoryPropertyFlags properties,
            const vk::Device device,
            const vk::Queue graphics_queue,
            const vk::CommandPool command_pool,
            const vk::PhysicalDevice p_device
        ) : device_ { device },
            physical_device_ { p_device },
            graphics_queue_ { graphics_queue },
            command_pool_ { command_pool }
        {
            createBuffer(size, usage, properties);
        }

        [[nodiscard]] vk::Buffer getBuffer() const { return buffer_; }
        [[nodiscard]] vk::DeviceMemory getMemory() const { return memory_; }

        void set(
            const void* data,
            vk::DeviceSize buffer_size
        ) const;

        void copy (
            const MemoryBuffer& source,
            vk::DeviceSize size
        ) const;

        static uint32_t findMemoryType(
            vk::PhysicalDevice p_device,
            uint32_t filter,
            const vk::MemoryPropertyFlags& properties
        );

        static void bufferAsImage(
            vk::Image image,
            uint32_t width, uint32_t height,
            vk::Device device,
            vk::Queue graphics_queue,
            vk::CommandPool command_pool,
            vk::Buffer buffer
        );

        void asImage(
            const vk::Image image,
            const uint32_t width, const uint32_t height
        ) const {
            bufferAsImage(
                image,
                width, height,
                device_,
                graphics_queue_,
                command_pool_,
                buffer_
            );
        }

        ~MemoryBuffer() {
            device_.destroyBuffer(buffer_);
            device_.freeMemory(memory_);
        }

    private:
        vk::Device device_;
        vk::PhysicalDevice physical_device_;
        vk::Queue graphics_queue_;
        vk::CommandPool command_pool_;
        vk::Buffer buffer_;
        vk::DeviceMemory memory_;

        void createBuffer(
            vk::DeviceSize size,
            const vk::BufferUsageFlags& usage,
            const vk::MemoryPropertyFlags& properties
        );
};