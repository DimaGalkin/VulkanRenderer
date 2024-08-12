#pragma once

#include <vulkan/vulkan.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <opencv4/opencv2/videoio.hpp>
#include<opencv4/opencv2/core/mat.hpp>
#include <opencv2/core/ocl.hpp>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <memory>
#include <opencv2/imgproc.hpp>

#include "types.hpp"
#include "vulkan/buffers.hpp"

static glm::vec3 SELFCENTRE { INFINITY, INFINITY, INFINITY };

struct ModelObject {
    glm::mat4 translation_ = glm::mat4(1);
    glm::mat4 rotation_ = glm::mat4(1);
};

struct Material {
    std::string name_ = "default";

    glm::vec3 ambient_ = {1.0f, 1.0f, 1.0f};

    bool diffuse_is_map_ = false;
    glm::vec3 diffuse_ = {1.0f, 1.0f, 1.0f};
    std::string diffuse_map_path_;

    bool specular_is_map_ = false;
    glm::vec3 specular_ = {1.0f, 1.0f, 1.0f};
    std::string specular_map_path_;

    float specular_exponent_ = 10.0f;
    float transparency_d_ = 1.0f;
};

class Vertex final {
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

class OBJLoader final {
    public:
        static std::vector<std::string> split(const std::string& str, char delimiter);
        static std::unique_ptr<std::vector<Vertex>> load(const std::string& path);
};

class Mesh final {
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
        MemoryBuffer* buffer_ = nullptr; // TODO check private

    private:
        std::string path_;
        bool loaded_ = false;
};
using MeshPtr = std::shared_ptr<Mesh>;

template <tdl::File tex_type = tdl::File::Image>
class Texture final {
    public:
        explicit Texture( // TODO
            const std::string& path
        ) : path_ { path } {}

        void load(
            const vk::Device device,
            const vk::Queue graphics_queue,
            const vk::CommandPool command_pool,
            const vk::PhysicalDevice p_device,
            const vk::DescriptorSetLayout layout,
            const vk::DescriptorPool descriptor_pool
        ) {
            device_ = device;
            graphics_queue_ = graphics_queue;
            command_pool_ = command_pool;
            p_device_ = p_device;
            layout_ = layout;
            descriptor_pool_ = descriptor_pool;

            if constexpr (tex_type == tdl::File::Image) {
                loadImage();
            } else if (tex_type == tdl::File::Video) {
                loadVideo();
            } else {
                throw std::invalid_argument("Invalid texture type");
            }
        }

        void setNextImage() {
            frame_mutex_.lock();
            image_.buffer_->set(frame_data_.data, width_ * height_ * 4);
            frame_mutex_.unlock();

            Image::setImageLayout(
                image_.image_,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eTransferDstOptimal,
                device_,
                command_pool_,
                graphics_queue_
            );

            image_.buffer_->asImage(image_.image_, width_, height_);

            Image::setImageLayout(
                image_.image_,
                vk::ImageLayout::eTransferDstOptimal,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                device_,
                command_pool_,
                graphics_queue_
            );
        }

        void render(
            const vk::CommandBuffer& command_buffer,
            const vk::PipelineLayout& pipeline_layout
        ) {
            image_.render(command_buffer, pipeline_layout);
        };

        void loadNextFrame() {
            const auto time = std::chrono::high_resolution_clock::now();
            const float delta = std::chrono::duration<float, std::chrono::milliseconds::period>(time - time_).count();

            if (delta < (1000.0f/fps_)) return;
            time_ = std::chrono::high_resolution_clock::now();

            if (!video_->grab()) {
                video_->release();
                video_ = std::make_unique<cv::VideoCapture>(path_ , cv::CAP_FFMPEG);
                video_->grab();
            }

            video_->retrieve(frame_);

            frame_mutex_.lock();

            cv::cvtColor(frame_, frame_data_, cv::COLOR_BGR2RGBA);

            frame_mutex_.unlock();
        }

        ~Texture() {
            video_->release();
        };

    private:
        void loadImage() {
            if (loaded_) return;

            int width, height, channels;
            const stbi_uc* pixels = stbi_load(
                path_.c_str(),
                &width, &height,
                &channels,
                STBI_rgb_alpha
            );

            if (!pixels) {
                throw std::runtime_error("Failed to load texture image!");
            }

            image_.loadImage(
                pixels,
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height),
                device_,
                command_pool_,
                graphics_queue_,
                p_device_
            );

            stbi_image_free(const_cast<stbi_uc*>(pixels));

            image_.createSampler();

            image_.createDescriptor(
                layout_,
                descriptor_pool_
            );

            image_.updateDescriptor();

            loaded_ = true;
        }

        void loadFrame() {
            *video_ >> frame_;

            if (frame_.empty()) {
                video_->release();
                video_ = std::make_unique<cv::VideoCapture>(path_ , cv::CAP_FFMPEG);

                *video_ >> frame_;
            }

            cv::cvtColor(frame_, frame_data_, cv::COLOR_BGR2RGBA);

            image_.loadImage(
                frame_data_.data,
                static_cast<uint32_t>(width_),
                static_cast<uint32_t>(height_),
                device_,
                command_pool_,
                graphics_queue_,
                p_device_
            );

            image_.createSampler();
            image_.updateDescriptor();
        }

        void loadVideo() {
            if (loaded_) return;

            video_ = std::make_unique<cv::VideoCapture>(path_, cv::CAP_FFMPEG);

            width_ = video_->get(cv::CAP_PROP_FRAME_WIDTH);
            height_ = video_->get(cv::CAP_PROP_FRAME_HEIGHT);
            fps_ = video_->get(cv::CAP_PROP_FPS);

            image_.setDevice(device_);
            image_.createDescriptor(
                layout_,
                descriptor_pool_
            );

            loadFrame();

            loaded_ = true;
        }

        std::unique_ptr<cv::VideoCapture> video_;
        cv::Mat frame_;
        cv::Mat frame_data_;
        std::mutex frame_mutex_;

        std::string path_;
        bool loaded_ = false;

        uint32_t width_ = 0;
        uint32_t height_ = 0;
        float fps_ = 0.0f;

        vk::Device device_;
        vk::Queue graphics_queue_;
        vk::CommandPool command_pool_;
        vk::PhysicalDevice p_device_;
        vk::DescriptorSetLayout layout_;
        vk::DescriptorPool descriptor_pool_;

        std::chrono::high_resolution_clock::time_point time_ = std::chrono::high_resolution_clock::now();

        Image image_;
};

class ObjectInterface {
    friend class Vlkn;

    public:
        ObjectInterface() = default;

        virtual void rotate (
            const glm::vec3& angles,
            const glm::vec3& centre
        ) = 0;

        virtual void imageTick() = 0;

        virtual void frameTick() = 0;

        virtual void translate (
            const glm::vec3& val
        ) = 0;

        virtual ~ObjectInterface() = default;

    protected:
        virtual void loadTexture(
            vk::Device device,
            vk::Queue graphics_queue,
            vk::CommandPool command_pool,
            vk::PhysicalDevice p_device,
            const vk::DescriptorSetLayout& layout,
            const vk::DescriptorPool& descriptor_pool
        ) const = 0;

        virtual void loadMesh(
            vk::Device device,
            vk::Queue graphics_queue,
            vk::CommandPool command_pool,
            vk::PhysicalDevice p_device
        )  = 0;

        virtual void render(
            const vk::CommandBuffer& command_buffer,
            const vk::PipelineLayout& pipeline_layout,
            unsigned long cframe
        ) = 0;

        virtual void initUBOs(
            unsigned int max_f_frames,
            vk::Device device,
            vk::Queue graphics_queue,
            vk::CommandPool command_pool,
            vk::PhysicalDevice physical_device
        ) = 0;

        virtual void createDescriptorSets(
            unsigned int max_f_frames,
            vk::DescriptorPool descriptor_pool,
            vk::Device device,
            vk::DescriptorSetLayout ubo_layout
        ) = 0;

        virtual void caluclateCentre() = 0;

        std::vector<MemoryBuffer*> ubos_;
        ModelObject ubo_data_ {};
        bool has_changed_ = true;
};

template <tdl::File tex_type = tdl::File::Image>
class Object final : public ObjectInterface {
    friend class Vlkn;

    public:
        Object(
            const MeshPtr& mesh,
            const std::shared_ptr<Texture<tex_type>> tex
        ) : ObjectInterface(),
            mesh_ {mesh},
            tex_ {tex}
        {};

        Object(
            const std::string& model_path,
            const std::string& tex_path
        ) : ObjectInterface(),
            mesh_ {std::make_shared<Mesh>(model_path)},
            tex_ {std::make_shared<Texture<tex_type>>(tex_path)}
        {}

        void imageTick() override {
            if constexpr (tex_type == tdl::File::Video) tex_->setNextImage();
        }

        void frameTick() override {
            if constexpr (tex_type == tdl::File::Video) tex_->loadNextFrame();
        }

        void rotate (
            const glm::vec3& angles,
            const glm::vec3& centre
        ) override {
            glm::vec3 rot_centre = centre;
            if (centre == glm::vec3 {INFINITY, INFINITY, INFINITY}) {
                rot_centre = position_;
            }

            ubo_data_.rotation_ = glm::translate(ubo_data_.rotation_, rot_centre);
            ubo_data_.rotation_ = glm::rotate(ubo_data_.rotation_, angles.x, {1, 0, 0});
            ubo_data_.rotation_ = glm::rotate(ubo_data_.rotation_, angles.y, {0, 1, 0});
            ubo_data_.rotation_ = glm::rotate(ubo_data_.rotation_, angles.z, {0, 0, 1});
            ubo_data_.rotation_ = glm::translate(ubo_data_.rotation_, -rot_centre);

            has_changed_ = true;
        }

        void translate (
            const glm::vec3& val
        ) override {
            ubo_data_.translation_ = glm::translate(ubo_data_.translation_, val);
        }

        ~Object() override {
            for (const auto& memory : ubos_) {
                delete memory;
            }
        }

    protected:
        void loadTexture (
            const vk::Device device,
            const vk::Queue graphics_queue,
            const vk::CommandPool command_pool,
            const vk::PhysicalDevice p_device,
            const vk::DescriptorSetLayout& layout,
            const vk::DescriptorPool& descriptor_pool
        ) const override {
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
        ) override {
            mesh_->load();
            mesh_->init_buffer(device, graphics_queue, command_pool, p_device);
            caluclateCentre();
        }

        void render(
            const vk::CommandBuffer& command_buffer,
            const vk::PipelineLayout& pipeline_layout,
            const unsigned long cframe
        ) override {
            command_buffer.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                pipeline_layout,
                2,
                1,
                &descriptor_sets_[cframe],
                0,
                nullptr
            );

            tex_->render(command_buffer, pipeline_layout);
            mesh_->render(command_buffer);
        }

        void initUBOs(
            const unsigned int max_f_frames,
            const vk::Device device,
            const vk::Queue graphics_queue,
            const vk::CommandPool command_pool,
            const vk::PhysicalDevice physical_device
        ) override {
            ubos_.resize(max_f_frames);
            for (unsigned int i = 0; i < max_f_frames; ++i) {
                ubos_[i] = new MemoryBuffer (
                    sizeof(ModelObject),
                    vk::BufferUsageFlagBits::eUniformBuffer,
                    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                    device,
                    graphics_queue,
                    command_pool,
                    physical_device
                );
            }
        }

        void createDescriptorSets(
            const unsigned int max_f_frames,
            const vk::DescriptorPool descriptor_pool,
            const vk::Device device,
            const vk::DescriptorSetLayout ubo_layout
        ) override {
            std::vector<vk::DescriptorSetLayout> layouts (max_f_frames, ubo_layout);

            const vk::DescriptorSetAllocateInfo alloc_info {
                descriptor_pool,
                static_cast<uint32_t>(max_f_frames),
                layouts.data()
            };

            descriptor_sets_.resize(max_f_frames);

            if (
                device.allocateDescriptorSets(
                    &alloc_info,
                    descriptor_sets_.data()
                ) != vk::Result::eSuccess
            ) throw std::runtime_error("ERR 14 ");

            for (int i = 0; i < max_f_frames; ++i) {
                const vk::DescriptorBufferInfo buffer_info {
                    ubos_[i]->getBuffer(),
                    0,
                    sizeof(ModelObject)
                };

                const vk::WriteDescriptorSet descriptor_write {
                    descriptor_sets_[i],
                    1,
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

        void caluclateCentre() override {
            centre_ = glm::vec3(0.0f);
            for (const auto& vert : *mesh_->vertices_) centre_ += vert.pos;
            centre_ /= mesh_->vertices_->size();
            position_ = centre_;
        }

        MeshPtr mesh_;
        std::shared_ptr<Texture<tex_type>> tex_;

        glm::vec3 centre_ {};
        glm::vec3 position_ {};

        std::vector<vk::DescriptorSet> descriptor_sets_;
};
using ObjectPtr = std::shared_ptr<Object<>>;

template <tdl::File tex_type = tdl::File::Image, typename... Args>
std::shared_ptr<Object<tex_type>> makeObject(Args... args) {
    return std::make_shared<Object<tex_type>>(args...);
}