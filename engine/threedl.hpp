#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include <chrono>

#include "objects.hpp"
#include "vulkan/vulkan-utils.hpp"

class ThreeDL {
    public:
        std::chrono::high_resolution_clock::time_point time_ = std::chrono::high_resolution_clock::now();

        ThreeDL();

        void add(const ObjectPtr& object) { objects_.push_back(object); }

        void start();
        void parseKeys();
        void pressedKey(int key);

        static glm::mat3 getYRotationMat(int dir);
        static void onKey(GLFWwindow* window, int key, int scancode, int action, int mods);
        static void onResize(GLFWwindow* window, int width, int height);

        ~ThreeDL() {
            glfwDestroyWindow(window_);
            glfwTerminate();
        }
    private:
        GLFWwindow* window_;

        std::unique_ptr<Vlkn> app_ = nullptr;

        std::vector<ObjectPtr> objects_;

        std::unordered_map<int, bool> keys_;

        glm::mat4 camera_ = glm::mat4(1.0f);
        glm::mat4 rotation_ = glm::mat4(1.0f);
        glm::vec3 forward_ = glm::vec3(0.0f, 0.0f, 0.1f);
        glm::vec3 left_ = glm::vec3(0.1f, 0.0f, 0.0f);

        UniformBufferObject ubo_ = {
            glm::mat4(1.0f),
            camera_
        };
};