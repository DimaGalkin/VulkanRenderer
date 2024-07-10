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

class Vertex {
    public:
        Vertex(
            const glm::vec3& pos,
            const glm::vec3& color,
            const glm::vec2& uv
        ) : pos { pos },
            color { color },
            uv { uv }
        {}

        glm::vec3 pos, color;
        glm::vec2 uv;

        static vk::VertexInputBindingDescription getBindingDescription();
        static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions();
};

class OBJLoader {
    public:
        static std::vector<std::string> split(const std::string& str, char delimiter);
        static std::unique_ptr<std::vector<Vertex>> load(const std::string& path);
};

class Mesh {
    public:
        explicit Mesh(
            const std::string& path
        ) : path_ { path } {}

        void init_buffer(
            vk::Device device,
            vk::Queue graphics_queue,
            vk::CommandPool command_pool,
            vk::PhysicalDevice p_device
        );

        void render(
            const vk::CommandBuffer& command_buffer
        ) const;

        void load() {
            if (loaded_) return;

            vertices_ = OBJLoader::load(path_);
            loaded_ = true;
        }

        ~Mesh() = default;

        std::unique_ptr<std::vector<Vertex>> vertices_;
        MemoryBuffer* buffer_ = nullptr;

    private:
        std::string path_;
        bool loaded_ = false;
};
using MeshPtr = std::shared_ptr<Mesh>;

class Texture {
    public:
        explicit Texture(
            const std::string& path
        ) : path_ { path } {}

        void load(
            vk::Device device,
            vk::Queue graphics_queue,
            vk::CommandPool command_pool,
            vk::PhysicalDevice p_device,
            const vk::DescriptorSetLayout& layout,
            const vk::DescriptorPool& descriptor_pool
        );

        void render(
            const vk::CommandBuffer& command_buffer,
            const vk::PipelineLayout& pipeline_layout
        ) const;

        ~Texture() = default;

    private:
        std::string path_;
        bool loaded_ = false;

        vk::Device device_;

        Image image_;
};
using TexturePtr = std::shared_ptr<Texture>;

class Object {
    public:
        Object(
            const MeshPtr& mesh,
            const TexturePtr& tex
        ) : mesh_ {mesh},
            tex_ {tex}
        {};

        Object(
            const std::string& model_path,
            const std::string& tex_path
        ) : mesh_ {std::make_shared<Mesh>(model_path)},
            tex_ {std::make_shared<Texture>(tex_path)}
        {}

        void loadTexture(
            const vk::Device device,
            const vk::Queue graphics_queue,
            const vk::CommandPool command_pool,
            const vk::PhysicalDevice p_device,
            const vk::DescriptorSetLayout& layout,
            const vk::DescriptorPool& descriptor_pool
        ) const {
            tex_->load(
                device,
                graphics_queue,
                command_pool,
                p_device,
                layout,
                descriptor_pool
            );
        }

        void loadMesh(
            const vk::Device device,
            const vk::Queue graphics_queue,
            const vk::CommandPool command_pool,
            const vk::PhysicalDevice p_device
        ) const {
            mesh_->load();
            mesh_->init_buffer(device, graphics_queue, command_pool, p_device);
        }

        void render(
            const vk::CommandBuffer& command_buffer,
            const vk::PipelineLayout& pipeline_layout
        ) const {
            mesh_->render(command_buffer);
            tex_->render(command_buffer, pipeline_layout);
        }

        MeshPtr mesh_;
        TexturePtr tex_;
};
using ObjectPtr = std::shared_ptr<Object>;