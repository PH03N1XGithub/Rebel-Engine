#pragma once
#include <glm/glm.hpp>
#include "EngineReflectionExtensions.h"
#include <glm/gtc/matrix_transform.hpp>

#include "CameraView.h"

// Defines several possible options for camera movement. Used as abstraction to stay away from window-system specific input methods
enum Camera_Movement {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT,
    UP,
    DOWN
};

// Default camera valuese
const Float YAW         = -90.0f;
const Float PITCH       =  0.0f;
const Float SPEED       =  5.0f;
const Float SENSITIVITY =  150.0f;
const Float FOV         =  60.0f;
const Float NEAR_PLANE  =  0.1f;
const Float FAR_PLANE   = 10000.0f;


class Camera {

    REFLECTABLE_CLASS(Camera,void)

    
public:
    /**
     * @brief Constructor that takes position vector.
     */
    Camera(Vector3 position = Vector3(0.0f, 0.0f, 0.0f), 
           Float yaw = YAW, Float pitch = PITCH);

    /**
     * @brief Calculates the view matrix using Euler angles and the lookAt matrix.
     * @return The 4x4 view matrix.
     */
    Mat4 GetViewMatrix() const;

    /**
     * @brief Calculates the projection matrix (Perspective).
     * @param aspectRatio The aspect ratio of the viewport (width / height).
     * @return The 4x4 projection matrix.
     */
    Mat4 GetProjectionMatrix(Float aspectRatio) const;

    /**
     * @brief Processes input received from any keyboard-like input system.
     * @param direction The direction of movement.
     * @param deltaTime The time difference between the last and current frame.
     */
    void ProcessKeyboard(Camera_Movement direction, Float deltaTime);

    /**
     * @brief Processes input received from a mouse movement event.
     * @param xoffset The horizontal offset of the mouse movement.
     * @param yoffset The vertical offset of the mouse movement.
     * @param constrainPitch Whether to limit the vertical rotation (pitch).
     */
    void ProcessMouseMovement(Float xoffset, Float yoffset, Bool constrainPitch = true);

    /**
     * @brief Processes input received from a mouse scroll-wheel event.
     * @param yoffset The vertical scroll offset.
     */
    void ProcessMouseScroll(Float yoffset);

    CameraView GetCameraView(Float aspect) const;

    // Camera Attributes
    Vector3 Position;
    Vector3 Front;
    Vector3 Up;
    Vector3 Right;
    Vector3 WorldUp = Vector3(0.0f, 0.0f, 1.0f);

    // Euler Angles
    Float Yaw;
    Float Pitch;

    // Camera options
    Float MovementSpeed;
    Float MouseSensitivity;
    Float Zoom; // This stores the FOV

    // Near/Far planes
    Float NearPlane;
    Float FarPlane;

    void Update(Float deltaTime);

private:
    /**
     * @brief Calculates the front, right, and up vectors from the camera's (updated) Euler Angles.
     */
    void updateCameraVectors();
};
using namespace Rebel::Core::Reflection;
REFLECT_CLASS(Camera, void)
    REFLECT_PROPERTY(Camera, Position, EPropertyFlags::Editable | EPropertyFlags::VisibleInEditor);
END_REFLECT_CLASS(Camera)

