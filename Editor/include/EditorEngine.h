#pragma once
#include "ImGUIModule.h"
#include "Engine/Framework/BaseEngine.h"
#include "Engine/Rendering/Camera.h"


class EditorEngine : public BaseEngine
{
public:
	~EditorEngine() override;
	CameraView GetActiveCamera(Float aspect) const override;
	bool ShouldProcessGameplayInput() const override;
	void SetEditorCameraCaptured(bool captured);
	bool IsEditorCameraCaptured() const { return m_bExternalEditorCameraCaptured; }
	const String& GetCurrentScenePath() const { return m_CurrentScenePath; }
	Camera* GetEditorCamera() const { return m_EditorCamera; }

	void StartPlayInEditor();
	void StopPlayInEditor();
	bool SaveEditorScene(const String& path = "");
	bool LoadEditorScene(const String& path);

protected:
	bool Initialize() override;
	void Shutdown() override;
	void OnEngineEvent(const Event& event) override;
	void Tick(float deltaTime) override;
	void OnInit() override;
	void OnShutdown() override;

private:
	void SetPIECursorCaptured(bool captured);
	void TogglePIECursorCaptured();
	
	// --- NEW: Camera Instance ---
	Camera* m_EditorCamera = nullptr; 
	bool m_bPIECursorCaptured = false;
	bool m_bExternalEditorCameraCaptured = false;
	String m_CurrentScenePath = "scene.Ryml";
};

