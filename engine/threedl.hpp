#pragma once

#include <opencv4/opencv2/opencv.hpp>

#include <vector>
#include <memory>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <thread>
#include <functional>

#include "types.hpp"
#include "objects.hpp"
#include "camera.hpp"
#include "vulkan/vulkan-utils.hpp"

static constexpr char big_buck_bunny[] = "https://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4";

template <typename ObjectT>
concept ValidObject = std::convertible_to<ObjectT, Object<>> || std::convertible_to<ObjectT, Object<tdl::File::Video>>;

template <tdl::Control T = tdl::Control::Independent>
class ThreeDL {
    public:
        ThreeDL() {
            glfwInit();
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // no opengl

            window_ = glfwCreateWindow(
                1920,
                1200,
                "ThreeDL App",
                nullptr, nullptr
            );

            glfwSetWindowUserPointer(window_, this);
            glfwSetFramebufferSizeCallback(window_, onResize);
            glfwSetKeyCallback(window_, onKey);
        };

        template <typename ObjectT>
        requires ValidObject<ObjectT>
        void add(const std::shared_ptr<ObjectT>& object) { objects_.push_back(object); }

        void internalAnimation(const std::function<void()>& animation) {
            while (!glfwWindowShouldClose(window_)) {
                glfwPollEvents();

                const auto time = std::chrono::high_resolution_clock::now();
                const float delta = std::chrono::duration<float, std::chrono::milliseconds::period>(time - time_).count();

                if (delta < 1) continue;

                for (const auto& obj : objects_) {
                    obj->frameTick();
                }

                animation();

                if constexpr (T == tdl::Control::Controlled) {
                    ubo_mutex_.lock();
                    controller_->tick(keys_, delta / 10.0f);
                    ubo_mutex_.unlock();
                }

                time_ = std::chrono::high_resolution_clock::now();
            }   
        }

        // for just a camera
        void setCamera(const std::shared_ptr<tdl::Camera>& camera) {
            if constexpr (T == tdl::Control::Independent) {
                camera_ = camera;
            } else {
                throw std::runtime_error("Controller needed");
            }
        }

        // for a camera controller
        template <typename ControllerT>
        void setCamera(const std::shared_ptr<ControllerT>& controller)
        requires std::derived_from<ControllerT, tdl::CameraController>
        {
            if constexpr (T == tdl::Control::Controlled) {
                controller_ = controller;
            } else {
                throw std::runtime_error("Camera needed");
            }
        }

        void start(const std::function<void()>& animation) {
            app_ = std::make_unique<Vlkn>(window_);

            bool no_cam = false;
            if constexpr (T == tdl::Control::Controlled) {
                if (controller_ == nullptr) no_cam = true;
            } else {
                if (camera_ == nullptr) no_cam = true;
            }

            if (no_cam) {
                throw std::runtime_error("No cam error");
            }

            for (const auto& object : objects_) {
                app_->add(object);
            }

            app_->init();

            time_ = std::chrono::high_resolution_clock::now();
            std::jthread user_thread (&ThreeDL::internalAnimation, this, animation);

            while (!glfwWindowShouldClose(window_)) {
                if constexpr (T == tdl::Control::Controlled) {
                    ubo_mutex_.lock();

                    ubo_.proj = controller_->getProjectionMatrix();
                    ubo_.camera = controller_->getCameraMatrix();
                    ubo_.rotation = controller_->getRotationMatrix();

                    ubo_mutex_.unlock();
                } else {
                    ubo_.proj = camera_->getProjectionMatrix();
                    ubo_.camera = glm::mat4(1);
                    ubo_.rotation = glm::mat4(1);
                }

                app_->newFrame(ubo_);
            }

            user_thread.join();
        };

        static void onKey(GLFWwindow* window, const int key, const int scancode, const int action, const int mods) {
            const auto instance = static_cast<ThreeDL*>(glfwGetWindowUserPointer(window));

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

        ~ThreeDL() {
            glfwDestroyWindow(window_);
            glfwTerminate();
        }
    private:
        GLFWwindow* window_;

        std::unique_ptr<Vlkn> app_ = nullptr;

        std::vector<std::shared_ptr<ObjectInterface>> objects_;

        std::unordered_map<int, bool> keys_;

        std::mutex ubo_mutex_;

        std::shared_ptr<tdl::CameraController> controller_ = nullptr;
        std::shared_ptr<tdl::Camera> camera_ = nullptr;

        std::chrono::high_resolution_clock::time_point time_ = std::chrono::high_resolution_clock::now();

        UniformBufferObject ubo_ = {
            glm::mat4(1.0f),
            glm::mat4(1.0f),
            glm::mat4(1.0f)
        };
};