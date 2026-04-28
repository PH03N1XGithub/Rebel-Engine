#pragma once
#include "Engine/Framework/BaseModule.h"
//#include "Engine/Physics/PhysicsSystem.h"

class PhysicsSystem;

class PhysicsModule : public IModule
{
	REFLECTABLE_CLASS(PhysicsModule, IModule)
public:
	PhysicsModule();
	~PhysicsModule() override;
	void Init() override;
	void Shutdown() override;
	void Tick(float deltaTime) override;
	void OnEvent(const Event& e) override{};
	PhysicsSystem* GetPhysicsSystem() const { return m_Physics; }
private:
	PhysicsSystem* m_Physics = nullptr;
};
REFLECT_CLASS(PhysicsModule, IModule)
END_REFLECT_CLASS(PhysicsModule)

