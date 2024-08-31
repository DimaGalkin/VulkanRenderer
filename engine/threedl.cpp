#include "threedl.hpp"

#include <thread>

#include "objects.hpp"

void tdl::ThreeDL::internalAnimation(
    const std::function<void()>& animation
) {
    while (!glfwWindowShouldClose(info_.window_)) {
        glfwPollEvents(); // Log all events, eg. key presses

        // get the current time to calculate a time delta
        const auto time = std::chrono::high_resolution_clock::now();
        const float delta = std::chrono::duration<float, std::chrono::milliseconds::period>(time - time_).count();

        if (delta < 1) continue; // this function runs at at most every millisecond

        time_ = std::chrono::high_resolution_clock::now(); // update time this was last called

        for (const auto& obj : models_) {
            obj->frameTick(); // load next frame for video textures
        }

        animation(); // call user defined animation function

        // control code only called if a camera controller was passed to setCamera()
        if (controlled_) {
            ubo_mutex_.lock();
            controller_->tick(keys_, delta / 10.0f);
            ubo_mutex_.unlock();
        }
    }
}

void tdl::ThreeDL::openWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // no opengl

    // create window
    info_.window_ = glfwCreateWindow(
        info_.width_,
        info_.height_,
        info_.title_.c_str(),
        nullptr, nullptr
    );

    hideWindow(); // hide window while loading

    // helper functions used for screen resizing & key press detection
    glfwSetWindowUserPointer(info_.window_, this);
    glfwSetFramebufferSizeCallback(info_.window_, onResize);
    glfwSetKeyCallback(info_.window_, onKey);
}

void tdl::ThreeDL::start(
    const std::function<void()>& animation
) {
    app_ = std::make_unique<Vlkn>(&info_); // create vulkan helper

    openWindow();

    if (controlled_ && controller_ == nullptr) {
        throw std::runtime_error("ERR 063: Camera controller has not been provided. ThreeDL::start(...)");
    }

    if (!controlled_ && camera_ == nullptr) {
        throw std::runtime_error("ERR 064: Camera has not been provided. ThreeDL::start(...)");
    }

    // add render queue to vulkan
    for (const auto& object : models_) {
        app_->add(object);
    }

    app_->init(); // initialse the vulkan helper

    time_ = std::chrono::high_resolution_clock::now(); // set start time
    // start thread that calls the animation function independetly of the loop below
    std::jthread user_thread (&tdl::ThreeDL::internalAnimation, this, animation);

    showWindow(); // now that everything has loaded the window can be shown

    while (!glfwWindowShouldClose(info_.window_)) {
        if (controlled_) {
            // stop animation thread making any modifications
            ubo_mutex_.lock();

            ubo_.proj = controller_->getProjectionMatrix();
            ubo_.camera = controller_->getCameraMatrix(); // camera translation
            ubo_.rotation = controller_->getRotationMatrix(); // camera rotation

            // allow modifications from a different thread
            ubo_mutex_.unlock();
        } else {
            ubo_.proj = camera_->getProjectionMatrix();
        }

        app_->newFrame(ubo_); // draw render & the frame to the screen
    }

    user_thread.request_stop();
    user_thread.join(); // wait for user_thread to finish
}

void tdl::ThreeDL::onKey(
    GLFWwindow* window,
    const int key,
    const int scancode,
    const int action,
    const int mods
) {
    const auto instance = static_cast<tdl::ThreeDL*>(glfwGetWindowUserPointer(window));

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    // create a key map
    instance->keys_[key] = action != GLFW_RELEASE;
}

void tdl::ThreeDL::onResize(
    GLFWwindow* window,
    const int width,
    const int height
) {
    const auto instance = static_cast<tdl::ThreeDL*>(glfwGetWindowUserPointer(window));

    // update width, height and tell vulkan to recreate the swapchain (resized_ = true)
    instance->app_->info_->width_ = width;
    instance->app_->info_->height_ = height;
    instance->app_->resized_ = true;
}
