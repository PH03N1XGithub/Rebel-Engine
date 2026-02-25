#pragma once
#include "BaseModule.h"
//#include "PhysicsSystem.h"

class PhysicsSystem;

class PhysicsModule : public IModule
{
	REFLECTABLE_CLASS(PhysicsModule, IModule)
	PhysicsModule();
	~PhysicsModule() override;
	void Init() override;
	void Shutdown() override;
	void Tick(float deltaTime) override;
	void OnEvent(const Event& e) override{};
	PhysicsSystem* GetPhysicsSystem() const { return m_Physics; }

	PhysicsSystem* m_Physics = nullptr;
private:
};
REFLECT_CLASS(PhysicsModule, IModule)
END_REFLECT_CLASS(PhysicsModule)
