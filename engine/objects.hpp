#pragma once

#include <vulkan/vulkan.hpp>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

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
        Vertex(
            const glm::vec3& pos,
            const glm::vec3& color
        ) : pos { pos },
            color { color }
        {}

        glm::vec3 pos, color;

        static vk::VertexInputBindingDescription getBindingDescription();
        static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions();
};

class OBJLoader {
    public:
        static std::vector<std::string> split(const std::string& str, const char& delimiter);
        static std::unique_ptr<std::vector<Vertex>> load(const std::string& path);
};

class Mesh {
    public:
        explicit Mesh(const std::string& path) : path_ {path} {}

        void init_buffer(
            const std::shared_ptr<vk::Device>& device,
            vk::Queue* graphics_queue,
            vk::CommandPool* command_pool,
            vk::PhysicalDevice* p_device
        );

        void render(const vk::CommandBuffer& command_buffer) const;

        void load() {
            if (loaded_) return;

            vertices_ = OBJLoader::load(path_);
            loaded_ = true;
        }

        ~Mesh() {
            delete buffer_;
        }

        std::unique_ptr<std::vector<Vertex>> vertices_;
        MemoryBuffer<Vertex>* buffer_ = nullptr;

    private:
        std::string path_;
        bool loaded_ = false;
};
using MeshPtr = std::shared_ptr<Mesh>;

class Texture {
    public:
        explicit Texture(const std::string& path) : path_ {path} {}

        void load(
            const std::shared_ptr<vk::Device>& device,
            vk::Queue* graphics_queue,
            vk::CommandPool* command_pool,
            vk::PhysicalDevice* p_device
        );

        ~Texture() {
            delete pixel_data_;

            if (image_) {
                device_->destroyImage(image_);
            }
            if (image_memory_) {
                device_->freeMemory(image_memory_);
            }
        }

    private:
        std::string path_;

        bool loaded_ = false;

        MemoryBuffer<stbi_uc*>* pixel_data_ = nullptr;

        std::shared_ptr<vk::Device> device_;

        vk::Image image_;
        vk::DeviceMemory image_memory_;
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

        void load_texture(
            const std::shared_ptr<vk::Device>& device,
            vk::Queue* graphics_queue,
            vk::CommandPool* command_pool,
            vk::PhysicalDevice* p_device
        ) const {
            //tex_->load(device, graphics_queue, command_pool, p_device);
        }

        void load_mesh(
            const std::shared_ptr<vk::Device>& device,
            vk::Queue* graphics_queue,
            vk::CommandPool* command_pool,
            vk::PhysicalDevice* p_device
        ) const {
            mesh_->load();
            mesh_->init_buffer(device, graphics_queue, command_pool, p_device);
        }

        void render(const vk::CommandBuffer& command_buffer) const {
            mesh_->render(command_buffer);
        }

        MeshPtr mesh_;
        TexturePtr tex_;
};
using ObjectPtr = std::shared_ptr<Object>;