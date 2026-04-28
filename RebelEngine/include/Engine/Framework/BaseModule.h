#pragma once

struct Event;

enum TickType
{
	PreSimulation,   // input prep, AI planning, etc.
	Physics,         // physics systems
	PostSimulation,  // camera, animation sync, cleanup
	Render,          // scene render
	PostRender       // UI overlay (ImGui)
};

class REBELENGINE_API IModule
{
	REFLECTABLE_CLASS(IModule, void)
public:
	virtual ~IModule() = default;
	virtual void Init() = 0;
	virtual void OnEvent(const Event& e){}
	virtual void Tick(float deltaTime) =0;
	virtual void Shutdown() =0;
	[[nodiscard]] virtual TickType GetTickType() const { return m_TickType; }
protected:
	TickType m_TickType = TickType::PostSimulation;
};
REFLECT_ABSTRACT_CLASS(IModule, void)
