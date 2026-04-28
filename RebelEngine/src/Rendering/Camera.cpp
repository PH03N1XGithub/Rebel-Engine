#include "Engine/Framework/EnginePch.h"
#include "Engine/Rendering/Camera.h"
#include <algorithm> // For std::clamp or std::min/max

#include "Engine/Input/InputModule.h"
#include "Engine/Framework/Window.h"

// Helper function for degrees to radians (already in FMath/FMath.hpp, but often useful)
#ifndef GLM_FORCE_RADIANS
#define GLM_FORCE_RADIANS
#endif

namespace
{
constexpr float kCameraSpeedStep = 0.5f;

float SnapCameraSpeed(const float speed)
{
    return FMath::max(kCameraSpeedStep, glm::round(speed / kCameraSpeedStep) * kCameraSpeedStep);
}
}

Camera::Camera(const Vector3 position, const Float yaw, const Float pitch) 
    : m_MovementSpeed(SPEED), m_MouseSensitivity(SENSITIVITY), 
      m_Zoom(FOV), m_NearPlane(NEAR_PLANE), m_FarPlane(FAR_PLANE)
{
    Position = position;
    m_Yaw = yaw;
    m_Pitch = pitch;
    updateCameraVectors();
}

Mat4 Camera::GetViewMatrix() const {
    // Returns the view transformation matrix
    return FMath::lookAt(Position, Position + m_Front, m_Up);
}

Mat4 Camera::GetProjectionMatrix(Float aspectRatio) const {
    // Returns the perspective projection matrix
    return FMath::perspective(FMath::radians(m_Zoom), aspectRatio, m_NearPlane, m_FarPlane);
}

void Camera::ProcessKeyboard(Camera_Movement direction, Float deltaTime) {
    Float velocity = m_MovementSpeed * deltaTime;
    if (direction == FORWARD)
        Position += m_Front * velocity;
    if (direction == BACKWARD)
        Position -= m_Front * velocity;
    if (direction == LEFT)
        Position -= m_Right * velocity;
    if (direction == RIGHT)
        Position += m_Right * velocity;
    if (direction == UP)
        Position += m_WorldUp * velocity;
    if (direction == DOWN)
        Position -= m_WorldUp * velocity;
}

void Camera::ProcessMouseMovement(Float xoffset, Float yoffset, Bool constrainPitch) {
    xoffset *= m_MouseSensitivity;
    yoffset *= m_MouseSensitivity;

    m_Yaw -= xoffset;
    m_Pitch += yoffset;

    // Make sure that when pitch is out of bounds, screen doesn't get flipped
    if (constrainPitch) {
        m_Pitch = FMath::clamp(m_Pitch, -89.0f, 89.0f);
    }

    // Update Front, Right and Up Vectors using the updated Euler angles
    updateCameraVectors();
}

void Camera::ProcessMouseScroll(Float yoffset) {
    m_Zoom -= yoffset; // Increase/decrease FOV
    // Clamp FOV between 1.0f and 90.0f
    m_Zoom = FMath::clamp(m_Zoom,1.0f, 90.0f);
}

void Camera::SetRotation(const Float yaw, const Float pitch)
{
    m_Yaw = yaw;
    m_Pitch = FMath::clamp(pitch, -89.0f, 89.0f);
    updateCameraVectors();
}

void Camera::SetZoom(const Float zoom)
{
    m_Zoom = FMath::clamp(zoom, 1.0f, 90.0f);
}

void Camera::SetMovementSpeed(const Float speed)
{
    m_MovementSpeed = SnapCameraSpeed(speed);
}

CameraView Camera::GetCameraView(Float aspect) const
{
    // --- 1. Calculate forward direction from yaw / pitch ---
    const Float yawRad   = glm::radians(m_Yaw);
    const Float pitchRad = glm::radians(m_Pitch);

    Vector3 front;
    front.x = cos(yawRad) * cos(pitchRad);
    front.y = sin(yawRad) * cos(pitchRad);
    front.z = sin(pitchRad);
    front   = glm::normalize(front);

    const Vector3 up = Vector3(0.0f, 0.0f, 1.0f); // Z-up

    // --- 2. Build CameraView ---
    CameraView view;
    view.Position   = Position;
    view.View       = glm::lookAt(Position, Position + front, up);
    view.Projection = glm::perspective(
        glm::radians(m_Zoom),
        aspect,
        m_NearPlane,
        m_FarPlane
    );
    view.FOV = m_Zoom;

    return view;
}

void Camera::Update(Float deltaTime, Window* window)
{
    #define KEY_W 87
#define KEY_S 83
#define KEY_A 65
#define KEY_D 68
#define KEY_SPACE 32
#define KEY_LSHIFT 340
#define MOUSE_BUTTON_RIGHT 1

	static bool wasRightMouseHeld = false;
	static float lastMouseX = 0.0f;
	static float lastMouseY = 0.0f;
	GLFWwindow* nativeWindow = window
		? static_cast<GLFWwindow*>(window->GetNativeWindow())
		: nullptr;

	if (!nativeWindow)
		return;

	bool rightMouseHeld = InputModule::IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
	
	

	if (rightMouseHeld)
	{
		glfwSetInputMode(nativeWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		window->SetCursorDisabled(true);

		// Reset last mouse position when RMB is first pressed
		if (!wasRightMouseHeld)
		{
			lastMouseX = InputModule::GetMouseX();
			lastMouseY = InputModule::GetMouseY();
		}

		// --- Movement ---
		if (InputModule::IsKeyPressed(KEY_W)) ProcessKeyboard(FORWARD, deltaTime);
		if (InputModule::IsKeyPressed(KEY_S)) ProcessKeyboard(BACKWARD, deltaTime);
		if (InputModule::IsKeyPressed(KEY_A)) ProcessKeyboard(LEFT, deltaTime);
		if (InputModule::IsKeyPressed(KEY_D)) ProcessKeyboard(RIGHT, deltaTime);
		if (InputModule::IsKeyPressed(KEY_SPACE)) ProcessKeyboard(UP, deltaTime);
        const float activeMoveSpeed = InputModule::IsKeyPressed(KEY_LSHIFT)
            ? m_MovementSpeed * 2.0f
            : m_MovementSpeed;
        const float previousMoveSpeed = m_MovementSpeed;
        m_MovementSpeed = activeMoveSpeed;
        if (InputModule::IsKeyPressed(GLFW_KEY_Q)) ProcessKeyboard(DOWN, deltaTime);
        if (InputModule::IsKeyPressed(GLFW_KEY_E)) ProcessKeyboard(UP, deltaTime);
        m_MovementSpeed = previousMoveSpeed;

		// --- Rotation ---
		Double currentX;
		Double currentY;
		glfwGetCursorPos(nativeWindow, &currentX, &currentY);

		float deltaX = (static_cast<Float>(currentX) - lastMouseX) * 0.002f;
		float deltaY = (lastMouseY - static_cast<Float>(currentY)) * 0.002f;

		if (deltaX != 0.0f || deltaY != 0.0f)
			ProcessMouseMovement(deltaX, deltaY);

		lastMouseX = static_cast<Float>(currentX);
		lastMouseY = static_cast<Float>(currentY);

        // --- Editor wheel adjusts fly speed, not FOV ---
        float scrollY = InputModule::GetScrollY();
        if (scrollY != 0.0f)
        {
            SetMovementSpeed(m_MovementSpeed + scrollY * kCameraSpeedStep);
        }
	}
	else
	{
		glfwSetInputMode(nativeWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		window->SetCursorDisabled(false);
	}

	wasRightMouseHeld = rightMouseHeld;
}

void Camera::updateCameraVectors()
{
    //
    // Unreal coordinate system:
    //   +X = Forward
    //   +Y = Right
    //   +Z = Up
    //
    // Unreal rotations:
    //   Yaw   = rotation around Z
    //   Pitch = rotation around Y
    //   Roll  = rotation around X (not used)
    //

    const float radYaw   = FMath::radians(m_Yaw);
    const float radPitch = FMath::radians(m_Pitch);

    // Unreal-style forward vector
    Vector3 forward;
    forward.x =  cos(radPitch) * cos(radYaw);   // Forward X
    forward.y =  cos(radPitch) * sin(radYaw);   // Forward Y
    forward.z =  sin(radPitch);                 // Forward Z

    m_Front = FMath::normalize(forward);

    // Left-handed Unreal system:
    m_Right = FMath::normalize(FMath::cross(m_Front, Vector3(0,0,1))); // +Z up
    m_Up = FMath::normalize(FMath::cross(m_Right, m_Front));


}


