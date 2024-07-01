#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include <chrono>

#include "objects.hpp"
#include "vulkan/vulkan-utils.hpp"

using MeshPtr = std::shared_ptr<Mesh>;

class ThreeDL {
    public:
        std::chrono::high_resolution_clock::time_point time_ = std::chrono::high_resolution_clock::now();

        ThreeDL() {
            glfwInit();
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // no opengl

            window_ = glfwCreateWindow(
                1280,
                720,
                "ThreeDL App",
                nullptr, nullptr
            );

            glfwSetWindowUserPointer(window_, this);
            glfwSetFramebufferSizeCallback(window_, onResize);
            glfwSetKeyCallback(window_, onKey);
            //glfwSetInputMode(window_, GLFW_TRUE);
        }

        void parseKeys() {
            for (const auto& [key, pressed] : keys_) {
                if (pressed) pressedKey(key);
            }
        }

        glm::mat3 getYRotationMat(int dir) {
            auto theta = glm::radians(0.01f * dir);
            return glm::mat3{
                glm::vec3(glm::cos(theta), 0, glm::sin(theta)),
                glm::vec3(0, 1, 0),
                glm::vec3(-glm::sin(theta), 0, glm::cos(theta))
            };
        }

        void pressedKey(int key) {
            auto time = std::chrono::high_resolution_clock::now();
            auto delta = std::chrono::duration<float, std::chrono::seconds::period>(time - time_).count();
            delta *= 1000;
            time_ = time;
            switch (key) {
                case GLFW_KEY_W:
                    camera_ = glm::translate(camera_, forward_ * (delta));
                    break;
                case GLFW_KEY_S:
                    camera_ = glm::translate(camera_, -forward_ * (delta));
                    break;
                case GLFW_KEY_A:
                    camera_ = glm::translate(camera_, left_ * (delta));
                    break;
                case GLFW_KEY_D:
                    camera_ = glm::translate(camera_, -left_ * (delta));
                    break;
                case GLFW_KEY_Q:
                    rotation_ = glm::rotate(rotation_, glm::radians(-0.01f), glm::vec3(0.0f, 1.0f, 0.0f));
                    left_ = left_ * getYRotationMat(1);
                    forward_ = forward_ * getYRotationMat(1);
                    break;
                case GLFW_KEY_E:
                    rotation_ = glm::rotate(rotation_, glm::radians(0.01f), glm::vec3(0.0f, 1.0f, 0.0f));
                    left_ = glm::normalize(left_ * getYRotationMat(-1));
                    forward_ = glm::normalize(forward_ * getYRotationMat(-1));
                    break;
            }
        }

        static void onKey(GLFWwindow* window, int key, int scancode, int action, int mods) {
            auto instance = static_cast<ThreeDL*>(glfwGetWindowUserPointer(window));

            if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }

            instance->keys_[key] = action != GLFW_RELEASE;
        }

        static void onResize(GLFWwindow* window, const int width, const int height) {
            const auto instance = static_cast<ThreeDL*>(glfwGetWindowUserPointer(window));

            instance->app_->info_.width_ = width;
            instance->app_->info_.height_ = height;
            instance->app_->resized_ = true;
        }

        void add(const MeshPtr& mesh) {
            meshes_.push_back(mesh);
        }

        void start() {
            app_ = std::make_unique<vlkn<Vertex>>(window_);

            for (const auto& mesh : meshes_) {
                app_->add(mesh);
            }

            app_->init();

            while (!glfwWindowShouldClose(window_)) {
                glfwPollEvents();
                parseKeys();
                time_ = std::chrono::high_resolution_clock::now();
                ubo_.camera = camera_;
                ubo_.rotation = rotation_;
                app_->newFrame(ubo_);
            }
        }

        ~ThreeDL() {
            glfwDestroyWindow(window_);
            glfwTerminate();
        }
    private:
        std::vector<MeshPtr> meshes_;
        GLFWwindow* window_;

        std::unique_ptr<vlkn<Vertex>> app_ = nullptr;

        std::unordered_map<int, bool> keys_;

        glm::mat4 camera_ = glm::mat4(1.0f);
        glm::mat4 rotation_ = glm::mat4(1.0f);
        glm::vec3 forward_ = glm::vec3(0.0f, 0.0f, 0.1f);
        glm::vec3 left_ = glm::vec3(0.1f, 0.0f, 0.0f);

        UniformBufferObject ubo_ = {
            glm::mat4(1.0f),
            camera_
        };

        [[nodiscard]] std::vector<Vertex> meshVector() const {
            std::vector<Vertex> vertices {};

            for (const auto& mesh : meshes_) {
                vertices.insert(
                    vertices.end(),
                    mesh->vertices_->begin(),
                    mesh->vertices_->end()
                );

                mesh->vertices_.reset();
            }

            return vertices;
        }
};