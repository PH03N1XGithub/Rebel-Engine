#pragma once
#include <glm/glm.hpp>
#include "Engine/Framework/EngineReflectionExtensions.h"
#include <glm/gtc/matrix_transform.hpp>

#include "Engine/Rendering/CameraView.h"

class Window;

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
const Float YAW         = 0.0f;
const Float PITCH       =  0.0f;
const Float SPEED       =  5.0f;
const Float SENSITIVITY =  150.0f;
const Float FOV         =  90.0f;
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

    const Vector3& GetPosition() const { return Position; }
    void SetPosition(const Vector3& position) { Position = position; }
    void SetRotation(Float yaw, Float pitch);
    void SetZoom(Float zoom);
    void SetMovementSpeed(Float speed);
    Float GetNearPlane() const { return m_NearPlane; }
    Float GetFarPlane() const { return m_FarPlane; }
    Float GetZoom() const { return m_Zoom; }
    Float GetMovementSpeed() const { return m_MovementSpeed; }

    void Update(Float deltaTime, Window* window);

private:
    /**
     * @brief Calculates the front, right, and up vectors from the camera's (updated) Euler Angles.
     */
    void updateCameraVectors();

private:
    // Kept for reflection/editor compatibility.
    Vector3 Position;

    Vector3 m_Front;
    Vector3 m_Up;
    Vector3 m_Right;
    Vector3 m_WorldUp = Vector3(0.0f, 0.0f, 1.0f);

    Float m_Yaw;
    Float m_Pitch;

    Float m_MovementSpeed;
    Float m_MouseSensitivity;
    Float m_Zoom; // stores FOV

    Float m_NearPlane;
    Float m_FarPlane;
};
using namespace Rebel::Core::Reflection;
REFLECT_CLASS(Camera, void)
    REFLECT_PROPERTY(Camera, Position, EPropertyFlags::Editable | EPropertyFlags::VisibleInEditor);
END_REFLECT_CLASS(Camera)


