#pragma once

#include <memory>

#include "vulkan/buffers.hpp"
#include "objects.hpp"

#include <vulkan/vulkan.hpp>

#include <glm/glm.hpp>

#include <vector>

namespace tdl {
    enum class LightingModels : int {
        LAMBERT = 0,
        BLINNPHONG = 1,
        PHONG = 2
    };

    enum class LightType : int {
        AMBIENT = 0,
        POINT = 1,
        DIRECTIONAL = 2
    };

    struct alignas(16) Light {
        glm::vec4 position;
        glm::vec4 direction;
        glm::vec4 color {1.0f};
        glm::vec4 data;
    };

    struct LightObject {
        alignas(16) Light lights[16];
        alignas(2) int num_lights = 0;
    };

    class LightHelper {
        public:
            static void render (
                const std::vector<vk::DescriptorSet>& descriptor_sets,
                vk::CommandBuffer command_buffer,
                vk::PipelineLayout pipeline_layout,
                unsigned long cframe
            );

            static void initUBOs (
                std::vector<MemoryBuffer*>& ubos,
                unsigned int max_f_frames,
                vk::Device device,
                vk::Queue graphics_queue,
                vk::CommandPool command_pool,
                vk::PhysicalDevice physical_device
            );

            static void createDescriptorSets (
                std::vector<vk::DescriptorSet>& descriptor_sets,
                const std::vector<MemoryBuffer*>& ubos,
                unsigned int max_f_frames,
                vk::DescriptorPool descriptor_pool,
                vk::Device device,
                vk::DescriptorSetLayout ubo_layout
            );
    };

    class LightInterface {
        public:
            LightInterface (
                const glm::vec4& color,
                float intensity,
                LightingModels model
            ) : color_ { color },
                intensity_ { intensity },
                model_ { model }
            {}

            LightInterface (
                const glm::vec4& position,
                const glm::vec4& color,
                float intensity,
                LightingModels model
            ) : color_ { color },
                intensity_ { intensity },
                model_ { model },
                position_ { position }
            {}

            virtual void setColor(const glm::vec4& color) = 0;
            virtual void setIntensity(float intensity) = 0;
            virtual void setModel(LightingModels model) = 0;

            void translate(
                const glm::vec3& direction
            ) {
                position_ += glm::vec4(direction, 0);
            }

            virtual void exportGPU() = 0;

            virtual ~LightInterface() = default;

            Light ubo_data_ {};
            bool has_changed_ = true;
            glm::vec4 color_ {0.0f};
            float intensity_ = 0.0f;
            LightingModels model_ = LightingModels::BLINNPHONG;
            glm::vec4 position_ {0.0f};

            shared_model<Model> light_model_ {"../assets/light.obj", std::array<unsigned char, 4>{225, 225, 225, 225}};
    };

    class AmbientLight final : public LightInterface {
        public:
            explicit AmbientLight (
                const glm::vec4& color = glm::vec4 {1.0f, 1.0f, 1.0f, 0.0f},
                float intensity = 1.0f,
                LightingModels model = LightingModels::BLINNPHONG
            );

            void setColor (
                const glm::vec4& color
            ) override { color_ = color; has_changed_ = true; }

            void setIntensity (
                const float intensity
            ) override { intensity_ = intensity; has_changed_ = true; }

            void setModel (
                const LightingModels model
            ) override { model_ = model; has_changed_ = true; }

            void exportGPU() override {
                ubo_data_.color = color_;
                ubo_data_.data.y = intensity_;
                ubo_data_.data.z = (float)model_;
                ubo_data_.data.w = 0; // ambient
            }
    };

    class PointLight final : public LightInterface {
        public:
            explicit PointLight (
                const glm::vec4& position = glm::vec4 {0.0f},
                const glm::vec4& color = glm::vec4 {1.0f, 1.0f, 1.0f, 0.0f},
                float intensity = 250.0f,
                LightingModels model = LightingModels::PHONG
            ) : LightInterface(
                position,
                color,
                intensity,
                model
            ) {}

            void setColor (
                const glm::vec4& color
            ) override { color_ = color; has_changed_ = true; }

            void setIntensity (
                const float intensity
            ) override { intensity_ = intensity; has_changed_ = true; }

            void setModel (
                const LightingModels model
            ) override { model_ = model; has_changed_ = true; }

            void exportGPU() override {
                ubo_data_.color = color_;
                ubo_data_.position = position_;
                ubo_data_.data.y = intensity_;
                ubo_data_.data.z = (int)model_;
                ubo_data_.data.w = 1; // point
                light_model_->translate(position_);
            }
    };

    // class DirectionalLight final : PointLight {
    //     public:
    //         DirectionalLight() = default;
    //
    //         ~DirectionalLight() override = default;
    //     private:
    //         glm::vec4 direction_ {0.0f};
    //         float fov_ = 0.0f;
    // };

    template <typename T, typename... Args>
    requires std::derived_from<T, LightInterface>
    std::shared_ptr<LightInterface> make_light(
        Args... args
    ) {
        return std::make_shared<T>(args...);
    }
};