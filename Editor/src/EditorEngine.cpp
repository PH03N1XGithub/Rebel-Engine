#include "EnginePch.h"
#include "ImGUIModule.h"
#include "EditorEngine.h"
#include "imgui.h"
#include "PhysicsModule.h"
#include "PhysicsSystem.h"
#include "ThirdParty/imgui_impl_opengl3.h"


DEFINE_LOG_CATEGORY(EditorLog);

EditorEngine::~EditorEngine()
{
}


bool EditorEngine::Initialize()
{
	Window::WindowProps props;
	props.Width = 1280;
	props.Height = 720;
	props.Title = "Rebel Editor";

	m_WindowSpecs = props;
	
	if (!BaseEngine::Initialize())
		return false;
	

	m_EditorCamera = new Camera(glm::vec3(0.0f, 5.0f, 3.0f));
	SetActiveCamera(m_EditorCamera);
	
	RB_LOG(EditorLog, info, "EditorEngine initialized successfully!");
	
	return true;
}


void EditorEngine::Shutdown()
{
	BaseEngine::Shutdown();
}

void EditorEngine::OnEngineEvent(const Event& e)
{
	BaseEngine::OnEngineEvent(e);
}

void EditorEngine::StartPlayInEditor()
{
	m_RuntimeScene = new Scene();

    m_EditorScene->Serialize("TempPIE.Ryml");
    m_RuntimeScene->Deserialize("TempPIE.Ryml");

    m_RuntimeScene->BeginPlay();

    m_Mode = EngineMode::Runtime;
    m_ActiveScene = m_RuntimeScene;

	//GetModuleManager().GetModule<PhysicsModule>()->Shutdown();
	GetModuleManager().GetModule<PhysicsModule>()->Init();
	
}

void EditorEngine::StopPlayInEditor()
{
	delete m_RuntimeScene;
	m_RuntimeScene = nullptr;

	m_Mode = EngineMode::Editor;
	m_ActiveScene = m_EditorScene;
	GetModuleManager().GetModule<PhysicsModule>()->m_Physics->Shutdown();
}

void EditorEngine::Tick(float deltaTime)
{
	
	if (!IsPlaying())
	{
		m_EditorCamera->Update(deltaTime);
	}
	else
	{
		
	}
	BaseEngine::Tick(deltaTime);
}

void EditorEngine::OnInit()
{
	BaseEngine::OnInit();
}

void EditorEngine::OnShutdown()
{
	BaseEngine::OnShutdown();
}

void EditorEngine::EnginePreTick(Float deltaTime)
{
	BaseEngine::EnginePreTick(deltaTime);
}

void EditorEngine::EngineTick(Float deltaTime)
{
	BaseEngine::EngineTick(deltaTime);
}

void EditorEngine::EnginePostTick(Float deltaTime)
{
	BaseEngine::EnginePostTick(deltaTime);
}
