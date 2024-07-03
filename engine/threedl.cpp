#include "threedl.hpp"

ThreeDL::ThreeDL() {
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
}

void ThreeDL::parseKeys() {
    for (const auto& [key, pressed] : keys_) {
        if (pressed) pressedKey(key);
    }
}

void ThreeDL::pressedKey(const int key) {
    const auto time = std::chrono::high_resolution_clock::now();
    float delta = std::chrono::duration<float, std::chrono::seconds::period>(time - time_).count();

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

        default:
            break;
    }
}

void ThreeDL::start() {
    app_ = std::make_unique<Vlkn>(window_);

    for (const auto& object : objects_) {
        app_->add(object);
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

glm::mat3 ThreeDL::getYRotationMat(const int dir) {
    const float theta = glm::radians(0.01f * static_cast<float>(dir));

    return glm::mat3{
        glm::vec3(glm::cos(theta), 0, glm::sin(theta)),
        glm::vec3(0, 1, 0),
        glm::vec3(-glm::sin(theta), 0, glm::cos(theta))
    };
}

void ThreeDL::onKey(GLFWwindow* window, const int key, const int scancode, const int action, const int mods) {
    const auto instance = static_cast<ThreeDL*>(glfwGetWindowUserPointer(window));

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    instance->keys_[key] = action != GLFW_RELEASE;
}

void ThreeDL::onResize(GLFWwindow* window, const int width, const int height) {
    const auto instance = static_cast<ThreeDL*>(glfwGetWindowUserPointer(window));

    instance->app_->info_.width_ = width;
    instance->app_->info_.height_ = height;
    instance->app_->resized_ = true;
}