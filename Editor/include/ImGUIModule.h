#pragma once
#include "EnginePch.h"
#include "Actor.h"
#include "BaseModule.h"
#include "Window.h"

DEFINE_LOG_CATEGORY(ImGuiLog)

class ImGuiModule : public IModule
{
	REFLECTABLE_CLASS(ImGuiModule, IModule)
	ImGuiModule()
	{
		E_TickType = TickType::PostRender;
	};
	~ImGuiModule() override;

	void SetColorTheme();
	void Init() override;
	void BeginFrame();
	void EndFrame();
	static void DrawPropertyUI(void* object, const Rebel::Core::Reflection::PropertyInfo& prop);
	static void DrawReflectedObjectUI(void* object, const Rebel::Core::Reflection::TypeInfo& type);
	void DrawComponentsForActor(Actor& actor);
	void Tick(float dt) override;
	void OnEvent(const Event& e) override;
	void Shutdown() override;

};

REFLECT_CLASS(ImGuiModule, IModule)
END_REFLECT_CLASS(ImGuiModule)
