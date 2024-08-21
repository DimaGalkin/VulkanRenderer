#pragma once

#include <opencv4/opencv2/opencv.hpp>

#include <vector>
#include <memory>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <thread>
#include <functional>

#include "camera.hpp"
#include "vulkan/vulkan-utils.hpp"

namespace tdl {
    static constexpr char big_buck_bunny[] = "https://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4";

    class ThreeDL {
        public:
            ThreeDL() = default;

            void add (
                const shared_model<Model>& model
            ) {
                models_.push_back(model);
            }

            void internalAnimation (
                const std::function<void()>& animation
            );

            template<typename ControllerT>
            requires tdl::ValidController<ControllerT>
            void setCamera(
                const std::shared_ptr<ControllerT>& controller
            ) {
                controller_ = controller;
                controlled_ = true;
            }

            void setCamera (
                const std::shared_ptr<tdl::Camera>& camera
            ) {
                camera_ = camera;
                controlled_ = false;
            }

            void openWindow();
            void showWindow() const { glfwShowWindow(info_.window_); }
            void hideWindow() const { glfwHideWindow(info_.window_); }

            void start (
                const std::function<void()>& animation
            );
            void start() { start([]{}); }

            static void onKey (
                GLFWwindow* window,
                int key,
                int scancode,
                int action,
                int mods
            );

            static void onResize(
                GLFWwindow* window,
                int width,
                int height
            );

            ~ThreeDL() {
                glfwDestroyWindow(info_.window_);
                glfwTerminate();
            }
        private:
            RendererInfo info_;

            std::unique_ptr<Vlkn> app_ = nullptr;

            std::vector<shared_model<Model>> models_;

            std::unordered_map<int, bool> keys_;

            std::mutex ubo_mutex_;

            std::shared_ptr<tdl::CameraController> controller_ = nullptr;
            std::shared_ptr<tdl::Camera> camera_ = nullptr;
            bool controlled_ = false;

            std::chrono::high_resolution_clock::time_point time_ = std::chrono::high_resolution_clock::now();

            UniformBufferObject ubo_ = {
                glm::mat4(1.0f),
                glm::mat4(1.0f),
                glm::mat4(1.0f)
            };
    };
}