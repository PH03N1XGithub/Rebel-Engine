#pragma once
#include "Core.h"
#include "Core/Core.h"
#define GLFW_EXPOSE_NATIVE_WIN32 // TODO: move to Premake
#include <GLFW/glfw3.h>

struct Event
{
	enum class Type
	{
		WindowClose,
		WindowResize,
		KeyPressed,
		KeyReleased,
		MouseMoved,
		MouseButtonPressed,
		MouseButtonReleased,
		MouseScrolled
	};

	Type Type;
	uint16 Key;        // key code or mouse button
	uint16 X, Y;       // mouse coords or window size
	Float ScrollX, ScrollY;
	Bool Handled = false;
};

DECLARE_MULTICAST_DELEGATE(OnEngineEvent, const Event&);

class REBELENGINE_API Window
{
public:
	struct WindowProps
	{
		Rebel::Core::String Title = "RebelEngine";
		uint16 Width = 1280, Height = 720;
		Bool VSync = false;
	};

public:
	Window(const WindowProps& props);
	~Window();

	void OnUpdate();
	void SetVSync(bool enabled);
	bool IsVSync() const;

	uint16 GetWidth() const;
	uint16 GetHeight() const;

	void* GetNativeWindow() const;
	GLFWwindow* GetGLFWWindow() const { return m_Window; }

	void Resize(int width, int height);

	static OnEngineEvent& GetEventDelegate() { return GEngineEvent; }
	static OnEngineEvent GEngineEvent;

private:
	void Init(const WindowProps& props);
	void Shutdown();
	void SetupGLFWCallbacks();

private:
	GLFWwindow* m_Window = nullptr;

	struct WindowData
	{
		Rebel::Core::String Title;
		uint16 Width, Height;
		Bool VSync;
	} m_Data;
};
