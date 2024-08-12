#pragma once

#include <glm/glm.hpp>
#include <glm/ext/matrix_clip_space.hpp>

#include <GLFW/glfw3.h>

#include <stdexcept>
#include <unordered_map>

namespace tdl {
    class Camera {
        friend class Vlkn;

        public:
            explicit Camera(float perspective);

            void setFov(
                float fov
            );

            void setFarPlane(
                float distance
            );

            void setNearPlane(
                float distance
            );

            void setPerspective(
                float perspective
            );

            [[nodiscard]] glm::mat4 getProjectionMatrix();

            explicit operator bool() const { return has_changed_; }

            virtual ~Camera() = default;

        protected:
            struct CameraDetails {
                float near_ = 0.01f;
                float far_ = 10000.0f;
                float fov_ = 1.13446f; // 65 degrees
                float perspective_ = 16.0f/9.0f;
            } details_ = {};

            bool has_changed_ = true;

            glm::mat4 proj_mat_ {};
    };

    class CameraController : public Camera {
        public:
            explicit CameraController(const float perspective) : Camera(perspective) {};

            virtual void tick(
                const std::unordered_map<int, bool>& keys,
                double delta_t
            ) = 0;

            [[nodiscard]] glm::mat4 getCameraMatrix() const { return camera_mat_; }
            [[nodiscard]] glm::mat4 getRotationMatrix() const { return rotation_mat_; }


            ~CameraController() override = default;
        protected:
            virtual void keyPressed(
                int key,
                double delta_t
            ) = 0;

            glm::mat4 rotation_mat_ = glm::mat4(1.0f);
            glm::mat4 camera_mat_ = glm::mat4(1.0f);
    };

    class FPSController final : public CameraController {
        public:
            explicit FPSController(const float perspective) : CameraController(perspective) {};

            void tick(
                const std::unordered_map<int, bool>& keys,
                double delta_t
            ) override;

            ~FPSController() override = default;
        private:
            void keyPressed(
                int key,
                double delta_t
            ) override;
    };
};