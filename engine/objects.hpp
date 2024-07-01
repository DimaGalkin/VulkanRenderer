#pragma once

#include <vulkan/vulkan.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <memory>

#include "vulkan/buffers.hpp"

template <typename T> class vlkn;

class Vertex {
    public:
        Vertex(const glm::vec3& pos, const glm::vec3& color) : pos {pos}, color {color} {}

        glm::vec3 pos;
        glm::vec3 color;

        static vk::VertexInputBindingDescription getBindingDescription() {
            vk::VertexInputBindingDescription bindingDescription = {};
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(Vertex);
            bindingDescription.inputRate = vk::VertexInputRate::eVertex;

            return bindingDescription;
        }

        static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions() {
            std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions = {};
            attributeDescriptions[0].binding = 0;
            attributeDescriptions[0].location = 0;
            attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
            attributeDescriptions[0].offset = offsetof(Vertex, pos);

            attributeDescriptions[1].binding = 0;
            attributeDescriptions[1].location = 1;
            attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
            attributeDescriptions[1].offset = offsetof(Vertex, color);

            return attributeDescriptions;
        }
};

class OBJLoader {
    public:
        static std::vector<std::string> split(const std::string& str, const char& delimiter) {
            std::vector<std::string> tokens;
            std::string token;
            std::istringstream tokenStream {str};

            while (std::getline(tokenStream, token, delimiter)) {
                tokens.push_back(token);
            }

            return tokens;
        }

        static std::unique_ptr<std::vector<Vertex>> load(const std::string& path_) {
            std::vector<glm::vec3> vertices;
            std::vector<Vertex> verts;

            std::ifstream file {path_};

            if (!file.is_open()) {
                throw std::runtime_error("Failed to open file!");
            }

            std::string line;

            for (std::string line; std::getline(file, line);) {
                auto tokens = split(line, ' ');

                if (tokens[0] == "v") {
                    vertices.push_back({
                        std::stof(tokens[1]),
                        std::stof(tokens[2]),
                        std::stof(tokens[3])
                    });
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
};

class Mesh {
    public:
        explicit Mesh(const std::string& path) : vertices_ {OBJLoader::load(path)} {}

        void init_buffer(
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

            MemoryBuffer<Vertex> copy_buffer {
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

        void render(
            const vk::CommandBuffer& command_buffer
        ) const {
            const vk::Buffer buffers[] = { buffer_->getBuffer() };
            const vk::DeviceSize offsets[] = { 0 };
            command_buffer.bindVertexBuffers(0, 1, buffers, offsets);
            command_buffer.draw(static_cast<uint32_t>(vertices_->size()), 1, 0, 0);
        }

        ~Mesh() {
            delete buffer_;
        }

        std::unique_ptr<std::vector<Vertex>> vertices_;
        MemoryBuffer<Vertex>* buffer_ = nullptr;
    private:
        glm::mat4 model_ = glm::mat4(1.0f);
};