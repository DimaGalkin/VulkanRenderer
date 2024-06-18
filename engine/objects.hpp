#pragma once

#include <vulkan/vulkan.hpp>

#include <glm/glm.hpp>

#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

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
        static std::vector<std::string> msplit (const std::string& str, const char& delimiter) {
            std::vector<std::string> tokens;
            std::string token;
            std::istringstream tokenStream {str};

            while (std::getline(tokenStream, token, delimiter)) {
                tokens.push_back(token);
            }

            return tokens;
        }

        static std::vector<Vertex> load(const std::string& path_) {
            std::vector<glm::vec3> vertices;
            std::vector<Vertex> verts;

            std::ifstream file {path_};

            if (!file.is_open()) {
                throw std::runtime_error("Failed to open file!");
            }

            std::string line;

            for (std::string line; std::getline(file, line);) {
                auto tokens = msplit(line, ' ');

                if (tokens[0] == "v") {
                    vertices.push_back({
                        std::stof(tokens[1]),
                        std::stof(tokens[2]),
                        std::stof(tokens[3])
                    });
                } else if (tokens[0] == "f") {
                    auto v1 = msplit(tokens[1], '/');
                    auto v2 = msplit(tokens[2], '/');
                    auto v3 = msplit(tokens[3], '/');

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

            return verts;
        }
};