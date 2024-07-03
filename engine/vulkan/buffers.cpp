#include "buffers.hpp"

vk::CommandBuffer CommandBuffer::begin(
    const vk::Device& device,
    const vk::CommandPool& command_pool
) {
    vk::CommandBuffer command_buffer;

    try {
        command_buffer = device.allocateCommandBuffers({
            command_pool,
            vk::CommandBufferLevel::ePrimary,
            1
        })[0];
    } catch (const vk::SystemError& err) {
        throw std::runtime_error(
            "ERR 002: Failed to allocate command buffer. CommandBuffer::begin(...)\n"
            + err.code().message()
        );
    }

    command_buffer.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    return command_buffer;
}

void CommandBuffer::end(
    const vk::Device& device,
    const vk::CommandBuffer& command_buffer,
    const vk::CommandPool& command_pool,
    const vk::Queue& graphics_queue
) {
    try {
        command_buffer.end();
    } catch (const vk::SystemError& err) {
        throw std::runtime_error(
            "ERR 006: Failed to end command buffer. CommandBuffer::end(...)\n"
            + err.code().message()
        );
    }

    const vk::SubmitInfo submit_info = {
        0,
        nullptr,
        nullptr,
        1,
        &command_buffer
    };

    graphics_queue.submit(submit_info, nullptr);
    graphics_queue.waitIdle();
    device.freeCommandBuffers(command_pool, command_buffer);
}