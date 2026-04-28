#pragma once
#include "Engine/Framework/EnginePch.h"
#include "Engine/Framework/BaseModule.h"
#include "Engine/Framework/Window.h"
#include "Editor/EditorLayer.h"

#include <memory>

DEFINE_LOG_CATEGORY(ImGuiLog)

class ImGuiModule : public IModule
{
	REFLECTABLE_CLASS(ImGuiModule, IModule)
	ImGuiModule()
	{
		m_TickType = TickType::PostRender;
	};
	~ImGuiModule() override;

	void SetColorTheme();
	void Init() override;
	void BeginFrame();
	void EndFrame();
	void Tick(float dt) override;
	void OnEvent(const Event& e) override;
	void Shutdown() override;

private:
	std::unique_ptr<EditorLayer> m_EditorLayer;
	ImFont* m_BaseFont = nullptr;
	ImFont* m_BoldFont = nullptr;

};

REFLECT_CLASS(ImGuiModule, IModule)
END_REFLECT_CLASS(ImGuiModule)
