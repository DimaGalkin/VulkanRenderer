#include "engine/vulkan/vulkan-utils.hpp"

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // no opengl

    GLFWwindow* window = glfwCreateWindow(
        1280,
        720,
        "ThreeDL App",
        nullptr, nullptr
    );

    vlkn<Vertex> app {window};

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        app.newFrame();
    }

    return EXIT_SUCCESS;
}