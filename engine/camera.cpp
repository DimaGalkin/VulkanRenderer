#include "camera.hpp"

#include <GLFW/glfw3.h>
#include <glm/ext/matrix_transform.hpp>
#include <stdexcept>

tdl::Camera::Camera(
    const float perspective = 16.0f/9.0f
) : details_ {
        .perspective_ = perspective
    },
    proj_mat_ {
        glm::perspective (
            details_.fov_,
            perspective,
            details_.near_, details_.far_
        )
    }
{}


void tdl::Camera::setFov(
    const float fov
) {
    if (fov > 180) {
        throw std::invalid_argument(
            "Error 020: Fov of camera can not be set to " + std::to_string(fov) + ". Fov has to be < 180"
        );
    }

    // negative fov is not physically possible
    if (fov < 0) {
        throw std::invalid_argument(
            "Error 021: Fov of camera can not be set to " + std::to_string(fov) + ". Fov has to be > 0"
        );
    }

    details_.fov_ = fov;

    proj_mat_ = glm::perspective(
        details_.fov_,
        details_.perspective_,
        details_.near_,
        details_.far_
    );

    // tell renderer that the projection matrix has changed and the UBO has to be updated.
    has_changed_ = true;
}

void tdl::Camera::setFarPlane(
    const float distance
) {
    // if near is beind far then all objects will be clipped out of view
    if (distance < details_.near_) {
        throw std::invalid_argument(
            "Error 022: Near plane of camera can not be set to " + std::to_string(distance) + ". The far plane can" +
            " not be in front of the near plane."
        );
    }

    details_.far_ = distance;

    proj_mat_ = glm::perspective(
        details_.fov_,
        details_.perspective_,
        details_.near_,
        details_.far_
    );

    // tell renderer that the projection matrix has changed and the UBO has to be updated.
    has_changed_ = true;
}

void tdl::Camera::setNearPlane(
    const float distance
) {
    // if near is beind far then all objects will be clipped out of view
    if (distance > details_.far_) {
        throw std::invalid_argument(
            "Error 023: Far plane of camera can not be set to " + std::to_string(distance) + ". The near plane can" +
            " not be behind the far plane."
        );
    }

    details_.near_ = distance;

    proj_mat_ = glm::perspective(
        details_.fov_,
        details_.perspective_,
        details_.near_,
        details_.far_
    );

    // tell renderer that the projection matrix has changed and the UBO has to be updated.
    has_changed_ = true;
}

void tdl::Camera::setPerspective(
    const float perspective
) {
    details_.perspective_ = perspective;
    has_changed_ = true;
}

glm::mat4 tdl::Camera::getProjectionMatrix() {
    has_changed_ = false; // matrix has not changed since last retrived
    return proj_mat_;
}


void tdl::DefaultController::tick(
    const std::unordered_map<int, bool>& keys,
    const double delta_t
) {
    for (const auto& [key, pressed] : keys) {
        if (pressed) keyPressed(key, delta_t); // if a key is currently pressed then log it.
    }
}

void tdl::DefaultController::keyPressed(
    const int key,
    const double delta_t
) {
    switch (key) {
        // forward
        case GLFW_KEY_W:
            camera_mat_ = glm::translate(camera_mat_, {0, 0, 1 * delta_t});
            break;
        // backward
        case GLFW_KEY_S:
            camera_mat_ = glm::translate(camera_mat_, {0, 0, -1 * delta_t});
            break;
        // left
        case GLFW_KEY_A:
            camera_mat_ = glm::translate(camera_mat_, {1 * delta_t, 0, 0});
            break;
        // right
        case GLFW_KEY_D:
            camera_mat_ = glm::translate(camera_mat_, {-1 * delta_t, 0, 0});
            break;
        // rotate left
        case GLFW_KEY_Q:
            rotation_mat_ = glm::rotate(rotation_mat_, -static_cast<float>(delta_t), {0, 1, 0});
            break;
        // rotate right
        case GLFW_KEY_E:
            rotation_mat_ = glm::rotate(rotation_mat_, static_cast<float>(delta_t), {0, 1, 0});
            break;
        default:
            break;
    }
}

