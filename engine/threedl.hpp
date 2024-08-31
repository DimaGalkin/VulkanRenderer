#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <functional>

#include "camera.hpp"
#include "vulkan/vulkan-utils.hpp"

namespace tdl {
    // useful link for testing video textures
    static constexpr char big_buck_bunny[] = "https://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4";

    /**
     * @breif 3D Engine: used to render tdl::Model
     *
     * The ThreeDL class is the class that the user uses to render objects.
     * The add() method can be used to add Models to the rendering queue, the class will render all of these without any
     * additional input. The setCamera() method can be used to set the Camera or Camera controller that will be
     * responsible for providing the projection matrix. The start() method is used to start the rendering process, it
     * can be used with or without the animation function which the class calls every frame: this can be used to move
     * objects around without user input.
    */
    class ThreeDL {
        public:
            ThreeDL() = default;

            /**
            * @breif Adds the given model to the render queue
            *
            * @param model shared_model<Model> to the model to add to the render queue
            */
            void add (
                const shared_model<Model>& model
            ) {
                models_.push_back(model);
            }

            /**
             * @breif Sets the camera controller of the ThreeDL class
             *
             * Tells the ThreeDL class that it needs to update the camera controller state in the animation function and
             * to retrive additional camera transformation matracies.
             *
             * @tparam ControllerT Type of camera controller
             * @param controller Camera controller object created by the user
            */
            template<typename ControllerT>
            requires tdl::ValidController<ControllerT>
            void setCamera (
                const std::shared_ptr<ControllerT>& controller
            ) {
                controller_ = controller;
                controlled_ = true;
            }

            /**
             * @breif Sets the static camera for the ThreeDL class
             *
             * @param camera Static camera object created by the user
            */
            void setCamera (
                const std::shared_ptr<tdl::Camera>& camera
            ) {
                camera_ = camera;
                controlled_ = false;
            }

            /**
             * @breif Tells the renderer to start rendering the scene
             *
             * This function is an infinite loop, any code after it will only be excecuted after the user exits from the
             * window.
             *
             * @param animation User defined animation function to call
            */
            void start (
                const std::function<void()>& animation
            );
            void start() { start([]{}); } // allow user to not pass an animation function

            ~ThreeDL() {
                glfwDestroyWindow(info_.window_);
                glfwTerminate();
            }
        private:
            /**
             * @breif Called by GLFW when a key is pressed
             *
             * Parameters that are not used are required as GLFW assumes they will be there
             *
             * @param window current open window
             * @param key key pressed
             * @param scancode NOT USED
             * @param action key up / down
             * @param mods NOT USED
            */
            static void onKey (
                GLFWwindow* window,
                int key,
                int scancode,
                int action,
                int mods
            );

            /**
             * @breif Called by GLFW when the window is resized
             *
             * @param window currently open GLFW window
             * @param width new width
             * @param height new height
             */
            static void onResize(
                GLFWwindow* window,
                int width,
                int height
            );

            /**
            * @breif Run repeatedly by the engine inside a seperate frame
            *
            * Calls the user defined animation function inside
            *
            * @param animation user defined animation function to call
            */
            void internalAnimation (
                const std::function<void()>& animation
            );

            /**
             * @breif Opens GLFW window based on params in info_
            */
            void openWindow();

            // shows GLFW window
            void showWindow() const { glfwShowWindow(info_.window_); }

            // hides GLFW window
            void hideWindow() const { glfwHideWindow(info_.window_); }

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