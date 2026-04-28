#include "Engine/Framework/EnginePch.h"
#include "Engine/Framework/Window.h"
#include <iostream>
#include <windows.h>
#include <GLFW/glfw3.h>
#include "GLFW/glfw3native.h"




OnEngineEvent Window::GEngineEvent;

Window::Window(const WindowProps& props)
{
	Init(props);
}

Window::~Window()
{
	Shutdown();
}


void Window::Init(const WindowProps& props)
{
	m_Data.Title = props.Title;
	m_Data.Width = props.Width;
	m_Data.Height = props.Height;
	m_Data.VSync = props.VSync;

	if (!glfwInit())
	{
		std::cerr << "Failed to initialize GLFW!\n";
		return;
	}
	glfwWindowHint(GLFW_DECORATED, GLFW_FALSE); // no OS title bar / border
	m_Window = glfwCreateWindow(props.Width, props.Height, props.Title.c_str(), nullptr, nullptr);
	if (!m_Window)
	{
		std::cerr << "Failed to create GLFW window!\n";
		glfwTerminate();
		return;
	}
	HWND hwnd = glfwGetWin32Window(m_Window);

	LONG style = GetWindowLong(hwnd, GWL_STYLE);

	// Keep the window undecorated. A thick frame reintroduces a native
	// top-edge line that conflicts with the custom ImGui chrome.
	style &= ~WS_CAPTION;
	style &= ~WS_SYSMENU;
	style &= ~WS_THICKFRAME;
	style &= ~WS_SIZEBOX;
	style |= WS_MINIMIZEBOX;
	style |= WS_MAXIMIZEBOX;

	SetWindowLong(hwnd, GWL_STYLE, style);

	// Apply the change
	SetWindowPos(hwnd, NULL,
				 0, 0, 0, 0,
				 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

	const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
	glfwSetWindowPos(m_Window,
		(mode->width  - 1280) / 2,
		(mode->height - 720)  / 2);

	glfwMakeContextCurrent(m_Window);
	SetVSync(props.VSync);

	SetupGLFWCallbacks();
}

void Window::Shutdown()
{
	glfwDestroyWindow(m_Window);
	glfwTerminate();
	m_Window = nullptr;
}

void Window::OnUpdate()
{
	glfwPollEvents();
}
void Window::SwapBuffers()
{
	glfwSwapBuffers(m_Window);
}

void Window::SetVSync(bool enabled)
{
	m_Data.VSync = enabled;
	glfwSwapInterval(enabled ? 1 : 0);
}

bool Window::IsVSync() const
{
	return m_Data.VSync;
}

void Window::SetCursorDisabled(bool disabled)
{
	if (!m_Window)
		return;

	m_Data.CursorDisabled = disabled;
	glfwSetInputMode(m_Window, GLFW_CURSOR, disabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);

	if (glfwRawMouseMotionSupported())
		glfwSetInputMode(m_Window, GLFW_RAW_MOUSE_MOTION, disabled ? GLFW_TRUE : GLFW_FALSE);
}

bool Window::IsCursorDisabled() const
{
	return m_Data.CursorDisabled;
}

void Window::Minimize()
{
	if (m_Window)
		glfwIconifyWindow(m_Window);
}

void Window::Maximize()
{
	if (m_Window)
		glfwMaximizeWindow(m_Window);
}

void Window::Restore()
{
	if (m_Window)
		glfwRestoreWindow(m_Window);
}

bool Window::IsMaximized() const
{
	return m_Window && glfwGetWindowAttrib(m_Window, GLFW_MAXIMIZED) == GLFW_TRUE;
}

void Window::StartDragMove()
{
	if (!m_Window)
		return;

	HWND hwnd = glfwGetWin32Window(m_Window);
	if (!hwnd)
		return;

	ReleaseCapture();
	SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
}

uint16 Window::GetWidth() const
{
	return m_Data.Width;
}

uint16 Window::GetHeight() const
{
	return m_Data.Height;
}

void* Window::GetNativeWindow() const
{
	return m_Window;
}

TArray<Rebel::Core::String> Window::ConsumeDroppedFiles()
{
	TArray<Rebel::Core::String> droppedFiles = m_PendingDroppedFiles;
	m_PendingDroppedFiles.Clear();
	return droppedFiles;
}

void Window::Resize(int width, int height)
{
	m_Data.Width = width;
	m_Data.Height = height;
}

void Window::SetupGLFWCallbacks()
{
	glfwSetWindowUserPointer(m_Window, this);

	glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* win)
	{
		Event e;
		e.Type = Event::Type::WindowClose;
		Window::GEngineEvent.Broadcast(e);
	});

	glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* win, int32 width, int32 height)
	{
		Event e;
		e.Type = Event::Type::WindowResize;
		e.X = width;
		e.Y = height;
		Window::GetEventDelegate().Broadcast(e);
	});

	glfwSetKeyCallback(m_Window, [](GLFWwindow* win, int32 key, int32 scancode, int32 action, int32 mods)
	{
		Event e;

		if (action == GLFW_PRESS)
			e.Type = Event::Type::KeyPressed;
		else if (action == GLFW_RELEASE)
			e.Type = Event::Type::KeyReleased;
		else if (action == GLFW_REPEAT)
			e.Type = Event::Type::KeyPressed; // treat repeat as hold

		e.Key = key;
		Window::GEngineEvent.Broadcast(e);
	});

	glfwSetCursorPosCallback(m_Window, [](GLFWwindow* win, double xpos, double ypos)
	{
		Event e;
		e.Type = Event::Type::MouseMoved;
		e.X = xpos;
		e.Y = ypos;
		Window::GEngineEvent.Broadcast(e);
	});

	glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* win, int32 button, int32 action, int32 mods)
	{
		Event e;
		e.Type = (action == GLFW_PRESS) ? Event::Type::MouseButtonPressed : Event::Type::MouseButtonReleased;
		e.Key = button;
		Window::GEngineEvent.Broadcast(e);
	});

	glfwSetScrollCallback(m_Window, [](GLFWwindow* win, double xoffset, double yoffset)
	{
		Event e;
		e.Type = Event::Type::MouseScrolled;
		e.ScrollX = static_cast<float>(xoffset);
		e.ScrollY = static_cast<float>(yoffset);
		Window::GEngineEvent.Broadcast(e);
	});

	glfwSetDropCallback(m_Window, [](GLFWwindow* win, int count, const char** paths)
	{
		Window* window = static_cast<Window*>(glfwGetWindowUserPointer(win));
		if (!window || !paths || count <= 0)
			return;

		for (int i = 0; i < count; ++i)
		{
			if (!paths[i])
				continue;

			window->m_PendingDroppedFiles.Add(paths[i]);
		}
	});
}

