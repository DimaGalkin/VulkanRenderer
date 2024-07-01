#pragma once

#include <vulkan/vulkan.hpp>
#include <stdexcept>
#include <memory>

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

        void set(
            const std::vector<T> data,
            const vk::DeviceSize& buffer_size
        ) const  {
            void* mapped = device_->mapMemory(*memory_, 0, buffer_size);
            memcpy(mapped, data.data(), buffer_size);
            device_->unmapMemory(*memory_);
        }

        void copy (
            const MemoryBuffer& source,
            const vk::DeviceSize size
        ) {
            const vk::CommandBuffer command_buffer = device_->allocateCommandBuffers({
                *command_pool_,
                vk::CommandBufferLevel::ePrimary,
                1
            })[0];

            command_buffer.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

            const vk::BufferCopy regions = {
                0,
                0,
                size
            };

            command_buffer.copyBuffer(
                source.getBuffer(),
                *buffer_,
                regions
            );

            command_buffer.end();

            const vk::SubmitInfo submit_info = {
                0,
                nullptr,
                nullptr,
                1,
                &command_buffer
            };

            graphics_queue_->submit(submit_info, nullptr);
            graphics_queue_->waitIdle();
            device_->freeCommandBuffers(*command_pool_, command_buffer);
        }

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

        // using compiler generated will result in 'vk::DeviceLost'
        MemoryBuffer& operator= (const MemoryBuffer& other) {
            device_ = other.device_;
            physical_device_ = other.physical_device_;
            command_pool_ = other.command_pool_;
            graphics_queue_ = other.graphics_queue_;

            return *this;
        }

    private:
        std::shared_ptr<vk::Device> device_;

        vk::PhysicalDevice* physical_device_ = nullptr;
        vk::Queue* graphics_queue_ = nullptr;
        vk::CommandPool* command_pool_ = nullptr;

        std::unique_ptr<vk::Buffer> buffer_;
        std::unique_ptr<vk::DeviceMemory> memory_;

        void createBuffer(
            const vk::DeviceSize& size,
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
                throw std::runtime_error("ERR 13");
            }

            const vk::MemoryRequirements mem_rqs = device_->getBufferMemoryRequirements(*buffer_);
            const vk::MemoryAllocateInfo allocInfo = {
                mem_rqs.size,
                findMemoryType(*physical_device_, mem_rqs.memoryTypeBits, properties)
            };

            try {
                memory_ = std::make_unique<vk::DeviceMemory>(device_->allocateMemory(allocInfo));
            } catch (const vk::SystemError& err) {
                throw std::runtime_error("ERR 14");
            }

            device_->bindBufferMemory(*buffer_, *memory_, 0);
        }
};