#include "lighting.hpp"

tdl::AmbientLight::AmbientLight(
    const glm::vec4& color,
    const float intensity,
    const LightingModels model
) : LightInterface(
    color, intensity, model
) {}

void tdl::LightHelper::render(
    const std::vector<vk::DescriptorSet>& descriptor_sets,
    const vk::CommandBuffer command_buffer,
    const vk::PipelineLayout pipeline_layout,
    const unsigned long cframe
) {
    command_buffer.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        pipeline_layout,
        2,
        1,
        &descriptor_sets[cframe],
        0,
        nullptr
    );
}

void tdl::LightHelper::createDescriptorSets(
    std::vector<vk::DescriptorSet>& descriptor_sets,
    const std::vector<MemoryBuffer*>& ubos,
    const unsigned int max_f_frames,
    const vk::DescriptorPool descriptor_pool,
    const vk::Device device,
    const vk::DescriptorSetLayout ubo_layout
) {
    std::vector<vk::DescriptorSetLayout> layouts (max_f_frames, ubo_layout);

    const vk::DescriptorSetAllocateInfo alloc_info {
        descriptor_pool,
        static_cast<uint32_t>(max_f_frames),
        layouts.data()
    };

    descriptor_sets.resize(max_f_frames);

    if (
        device.allocateDescriptorSets(
            &alloc_info,
            descriptor_sets.data()
        ) != vk::Result::eSuccess
    ) throw std::runtime_error("ERR 14 ");

    for (int i = 0; i < max_f_frames; ++i) {
        const vk::DescriptorBufferInfo buffer_info {
            ubos[i]->getBuffer(),
            0,
            sizeof(LightObject)
        };

        const vk::WriteDescriptorSet descriptor_write {
            descriptor_sets[i],
            5,
            0,
            1,
            vk::DescriptorType::eUniformBuffer,
            nullptr,
            &buffer_info,
            nullptr
        };

        device.updateDescriptorSets(1, &descriptor_write, 0, nullptr);
    }
}

void tdl::LightHelper::initUBOs(
    std::vector<MemoryBuffer*>& ubos,
    const unsigned int max_f_frames,
    const vk::Device device,
    const vk::Queue graphics_queue,
    const vk::CommandPool command_pool,
    const vk::PhysicalDevice physical_device
) {
    ubos.resize(max_f_frames);

    for (unsigned int i = 0; i < max_f_frames; ++i) {
        ubos[i] = new MemoryBuffer (
            sizeof(LightObject),
            vk::BufferUsageFlagBits::eUniformBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            device,
            graphics_queue,
            command_pool,
            physical_device
        );
    }
}
