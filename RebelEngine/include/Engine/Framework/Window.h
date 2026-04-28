#pragma once
#include "Engine/Framework/Core.h"
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
	int32 Key = 0;     // key code or mouse button
	double X = 0.0;    // mouse coords or window size
	double Y = 0.0;
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
	void SwapBuffers();
	void SetVSync(bool enabled);
	bool IsVSync() const;
	void SetCursorDisabled(bool disabled);
	bool IsCursorDisabled() const;
	void Minimize();
	void Maximize();
	void Restore();
	bool IsMaximized() const;
	void StartDragMove();

	uint16 GetWidth() const;
	uint16 GetHeight() const;

	void* GetNativeWindow() const;
	GLFWwindow* GetGLFWWindow() const { return m_Window; }
	TArray<Rebel::Core::String> ConsumeDroppedFiles();

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
		Bool CursorDisabled = false;
	} m_Data;

	TArray<Rebel::Core::String> m_PendingDroppedFiles;
};

