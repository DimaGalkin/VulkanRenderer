#include "buffers.hpp"

vk::CommandBuffer CommandBuffer::begin(
    const vk::Device device,
    const vk::CommandPool command_pool
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
            "ERR 001: Failed to allocate command buffer. CommandBuffer::begin(...)\n"
            + err.code().message()
        );
    }

    command_buffer.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    return command_buffer;
}

void CommandBuffer::end(
    const vk::Device device,
    const vk::CommandBuffer command_buffer,
    const vk::CommandPool command_pool,
    const vk::Queue graphics_queue
) {
    try {
        command_buffer.end();
    } catch (const vk::SystemError& err) {
        throw std::runtime_error(
            "ERR 002: Failed to end command buffer. CommandBuffer::end(...)\n"
            + err.code().message()
        );
    }

    const vk::SubmitInfo submit_info {
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

Image::Image(
    const unsigned char* const image,
    const uint32_t width,
    const uint32_t height,
    const vk::Device device,
    const vk::CommandPool command_pool,
    const vk::Queue graphics_queue,
    const vk::PhysicalDevice p_device
) {
    loadImage(
        image,
        width,
        height,
        device,
        command_pool,
        graphics_queue,
        p_device
    );
}

void Image::setImageLayout(
    const vk::Image image,
    const vk::ImageLayout old_layout,
    const vk::ImageLayout new_layout,
    const vk::Device device,
    const vk::CommandPool command_pool,
    const vk::Queue graphics_queue
) {
    const vk::CommandBuffer command_buffer = CommandBuffer::begin(device, command_pool);

    vk::PipelineStageFlags src;
    vk::PipelineStageFlags dst;

    vk::ImageMemoryBarrier barrier {
            {},
            {},
            old_layout,
            new_layout,
            vk::QueueFamilyIgnored,
            vk::QueueFamilyIgnored,
            image,
            vk::ImageSubresourceRange {
                vk::ImageAspectFlagBits::eColor,
                0,
                1,
                0,
                1
            }
    };

    // auto detect pipeline flags & access masks
    if (
        old_layout == vk::ImageLayout::eUndefined &&
        new_layout == vk::ImageLayout::eTransferDstOptimal
    ) {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        src = vk::PipelineStageFlagBits::eTopOfPipe;
        dst = vk::PipelineStageFlagBits::eTransfer;
    } else if (
        old_layout == vk::ImageLayout::eTransferDstOptimal &&
        new_layout == vk::ImageLayout::eShaderReadOnlyOptimal
    ) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        src = vk::PipelineStageFlagBits::eTransfer;
        dst = vk::PipelineStageFlagBits::eFragmentShader;
    } else {
        throw std::invalid_argument("ERR 003: Unsupported layout transition! Image::setImageLayout(...)\n");
    }

    command_buffer.pipelineBarrier(
        src,
        dst,
        {},
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier
    );

    CommandBuffer::end(device, command_buffer, command_pool, graphics_queue);
}

void Image::loadImage(
    const unsigned char* const image,
    const uint32_t width,
    const uint32_t height,
    const vk::Device device,
    const vk::CommandPool command_pool,
    const vk::Queue graphics_queue,
    const vk::PhysicalDevice p_device
) {
    device_ = device;
    physical_device_ = p_device;

    device_.destroyImage(image_); // destroy image incase it has already been allocated to avoid memory leaks

    // * 4 as there are 4 bytes per pixel (RGBA)
    const vk::DeviceSize image_size = width * height * 4;

    // buffer to hold image data
    buffer_ = new MemoryBuffer {
        image_size,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        device,
        graphics_queue,
        command_pool,
        p_device
    };
    buffer_->set(image, image_size); // copy raw data to buffer

    const vk::ImageCreateInfo image_info {
        {},
        vk::ImageType::e2D,
        vk::Format::eR8G8B8A8Srgb,
        vk::Extent3D {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height),
            1
        },
        1,
        1,
        vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        vk::SharingMode::eExclusive,
        0,
        nullptr,
        vk::ImageLayout::eUndefined
    };

    try {
        image_ = device.createImage(image_info);
    } catch (const vk::SystemError& e) {
        throw std::runtime_error(
            "ERR 004: Failed to create image! Image::loadImage(...)"
            + std::string(e.what())
        );
    }

    const vk::MemoryRequirements mem_reqs = device.getImageMemoryRequirements(image_);

    const vk::MemoryAllocateInfo alloc_info {
        image_size,
        MemoryBuffer::findMemoryType(
            p_device,
            mem_reqs.memoryTypeBits,
            vk::MemoryPropertyFlagBits::eDeviceLocal
        )
    };

    if (
        device.allocateMemory(&alloc_info, nullptr, &image_memory_)
        != vk::Result::eSuccess
    ) {
        throw std::runtime_error("ERR 005: Failed to allocate image memory! Image::loadImage(...)");
    }

    image_memory_ = device.allocateMemory(alloc_info);
    device.bindImageMemory(image_, image_memory_, 0);

    setImageLayout(
        image_,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eTransferDstOptimal,
        device,
        command_pool,
        graphics_queue
    );

    buffer_->asImage(image_, width, height);

    setImageLayout(
        image_,
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        device,
        command_pool,
        graphics_queue
    );

    const vk::ImageViewCreateInfo view_info {
            {},
            image_,
            vk::ImageViewType::e2D,
            vk::Format::eR8G8B8A8Srgb,
            {},
            vk::ImageSubresourceRange {
                vk::ImageAspectFlagBits::eColor,
                0,
                1,
                0,
                1
            }
    };

    try {
        image_view_ = device.createImageView(view_info);
    } catch (const vk::SystemError& e) {
        throw std::runtime_error(
            "ERR 006: Failed to create image view! Image::loadImage(...)"
            + std::string(e.what())
        );
    }
}

void Image::createSampler() {
    device_.destroySampler(sampler_);

    vk::PhysicalDeviceProperties properties {};
    physical_device_.getProperties(&properties);

    const vk::SamplerCreateInfo sampler_info {
        {},
        vk::Filter::eLinear,
        vk::Filter::eLinear,
        vk::SamplerMipmapMode::eLinear,
        vk::SamplerAddressMode::eMirrorClampToEdgeKHR,
        vk::SamplerAddressMode::eMirrorClampToEdgeKHR,
        vk::SamplerAddressMode::eMirrorClampToEdgeKHR,
        0.0f,
        properties.limits.maxSamplerAnisotropy > 1.0f,
        properties.limits.maxSamplerAnisotropy,
        VK_FALSE,
        vk::CompareOp::eAlways,
        0.0f,
        0.0f,
        vk::BorderColor::eIntOpaqueBlack,
        VK_FALSE
    };

    try {
        sampler_ = device_.createSampler(sampler_info);
    } catch (const vk::SystemError& e) {
        throw std::runtime_error(
            "ERR 007: Failed to create sampler! Image::createSampler(...)"
            + std::string(e.what())
        );
    }
}

void Image::createDescriptor(
    const vk::DescriptorSetLayout layout,
    const vk::DescriptorPool descriptor_pool
) {
    const vk::DescriptorSetAllocateInfo alloc_info {
        descriptor_pool,
        1,
        &layout
    };

    try {
        descriptor_set_ = device_.allocateDescriptorSets(alloc_info)[0];
    } catch (const vk::SystemError& e) {
        throw std::runtime_error(
            "ERR 008: Failed to allocate descriptor set! Image::createDescriptor(...)"
            + std::string(e.what())
        );
    }
}

void Image::updateDescriptor() const {
    const vk::DescriptorImageInfo image_info {
        sampler_,
        image_view_,
        vk::ImageLayout::eShaderReadOnlyOptimal
    };

   const vk::WriteDescriptorSet descriptor_write {
        descriptor_set_,
        1,
        0,
        1,
        vk::DescriptorType::eCombinedImageSampler,
        &image_info,
        nullptr,
        nullptr
    };

    try {
        device_.updateDescriptorSets(
            1, &descriptor_write,
            0, nullptr
        );
    } catch (const vk::SystemError& e) {
        throw std::runtime_error(
            "ERR 009: Failed to update descriptor set! Image::updateDescriptor(...)"
            + std::string(e.what())
        );
    }
}

void Image::render(
    const vk::CommandBuffer command_buffer,
    const vk::PipelineLayout pipeline_layout
) const {
    command_buffer.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        pipeline_layout,
        1,
        1,
        &descriptor_set_,
        0,
        nullptr
    );
}

void Image::setDevice(
    const vk::Device device
) { device_ = device; }

Image::~Image() {
    delete buffer_;

    try {
        device_.destroyImage(image_);
        device_.freeMemory(image_memory_);
        device_.destroyImageView(image_view_);
        device_.destroySampler(sampler_);
    } catch (const vk::SystemError& err) {
        throw std::runtime_error(
            "ERR 010: Failed to free image recources. Image::~Image(...)\n"
            + err.code().message()
        );
    }
}

void MemoryBuffer::set(
    const void* const data,
    const vk::DeviceSize buffer_size
) const {
    try {
        void* mapped = device_.mapMemory(memory_, 0, buffer_size);
        memcpy(mapped, data, buffer_size);
        device_.unmapMemory(memory_);
    } catch (const vk::SystemError& err) {
        throw std::runtime_error(
            "ERR 011: Failed to map memory for buffer. MemoryBuffer::set(...)\n"
            + err.code().message()
        );
    }
}

void MemoryBuffer::copy(
    const MemoryBuffer& source,
    const vk::DeviceSize size
) const {
    const vk::CommandBuffer command_buffer = CommandBuffer::begin(device_, command_pool_);

    command_buffer.copyBuffer(
        source.getBuffer(),
        buffer_,
        vk::BufferCopy {0, 0, size}
    );

    CommandBuffer::end(device_, command_buffer, command_pool_, graphics_queue_);
}

uint32_t MemoryBuffer::findMemoryType(
    const vk::PhysicalDevice p_device,
    const uint32_t filter,
    const vk::MemoryPropertyFlags& properties
) {
    const vk::PhysicalDeviceMemoryProperties memory_properties = p_device.getMemoryProperties();

    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
        if (
            (filter & (1 << i)) &&
            (memory_properties.memoryTypes[i].propertyFlags & properties) == properties
        ) return i;
    }

    throw std::runtime_error("ERR 012: Failed to find suitable memory type! MemoryBuffer::findMemoryType(...)\n");
}

void MemoryBuffer::bufferAsImage(
    const vk::Image image,
    const uint32_t width, const uint32_t height,
    const vk::Device device,
    const vk::Queue graphics_queue,
    const vk::CommandPool command_pool,
    const vk::Buffer buffer
) {
    const vk::CommandBuffer command_buffer = CommandBuffer::begin(device, command_pool);

    const vk::BufferImageCopy region {
        0,
        0, 0,
        {
            vk::ImageAspectFlagBits::eColor,
            0,
            0,
            1
        },
        {0, 0, 0},
        {width, height, 1}
    };

    command_buffer.copyBufferToImage(
        buffer,
        image,
        vk::ImageLayout::eTransferDstOptimal,
        1,
        &region
    );

    CommandBuffer::end(device, command_buffer, command_pool, graphics_queue);
}

void MemoryBuffer::createBuffer(
    const vk::DeviceSize size,
    const vk::BufferUsageFlags &usage,
    const vk::MemoryPropertyFlags &properties
) {
    try {
        buffer_ = device_.createBuffer({
            {},
            size,
            usage,
            vk::SharingMode::eExclusive
        });
    } catch (const vk::SystemError& err) {
        throw std::runtime_error(
            "ERR 013: Failed to create buffer. MemoryBuffer::createBuffer(...)\n"
            + err.code().message()
        );
    }

    const vk::MemoryRequirements mem_rqs = device_.getBufferMemoryRequirements(buffer_);
    const vk::MemoryAllocateInfo allocInfo {
        mem_rqs.size,
        findMemoryType(physical_device_, mem_rqs.memoryTypeBits, properties)
    };

    try {
        memory_ = device_.allocateMemory(allocInfo);
    } catch (const vk::SystemError& err) {
        throw std::runtime_error(
            "ERR 014: Failed to allocate memory for buffer. MemoryBuffer::createBuffer(...)\n"
            + err.code().message()
        );
    }

    device_.bindBufferMemory(buffer_, memory_, 0);
}
