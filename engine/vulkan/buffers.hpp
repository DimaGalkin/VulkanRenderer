#pragma once

/**
 * @author: Dima Galkin
 * @version: 1.0
 *
 * A collection of helper classes and functions for the Vulkan SDK for managing buffers.
*/

#include <vulkan/vulkan.hpp>

namespace tdl {
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
     * @breif Class that encapsulates vk::Buffer and vk::DeviceMemory.
     *
     * MemoryBuffer allocates a vk::Buffer and vk::DeviceMemory. It provides methods to copy data from another
     * MemoryBuffer, set data from a void*.
    */
    class MemoryBuffer {
        public:
            MemoryBuffer() = default;

            MemoryBuffer (
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

            void set (
                const void* data,
                vk::DeviceSize buffer_size
            ) const;

            void copy (
                const MemoryBuffer& source,
                vk::DeviceSize size
            ) const;

            static uint32_t findMemoryType (
                vk::PhysicalDevice p_device,
                uint32_t filter,
                const vk::MemoryPropertyFlags& properties
            );

            static void bufferAsImage (
                vk::Image image,
                uint32_t width, uint32_t height,
                vk::Device device,
                vk::Queue graphics_queue,
                vk::CommandPool command_pool,
                vk::Buffer buffer
            );

            void asImage (
                const vk::Image image,
                const uint32_t width,
                const uint32_t height
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

            void createBuffer (
                vk::DeviceSize size,
                const vk::BufferUsageFlags& usage,
                const vk::MemoryPropertyFlags& properties
            );
    };

    /**
     * @breif Helper class for vulkan image allocation
     *
     * Loads a unsigned char* into an image and creates an image view for it. Helper methods can be subsequently used
     * to create a sampler and bind them to a given command buffer.
    */
    class Image {
        public:
            Image() = default;

            Image (
                const unsigned char* image,
                uint32_t width,
                uint32_t height,
                vk::Device device,
                vk::CommandPool command_pool,
                vk::Queue graphics_queue,
                vk::PhysicalDevice p_device,
                vk::Sampler sampler
            );

            static void setImageLayout (
                vk::Image image,
                vk::ImageLayout old_layout,
                vk::ImageLayout new_layout,
                vk::Device device,
                vk::CommandPool command_pool,
                vk::Queue graphics_queue
            );

            void loadImage (
                const unsigned char* image,
                uint32_t width,
                uint32_t height,
                vk::Device device,
                vk::CommandPool command_pool,
                vk::Queue graphics_queue,
                vk::PhysicalDevice p_device
            );

            void createDescriptor (
                vk::DescriptorSetLayout layout,
                vk::DescriptorPool descriptor_pool
            );

            void recreateImageView (
                vk::Device device
            );

            void updateDescriptor() const;

            void render (
                vk::CommandBuffer command_buffer,
                vk::PipelineLayout pipeline_layout
            ) const;

            void setSampler(
                const vk::Sampler sampler
            ) { sampler_ = sampler; };

            void setDevice (
                vk::Device device
            );

            ~Image();

            MemoryBuffer* buffer_ = nullptr;
            vk::Image image_;
            vk::ImageView image_view_;

        private:
            vk::Device device_;
            vk::PhysicalDevice physical_device_;
            vk::DeviceMemory image_memory_;
            vk::Sampler sampler_;
            vk::DescriptorSet descriptor_set_;
    };
};