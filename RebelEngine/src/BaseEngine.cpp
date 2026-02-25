#include "EnginePch.h"
#include "BaseEngine.h"
#include "Window.h"
#include "AssetManager/AssetManager.h"
#include "PhysicsModule.h"

// This creates the single definition that the linker needs
REBELENGINE_API BaseEngine* GEngine = nullptr;

DEFINE_LOG_CATEGORY(EventLog);
DEFINE_LOG_CATEGORY(EngineLog);

BaseEngine::BaseEngine()
{
	GEngine = this;
}

BaseEngine::~BaseEngine()
{
}

void BaseEngine::Run()
{
	if (Initialize())
	{
		MainLoop();
		Shutdown();
	}
}


bool BaseEngine::Initialize()
{
	m_Window = RMakeUnique<Window>(m_WindowSpecs);

	Window::GetEventDelegate().AddRaw(this, &BaseEngine::OnEngineEvent);
	
	const Bool status = gladLoadGLLoader(GLADloadproc(glfwGetProcAddress));
	CHECK(status);
	
    // The base class creates the window and GLFW context (made current in Window::Init)
    // Derived classes (EditorEngine) will initialize GL loader and ImGui after this returns

	m_EditorScene = new Scene();
	m_ActiveScene = m_EditorScene;


	OnInit();
    return true;
}


void BaseEngine::OnEngineEvent(const Event& e)
{
	// Forward to modules that care about events
	for (auto& module : m_ModuleManager.m_Modules)
	{
		module.Get()->OnEvent(e);
	}
	
	switch (e.Type)
	{
	case Event::Type::WindowClose:
		RB_LOG(EventLog, trace, "Window Close Event received, stopping engine");
		m_Running = false;  // stop the main loop
		break;

	case Event::Type::WindowResize:
		RB_LOG(EventLog, trace, "Window resized to {}x{}", e.X, e.Y);
		m_Window->Resize(e.X, e.Y);
		// Optionally resize renderer viewport here
		break;

	case Event::Type::KeyPressed:
		//RB_LOG(EventLog, trace, "Key pressed: {}", e.Key);
		break;

	case Event::Type::MouseMoved:
		//RB_LOG(EventLog, trace, "Mouse moved: {} {}", e.X, e.Y);
		break;
	case Event::Type::MouseButtonPressed:
		//RB_LOG(EventLog, trace, "Mouse button pressed: {}", e.Key);
	break;
	case Event::Type::KeyReleased:
		break;
	case Event::Type::MouseButtonReleased:
		//RB_LOG(EventLog, trace, "Mouse button released: {}", e.Key);
		break;
	case Event::Type::MouseScrolled:
		//RB_LOG(EventLog, trace, "Mouse Scrolled: {}", e.ScrollY);
		break;
	}
}

void BaseEngine::MainLoop()
{
	Rebel::Core::Timer timer;         // global timer for frame timing
	float lastTime = timer.Elapsed();
	
	while (m_Running)
	{
		//PROFILE_SCOPE("MainLoop Frame") // optional, for profiling each frame

		float currentTime = timer.Elapsed();
		float deltaTime = currentTime - lastTime;
		lastTime = currentTime;
		
		if (m_Window)
			m_Window->OnUpdate();
		
		Tick(deltaTime);
		InputModule::ResetFrameState();
	}
}

void BaseEngine::Shutdown()
{
	OnShutdown();
}

void BaseEngine::Tick(float deltaTime)
{
	m_ModuleManager.TickModulesByType(TickType::PreTick, deltaTime);
	EnginePreTick(deltaTime);
	
	m_ModuleManager.TickModulesByType(TickType::PrePhysics, deltaTime);
	m_ModuleManager.TickModulesByType(TickType::Physics, deltaTime);
	m_ModuleManager.TickModulesByType(TickType::PostPhysics, deltaTime);

	EngineTick(deltaTime);
	m_ModuleManager.TickModulesByType(TickType::Tick, deltaTime);

	EnginePostTick(deltaTime);
	m_ModuleManager.TickModulesByType(TickType::PostTick, deltaTime);

	m_ModuleManager.TickModulesByType(TickType::PreRender, deltaTime);
	m_ModuleManager.TickModulesByType(TickType::Render, deltaTime);
	m_ModuleManager.TickModulesByType(TickType::PostRender, deltaTime);
	
}

void BaseEngine::EnginePreTick(Float deltaTime)
{
}

void BaseEngine::EngineTick(Float deltaTime)
{
	if (IsPlaying())
	{
		m_ActiveScene->Tick(deltaTime);
	}
	else
	{
		m_ActiveScene->UpdateTransforms();
		m_ActiveScene->FlushPendingActorDestroy();
	}
}

void BaseEngine::EnginePostTick(Float deltaTime)
{
}

void BaseEngine::OnInit()
{
	m_ModuleManager.RegisterModules();
	m_ModuleManager.InitModules();
}


void BaseEngine::OnShutdown()
{
	m_ModuleManager.ShutdownModules();
}
