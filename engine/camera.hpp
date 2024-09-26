#pragma once

#include <glm/glm.hpp>
#include <glm/ext/matrix_clip_space.hpp>

#include <memory>
#include <unordered_map>
#include <opencv2/core/types.hpp>

#include "types.hpp"

namespace tdl {
    /**
     * @breif Highest level camera class, represents a static camera, passed to a ThreeDL class through setCamera(...);
     *
     * Camera serves as the base for CameraController. The camera class is only useful for users that want a static
     * camera that does not move around the scene. When creating a custom CameraController this class should not be
     * inherited from.
    */
    class Camera {
        public:
            /**
             * @brief Create Camera, all params are set after construction other than perspective.
             *
             * @param perspective aspect ratio of the window / screen
             */
            explicit Camera (
                float perspective
            );

            /**
             * @breif Set the fov of the camera, fovs not in the valid range will result in a runtime error.
             *
             * @param fov field of view of the camera: 0 < fov < 180
            */
            void setFov (
                float fov
            );

            /**
             * @breif Sets the distance of the far clipping plane from the camera
             *
             * The distance of the far clipping plane can not be less than the distance of the near plane. To avoid
             * logic errors this is checked inside the setter and an invalid value will result in an std::runtime_error
             *
             * @param distance distance of the far plane from the camera
            */
            void setFarPlane (
                float distance
            );


            /**
             * @breif Sets the distance of the near clipping plane from the camera
             *
             * The distance of the near clipping plane can not be greater than the distance of the far plane. To avoid
             * logic errors this is checked inside the setter and an invalid value will result in an std::runtime_error
             *
             * @param distance distance of the near plane from the camera
            */
            void setNearPlane (
                float distance
            );

            /**
             * @breif Sets the aspect ratio of the projection plane.
             *
             * @param perspective aspect ratio of the screen or window
            */
            void setPerspective (
                float perspective
            );

            /**
             * @breif Getter for the projection matrix
             *
             * Sets has_changed_ to false to signify that projectoin matrix has not changed since the projmat was last
             * retrived
             *
             * @return projecton matrix (glm::mat4)
            */
            [[nodiscard]] glm::mat4 getProjectionMatrix(); // getter not const as changes has_changed_

            /**
             * @breif bool operator to check if the projection matrix has change d since it was last retrived.
            */
            [[nodiscard]] explicit operator bool() const { return has_changed_; }

            virtual ~Camera() = default;

            static constexpr auto type = Control::Independent; // specifies that this is not a controller to engine
        protected:
            struct CameraDetails {
                float near_ = 0.01f;
                float far_ = 10000.0f;
                float fov_ = 1.13446f; // 65 degrees
                float perspective_ = 16.0f/9.0f;
            } details_ = {};

            glm::vec3 forward_ {0.0f, 0.0f, 1.0f};
            glm::vec3 right_ {1.0f, 0.0f, 0.0f};

            glm::vec4 forward_w_ {0.0f, 0.0f, 1.0f, 0.0f};
            glm::vec4 right_w_ {1.0f, 0.0f, 0.0f, 0.0f};

            bool has_changed_ = true; // true as projmat hasn't been retrived yet

            glm::mat4 proj_mat_ {1.0f}; // initialise to identity matrix
    };

    /**
     * @breif Purely virtual interface to create custom camera controllers.
     *
     * An instance of this class can not be created, if a camera controller is needed a custom one can be created by
     * inheriting from this class and overloading key methods or the tdl::DefaultController class can be used.
    */
    class CameraController : public Camera {
        public:
            /**
             * @breif Calls parent class constructor (Camera)
             *
             * @param perspective aspect ratio of the screen or window
            */
            explicit CameraController (
                const float perspective
            ) : Camera  { perspective } {}

            /**
             * @breif Function that is called repeatedly by the engine.
             *
             * Can be overloaded to move camera.
             *
             * @param keys all the keys currently pressed
             * @param delta_t time since last call
            */
            virtual void tick (
                const std::unordered_map<int, bool>& keys,
                float delta_t
            ) = 0;

            /**
             * @breif gets the translation matrix of the camera
             *
             * @return glm::mat4
            */
            [[nodiscard]] glm::mat4 getCameraMatrix() const { return camera_mat_; }

            /**
             * @breif gets the rotation matrix of the camera
             *
             * @return glm::mat4
            */
            [[nodiscard]] glm::mat4 getRotationMatrix() const { return rotation_mat_; }

            ~CameraController() override = default;

            static constexpr auto type = Control::Controlled; // specifies that this is a controller to engine
        protected:
            /**
             * @breif called to log a key press
             *
             * Defines the behaviour for what to do when a given key is pressed
             *
             * @param key key code of key pressed
             * @param delta_t time since last call
            */
            virtual void keyPressed (
                int key,
                float delta_t
            ) = 0;

            glm::mat4 rotation_mat_ = glm::mat4(1.0f);
            glm::mat4 camera_mat_ = glm::mat4(1.0f);
    };

    /**
     * @breif Default controller provided by the ThreeDL engine
     *
     * Provides FPS style controls for the camera, this should only be used for simple projects or debugging as for most
     * appliucations a more custom solution will be required.
    */
    class DefaultController final : public CameraController {
        public:
            /**
             * @breif Initialises parent constructor
             *
             * @param perspective aspect ratio of the screen or window
            */
            explicit DefaultController (
                const float perspective
            ) : CameraController { perspective } {}

            /**
             * @breif Function that is called repeatedly by the engine.
             *
             * Can be overloaded to move camera.
             *
             * @param keys all the keys currently pressed
             * @param delta_t time since last call
            */
            void tick (
                const std::unordered_map<int, bool>& keys,
                float delta_t
            ) override;

            ~DefaultController() override = default;
        private:
            /**
             * @breif called to log a key press
             *
             * Overloaded in the default controller to mimic FPS style controls. WASD to move and QE to look left &
             * right.
             *
             * @param key key code of key pressed
             * @param delta_t time since last called
            */
            void keyPressed (
                int key,
                float delta_t
            ) override;
    };

    /**
     * Checks that the custom controller has been inherited from the interface and thus can be passed to the ThreeDL
     * class.
    */
    template<typename ControllerT>
    concept ValidController = std::derived_from<ControllerT, CameraController>;

    /**
     * Variadic template used to provide a more user friendly way to create a CameraController of any type that has
     * been inherited from the CameraController class.
    */
    template <typename ControllerT, typename... Args>
    requires ValidController<ControllerT>
    std::shared_ptr<ControllerT> make_camera (
        Args... args
    ) {
        return std::make_shared<ControllerT>(args...);
    }

    /**
     * Specialised template that makes it so that the user does not have to provide the type if they are creating a
     * camera instead of a controller.
    */
    template <typename... Args>
    std::shared_ptr<Camera> make_camera (
        Args... args
    ) {
        return std::make_shared<Camera>(args...);
    }
}
