#include "EnginePch.h"
#include "Window.h"
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

	// remove the standard title bar & system menu:
	style &= ~WS_CAPTION;
	style &= ~WS_SYSMENU;
	style &= ~WS_MAXIMIZEBOX;

	// keep/enable resize border:
	/*style |= WS_THICKFRAME;
	style |= WS_SIZEBOX;
	style |= WS_MINIMIZEBOX;*/

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
		e.X = static_cast<int>(xpos);
		e.Y = static_cast<int>(ypos);
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
}
