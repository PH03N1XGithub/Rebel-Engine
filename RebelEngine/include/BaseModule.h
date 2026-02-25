#pragma once

struct Event;

enum TickType
{
	PreRender, Render, PostRender,
	PreTick, Tick, PostTick,
	PrePhysics, Physics, PostPhysics,
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
	[[nodiscard]] virtual TickType GetTickType() const { return E_TickType; }
protected:
	TickType E_TickType = TickType::Tick;
};
REFLECT_ABSTRACT_CLASS(IModule, void)
