#include "EnginePch.h"
#include "Camera.h"
#include <algorithm> // For std::clamp or std::min/max

#include "BaseEngine.h"
#include "InputModule.h"

// Helper function for degrees to radians (already in FMath/FMath.hpp, but often useful)
#ifndef GLM_FORCE_RADIANS
#define GLM_FORCE_RADIANS
#endif

Camera::Camera(const Vector3 position, const Float yaw, const Float pitch) 
    : MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY), 
      Zoom(FOV), NearPlane(NEAR_PLANE), FarPlane(FAR_PLANE)
{
    Position = position;
    Yaw = yaw;
    Pitch = pitch;
    updateCameraVectors();
}

Mat4 Camera::GetViewMatrix() const {
    // Returns the view transformation matrix
    return FMath::lookAt(Position, Position + Front, Up);
}

Mat4 Camera::GetProjectionMatrix(Float aspectRatio) const {
    // Returns the perspective projection matrix
    return FMath::perspective(FMath::radians(Zoom), aspectRatio, NearPlane, FarPlane);
}

void Camera::ProcessKeyboard(Camera_Movement direction, Float deltaTime) {
    Float velocity = MovementSpeed * deltaTime;
    if (direction == FORWARD)
        Position += Front * velocity;
    if (direction == BACKWARD)
        Position -= Front * velocity;
    if (direction == LEFT)
        Position -= Right * velocity;
    if (direction == RIGHT)
        Position += Right * velocity;
    if (direction == UP)
        Position += WorldUp * velocity;
    if (direction == DOWN)
        Position -= WorldUp * velocity;
}

void Camera::ProcessMouseMovement(Float xoffset, Float yoffset, Bool constrainPitch) {
    xoffset *= MouseSensitivity;
    yoffset *= MouseSensitivity;

    Yaw -= xoffset;
    Pitch += yoffset;

    // Make sure that when pitch is out of bounds, screen doesn't get flipped
    if (constrainPitch) {
        Pitch = FMath::clamp(Pitch, -89.0f, 89.0f);
    }

    // Update Front, Right and Up Vectors using the updated Euler angles
    updateCameraVectors();
}

void Camera::ProcessMouseScroll(Float yoffset) {
    Zoom -= yoffset; // Increase/decrease FOV
    // Clamp FOV between 1.0f and 90.0f
    Zoom = FMath::clamp(Zoom,1.0f, 90.0f);
}

CameraView Camera::GetCameraView(Float aspect) const
{
    // --- 1. Calculate forward direction from yaw / pitch ---
    const Float yawRad   = glm::radians(Yaw);
    const Float pitchRad = glm::radians(Pitch);

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
        glm::radians(FOV),
        aspect,
        NearPlane,
        FarPlane
    );
    view.FOV = FOV;

    return view;
}

void Camera::Update(Float deltaTime)
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

	bool rightMouseHeld = InputModule::IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);

	if (rightMouseHeld)
	{
		glfwSetInputMode(static_cast<GLFWwindow*>(GEngine->GetWindow()->GetNativeWindow()), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

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
		if (InputModule::IsKeyPressed(KEY_LSHIFT))
			MovementSpeed = 10.f;
		if (!InputModule::IsKeyPressed(KEY_LSHIFT))
			MovementSpeed = 5.f;
		if (InputModule::IsKeyPressed(GLFW_KEY_Q)) ProcessKeyboard(DOWN, deltaTime);
		if (InputModule::IsKeyPressed(GLFW_KEY_E)) ProcessKeyboard(UP, deltaTime);

		// --- Rotation ---
		Double currentX;
		Double currentY;
		glfwGetCursorPos(static_cast<GLFWwindow*>(GEngine->GetWindow()->GetNativeWindow()), &currentX, &currentY);

		float deltaX = (static_cast<Float>(currentX) - lastMouseX) * 0.002f;
		float deltaY = (lastMouseY - static_cast<Float>(currentY)) * 0.002f;

		if (deltaX != 0.0f || deltaY != 0.0f)
			ProcessMouseMovement(deltaX, deltaY);

		lastMouseX = static_cast<Float>(currentX);
		lastMouseY = static_cast<Float>(currentY);

		// --- Zoom ---
		float scrollY = InputModule::GetScrollY();
		if (scrollY != 0.0f)
			ProcessMouseScroll(scrollY);
	}
	else
	{
		glfwSetInputMode(static_cast<GLFWwindow*>(GEngine->GetWindow()->GetNativeWindow()), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
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

    const float radYaw   = FMath::radians(Yaw);
    const float radPitch = FMath::radians(Pitch);

    // Unreal-style forward vector
    Vector3 forward;
    forward.x =  cos(radPitch) * cos(radYaw);   // Forward X
    forward.y =  cos(radPitch) * sin(radYaw);   // Forward Y
    forward.z =  sin(radPitch);                 // Forward Z

    Front = FMath::normalize(forward);

    // Left-handed Unreal system:
    Right = FMath::normalize(FMath::cross(Front, Vector3(0,0,1))); // +Z up
    Up = FMath::normalize(FMath::cross(Right, Front));


}

