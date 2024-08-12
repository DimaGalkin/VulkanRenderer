#include "objects.hpp"

vk::VertexInputBindingDescription Vertex::getBindingDescription() {
    return {
        0,
        sizeof(Vertex),
        vk::VertexInputRate::eVertex
    };
}

std::array<vk::VertexInputAttributeDescription, 3> Vertex::getAttributeDescriptions() {
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
        },
        vk::VertexInputAttributeDescription {
            2,
            0,
            vk::Format::eR32G32Sfloat,
            offsetof(Vertex, uv)
        }
    };
}

std::vector<std::string> OBJLoader::split(
    const std::string& str,
    const char delimiter
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
    std::vector<glm::vec2> uvs;
    std::vector<Vertex> verts;

    std::ifstream file { path };

    if (!file.is_open()) {
        throw std::runtime_error("ERR 015: Failed to open OBJ file with path: " + path);
    }

    for (std::string line; std::getline(file, line);) {
        const std::vector<std::string> tokens = split(line, ' ');

        if (tokens[0] == "v") {
            vertices.emplace_back(
                std::stof(tokens[1]),
                -std::stof(tokens[2]),
                std::stof(tokens[3])
            );
        } else if (tokens[0] == "f") {
            auto v1 = split(tokens[1], '/');
            auto v2 = split(tokens[2], '/');
            auto v3 = split(tokens[3], '/');

            verts.push_back({
                vertices[std::stoi(v1[0]) - 1],
                {1.0f, 1.0f, 1.0f},
                uvs[std::stoi(v1[1]) - 1]
            });

            verts.push_back({
                vertices[std::stoi(v2[0]) - 1],
                {1.0f, 1.0f, 1.0f},
                uvs[std::stoi(v2[1]) - 1]
            });

            verts.push_back({
                vertices[std::stoi(v3[0]) - 1],
                {1.0f, 1.0f, 1.0f},
                uvs[std::stoi(v3[1]) - 1]
            });
        } else if (tokens[0] == "vt") {
            uvs.emplace_back(
                std::stof(tokens[1]),
                1 - std::stof(tokens[2])
            );
        }
    }

    return std::make_unique<std::vector<Vertex>>(verts);
}

void Mesh::init_buffer(
    const vk::Device device,
    const vk::Queue graphics_queue,
    const vk::CommandPool command_pool,
    const vk::PhysicalDevice p_device
) {
    if (buffer_ != nullptr) return;

    buffer_ = new MemoryBuffer {
        vertices_->size() * sizeof(vertices_->at(0)),
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        device,
        graphics_queue,
        command_pool,
        p_device
    };

    const MemoryBuffer copy_buffer {
        vertices_->size() * sizeof(vertices_->at(0)),
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        device,
        graphics_queue,
        command_pool,
        p_device
    };

    copy_buffer.set(vertices_->data(), vertices_->size() * sizeof(vertices_->at(0)));
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