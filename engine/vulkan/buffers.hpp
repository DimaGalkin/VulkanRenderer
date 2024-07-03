/**
 * @author: Dima Galkin
 * @version: 1.0
 *
 * A collection of helper classes and functions for the Vulkan SDK for managing buffers.
*/

#pragma once

#include <vulkan/vulkan.hpp>
#include <stdexcept>
#include <memory>

/**
 * @breif Helper functions to create a vk::CommandBuffer.
 *
 * CommandBuffer provides static methods to create and end a command buffer.
*/
class CommandBuffer {
    public:
        static vk::CommandBuffer begin(
            const vk::Device& device,
            const vk::CommandPool& command_pool
        );

        static void end(
            const vk::Device& device,
            const vk::CommandBuffer& command_buffer,
            const vk::CommandPool& command_pool,
            const vk::Queue& graphics_queue
        );
};

/**
 * @breif Class that encapsulates vk::Buffer and vk::DeviceMemory.
 *
 * MemoryBuffer allocates a vk::Buffer and vk::DeviceMemory. It provides methods to copy data from another MemoryBuffer,
 * set data from a std::vector<T>.
*/
template <typename T> class MemoryBuffer {
    public:
        MemoryBuffer() = delete;

        MemoryBuffer(
            const vk::DeviceSize& size,
            const vk::BufferUsageFlags& usage,
            const vk::MemoryPropertyFlags& properties,
            const std::shared_ptr<vk::Device>& device,
            vk::Queue* graphics_queue,
            vk::CommandPool* command_pool,
            vk::PhysicalDevice* p_device
        ) : device_ {device},
            physical_device_ {p_device},
            graphics_queue_ {graphics_queue},
            command_pool_ {command_pool}
        {
            createBuffer(size, usage, properties);
        }

        [[nodiscard]] vk::Buffer getBuffer() const { return *buffer_; }
        [[nodiscard]] vk::DeviceMemory getMemory() const { return *memory_; }
        [[nodiscard]] std::unique_ptr<vk::Buffer> extractBuffer() { return std::move(buffer_); }

        /**
         * @breif Copies data from a std::vector<T> to the buffer.
         *
         * Auto maps and unmaps memory needed for the copy. Buffer size must be equal to the vector size * sizeof(T).
        */
        void set(
            const std::vector<T>& data,
            const vk::DeviceSize buffer_size
        ) const {
            try {
                void* mapped = device_->mapMemory(*memory_, 0, buffer_size);
                memcpy(mapped, data.data(), buffer_size);
                device_->unmapMemory(*memory_);
            } catch (const vk::SystemError& err) {
                throw std::runtime_error(
                    "ERR 001: Failed to map memory for buffer. MemoryBuffer::set(...)\n"
                    + err.code().message()
                );
            }
        }

        /**
         * @breif Copies data from another MemoryBuffer to this buffer.
         *
         * Auto allocates a command buffer, copies the data, and frees the command buffer.
        */
        void copy (
            const MemoryBuffer& source,
            const vk::DeviceSize size
        ) const {
            const vk::CommandBuffer command_buffer = CommandBuffer::begin(*device_, *command_pool_);
            const vk::BufferCopy regions = {0, 0, size};

            command_buffer.copyBuffer(
                source.getBuffer(),
                *buffer_,
                regions
            );

            CommandBuffer::end(*device_, command_buffer, *command_pool_, *graphics_queue_);
        }

        /**
         * @breif Finds the memory type that meets the requirements.
         *
         * @param p_device The physical device to check.
         * @param filter The memory type filter.
         * @param properties The memory properties to check.
        */
        static uint32_t findMemoryType(
            const vk::PhysicalDevice& p_device,
            const uint32_t filter,
            const vk::MemoryPropertyFlags& properties
        ) {
            const vk::PhysicalDeviceMemoryProperties memory_properties = p_device.getMemoryProperties();

            for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
                if (
                    (filter & (1 << i)) &&
                    (memory_properties.memoryTypes[i].propertyFlags & properties) == properties
                ) return i;
            }

            throw std::runtime_error("ERR 15");
        }

        ~MemoryBuffer() {
            device_->destroyBuffer(*buffer_);
            device_->freeMemory(*memory_);
        }

    private:
        std::shared_ptr<vk::Device> device_;

        vk::PhysicalDevice* physical_device_ = nullptr;
        vk::Queue* graphics_queue_ = nullptr;
        vk::CommandPool* command_pool_ = nullptr;

        std::unique_ptr<vk::Buffer> buffer_ = nullptr;
        std::unique_ptr<vk::DeviceMemory> memory_ = nullptr;

        /**
         * @breif Creates a vk::Buffer and vk::DeviceMemory.
         *
         * Uses the device to create a buffer and memory. The buffer is bound to the memory.
        */
        void createBuffer(
            const vk::DeviceSize size,
            const vk::BufferUsageFlags& usage,
            const vk::MemoryPropertyFlags& properties
        ) {
            try {
                buffer_ = std::make_unique<vk::Buffer>(device_->createBuffer({
                    {},
                    size,
                    usage,
                    vk::SharingMode::eExclusive
                }));
            } catch (const vk::SystemError& err) {
                throw std::runtime_error(
                    "ERR 004: Failed to create buffer. MemoryBuffer::createBuffer(...)\n"
                    + err.code().message()
                );
            }

            const vk::MemoryRequirements mem_rqs = device_->getBufferMemoryRequirements(*buffer_);
            const vk::MemoryAllocateInfo allocInfo = {
                mem_rqs.size,
                findMemoryType(*physical_device_, mem_rqs.memoryTypeBits, properties)
            };

            try {
                memory_ = std::make_unique<vk::DeviceMemory>(device_->allocateMemory(allocInfo));
            } catch (const vk::SystemError& err) {
                throw std::runtime_error(
                    "ERR 005: Failed to allocate memory for buffer. MemoryBuffer::createBuffer(...)\n"
                    + err.code().message()
                );
            }

            device_->bindBufferMemory(*buffer_, *memory_, 0);
        }
};