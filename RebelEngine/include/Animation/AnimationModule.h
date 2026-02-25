#pragma once

#include "BaseModule.h"

class Scene;

class AnimationModule : public IModule
{
    REFLECTABLE_CLASS(AnimationModule, IModule)

    AnimationModule();
    ~AnimationModule() override;

    void UpdateAnimations(Scene* scene, float dt);

    void OnEvent(const Event& e) override;
    void Init() override;
    void Tick(float deltaTime) override;
    void Shutdown() override;
};
REFLECT_CLASS(AnimationModule, IModule)
END_REFLECT_CLASS(AnimationModule)
