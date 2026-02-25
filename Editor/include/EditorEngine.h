#pragma once
#include "ImGUIModule.h"
#include "BaseEngine.h"
#include "Camera.h"


class EditorEngine : public BaseEngine
{
public:
	~EditorEngine() override;
	bool Initialize() override;
	void Shutdown() override;
	void OnEngineEvent(const Event& event) override;

	void StartPlayInEditor();
	void StopPlayInEditor();


protected:
	void Tick(float deltaTime) override;
	void OnInit() override;
	void OnShutdown() override;
	void EnginePreTick(Float deltaTime) override;
	void EngineTick(Float deltaTime) override;
	void EnginePostTick(Float deltaTime) override;


	// --- NEW: Camera Instance ---
	Camera* m_EditorCamera = nullptr; 
};

