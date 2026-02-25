#pragma once

#include "Camera.h"
#include "Core.h"
#include "ModuleManager.h"
#include "glad/glad.h"
#include "Core/CoreTypes.h"
#include "Window.h"
#include "Scene.h"
#include "AssetManager/AssetManagerModule.h"
#include "RenderModule.h"
#include "RenderModule.h"
#include "InputModule.h"
#include "PhysicsModule.h"
#include "Animation/AnimationModule.h"


enum class EngineMode
{
	Editor,
	Runtime
};

class REBELENGINE_API BaseEngine
{
public:
	BaseEngine();
	virtual ~BaseEngine();
	
	// Called by the application
	void Run();
	
	Window* GetWindow() const { return m_Window.Get(); }
	ModuleManager& GetModuleManager() { return m_ModuleManager; }
	CameraView GetActiveCamera(Float aspect) const
	{
		if (m_Mode == EngineMode::Editor)
			return m_ActiveCamera->GetCameraView(aspect);

		if (auto* cam = m_ActiveScene->FindPrimaryCamera())
			return cam->GetCameraView(aspect);

		return m_ActiveCamera->GetCameraView(aspect);
	}
	
	Scene* GetActiveScene() const { return m_ActiveScene; }

	EngineMode GetMode() const { return m_Mode; }
	bool IsPlaying() const { return m_Mode == EngineMode::Runtime; }

protected:
	// Initialize all core systems
	virtual Bool Initialize();
	// For derived classes (Editor / Runtime) to implement
	virtual void OnInit();
	// Main loop
	void MainLoop();
	// Fixed update / variable update
	virtual void Tick(Float deltaTime);     // for game logic
	virtual void EnginePreTick(Float deltaTime);
	virtual void EngineTick(Float deltaTime);
	virtual void EnginePostTick(Float deltaTime);

	virtual void OnEngineEvent(const Event& event);
	
	void SetActiveCamera(Camera* camera) { m_ActiveCamera = camera; }
	void SetActiveScene(Scene* scene) { m_ActiveScene = scene; }
	
	// Shutdown all systems
	virtual void Shutdown();
	virtual void OnShutdown();
	
	RUniquePtr<Window>			m_Window;
	Window::WindowProps			m_WindowSpecs;

	ModuleManager				m_ModuleManager;
	TArray<RUniquePtr<IModule>> m_Modules;
	
	Camera*						m_ActiveCamera = nullptr;
	Scene* 						m_EditorScene  = nullptr;
	Scene* 						m_RuntimeScene = nullptr;
	Scene* 						m_ActiveScene  = nullptr;


	EngineMode m_Mode = EngineMode::Editor;

private:
	Bool						m_Running = true;
};

extern REBELENGINE_API BaseEngine* GEngine;

