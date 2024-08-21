#include "threedl.hpp"

#include "objects.hpp"

void tdl::ThreeDL::internalAnimation(
    const std::function<void()> &animation
) {
    while (!glfwWindowShouldClose(info_.window_)) {
        glfwPollEvents();

        const auto time = std::chrono::high_resolution_clock::now();
        const float delta = std::chrono::duration<float, std::chrono::milliseconds::period>(time - time_).count();

        if (delta < 1) continue;

        for (const auto& obj : models_) {
            obj->frameTick();
        }

        animation();

        if (controlled_) {
            ubo_mutex_.lock();
            controller_->tick(keys_, delta / 10.0f);
            ubo_mutex_.unlock();
        }

        time_ = std::chrono::high_resolution_clock::now();
    }
}

void tdl::ThreeDL::openWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // no opengl

    info_.window_ = glfwCreateWindow(
        info_.width_,
        info_.height_,
        info_.title_.c_str(),
        nullptr, nullptr
    );

    hideWindow();

    glfwSetWindowUserPointer(info_.window_, this);
    glfwSetFramebufferSizeCallback(info_.window_, onResize);
    glfwSetKeyCallback(info_.window_, onKey);
}

void tdl::ThreeDL::start(
    const std::function<void()> &animation
) {
    app_ = std::make_unique<Vlkn>(&info_);

    openWindow();

    bool no_cam = false;
    if (controlled_) {
        if (controller_ == nullptr) no_cam = true;
    } else {
        if (camera_ == nullptr) no_cam = true;
    }

    if (no_cam) {
        throw std::runtime_error("No cam error");
    }

    for (const auto& object : models_) {
        app_->add(object);
    }

    app_->init();

    time_ = std::chrono::high_resolution_clock::now();
    std::jthread user_thread (&tdl::ThreeDL::internalAnimation, this, animation);

    showWindow();

    while (!glfwWindowShouldClose(info_.window_)) {
        if (controlled_) {
            ubo_mutex_.lock();

            ubo_.proj = controller_->getProjectionMatrix();
            ubo_.camera = controller_->getCameraMatrix();
            ubo_.rotation = controller_->getRotationMatrix();

            ubo_mutex_.unlock();
        } else {
            ubo_.proj = camera_->getProjectionMatrix();
        }

        app_->newFrame(ubo_);
    }

    user_thread.join();
}

void tdl::ThreeDL::onKey(
    GLFWwindow *window,
    const int key,
    const int scancode,
    const int action,
    const int mods
) {
    const auto instance = static_cast<tdl::ThreeDL*>(glfwGetWindowUserPointer(window));

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    instance->keys_[key] = action != GLFW_RELEASE;
}

void tdl::ThreeDL::onResize(
    GLFWwindow *window,
    const int width,
    const int height
) {
    const auto instance = static_cast<tdl::ThreeDL*>(glfwGetWindowUserPointer(window));

    instance->app_->info_->width_ = width;
    instance->app_->info_->height_ = height;
    instance->app_->resized_ = true;
}
