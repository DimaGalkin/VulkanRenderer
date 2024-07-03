#include "objects.hpp"

vk::VertexInputBindingDescription Vertex::getBindingDescription() {
    return {
        0,
        sizeof(Vertex),
        vk::VertexInputRate::eVertex
    };
}

std::array<vk::VertexInputAttributeDescription, 2> Vertex::getAttributeDescriptions() {
    return {
        vk::VertexInputAttributeDescription {
            0,
            0,
            vk::Format::eR32G32B32Sfloat,
            offsetof(Vertex, pos)
        },
        vk::VertexInputAttributeDescription {
            1,
            0,
            vk::Format::eR32G32B32Sfloat,
            offsetof(Vertex, color)
        }
    };
}

std::vector<std::string> OBJLoader::split(
    const std::string& str,
    const char& delimiter
) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream {str};

    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}

std::unique_ptr<std::vector<Vertex>> OBJLoader::load(
    const std::string& path
) {
    std::vector<glm::vec3> vertices;
    std::vector<Vertex> verts;

    std::ifstream file { path };

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file!");
    }

    for (std::string line; std::getline(file, line);) {
        const std::vector<std::string> tokens = split(line, ' ');

        if (tokens[0] == "v") {
            vertices.emplace_back(
                std::stof(tokens[1]),
                std::stof(tokens[2]),
                std::stof(tokens[3])
            );
        } else if (tokens[0] == "f") {
            auto v1 = split(tokens[1], '/');
            auto v2 = split(tokens[2], '/');
            auto v3 = split(tokens[3], '/');

            verts.push_back({
                vertices[std::stoi(v1[0]) - 1],
                {1.0f, 1.0f, 1.0f}
            });

            verts.push_back({
                vertices[std::stoi(v2[0]) - 1],
                {1.0f, 1.0f, 1.0f}
            });

            verts.push_back({
                vertices[std::stoi(v3[0]) - 1],
                {1.0f, 1.0f, 1.0f}
            });
        }
    }

    return std::make_unique<std::vector<Vertex>>(verts);
}

void Mesh::init_buffer(
    const std::shared_ptr<vk::Device>& device,
    vk::Queue* graphics_queue,
    vk::CommandPool* command_pool,
    vk::PhysicalDevice* p_device
) {
    if (buffer_ != nullptr) return;

    buffer_ = new MemoryBuffer<Vertex>(
        vertices_->size() * sizeof(vertices_->at(0)),
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        device,
        graphics_queue,
        command_pool,
        p_device
    );

    const MemoryBuffer<Vertex> copy_buffer {
        vertices_->size() * sizeof(vertices_->at(0)),
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        device,
        graphics_queue,
        command_pool,
        p_device
    };

    copy_buffer.set(*vertices_, vertices_->size() * sizeof(vertices_->at(0)));
    buffer_->copy(copy_buffer, vertices_->size() * sizeof(vertices_->at(0)));
}

void Mesh::render(
    const vk::CommandBuffer& command_buffer
) const {
    const vk::Buffer buffers[] = { buffer_->getBuffer() };
    static constexpr vk::DeviceSize offsets[] = { 0 };
    command_buffer.bindVertexBuffers(0, 1, buffers, offsets);
    command_buffer.draw(static_cast<uint32_t>(vertices_->size()), 1, 0, 0);
}

void Texture::load(
    const std::shared_ptr<vk::Device>& device,
    vk::Queue* graphics_queue,
    vk::CommandPool* command_pool,
    vk::PhysicalDevice* p_device
) {
    if (loaded_) return;

    delete pixel_data_;

    device_ = device;

    int width, height, channels;
    stbi_uc* pixels = stbi_load(
        path_.c_str(),
        &width, &height,
        &channels,
        STBI_rgb_alpha
    );

    const vk::DeviceSize image_size = width * height * 4;

    if (!pixels) {
        throw std::runtime_error("Failed to load texture image!");
    }

    pixel_data_ = new MemoryBuffer<stbi_uc*>(
        image_size,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        device,
        graphics_queue,
        command_pool,
        p_device
    );
    pixel_data_->set({pixels}, image_size);

    stbi_image_free(pixels);

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
        image_ = device->createImage(image_info);
    } catch (const vk::SystemError& e) {
        throw std::runtime_error(
            "ERR 007: Failed to create image! Texture::load(...)"
            + std::string(e.what())
        );
    }

    const vk::MemoryRequirements mem_reqs = device->getImageMemoryRequirements(image_);

    const vk::MemoryAllocateInfo alloc_info {
        mem_reqs.size,
        MemoryBuffer<void>::findMemoryType(
            *p_device,
            mem_reqs.memoryTypeBits,
            vk::MemoryPropertyFlagBits::eDeviceLocal
        )
    };

    if (
        device->allocateMemory(&alloc_info, nullptr, &image_memory_)
        != vk::Result::eSuccess
    ) {
        throw std::runtime_error("Failed to allocate image memory!");
    }

    image_memory_ = device->allocateMemory(alloc_info);

    loaded_ = true;
}
