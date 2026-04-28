#pragma once

#include "Engine/Framework/BaseModule.h"

class Scene;
class AssetManager;

class AnimationModule : public IModule
{
    REFLECTABLE_CLASS(AnimationModule, IModule)

    AnimationModule();
    ~AnimationModule() override;

    void SetSceneContext(Scene* scene) { m_Scene = scene; }
    void SetAssetManagerContext(AssetManager* assetManager) { m_AssetManager = assetManager; }

private:
    void UpdateAnimations(float dt);

public:
    void OnEvent(const Event& e) override;
    void Init() override;
    void Tick(float deltaTime) override;
    void Shutdown() override;

private:
    Scene* m_Scene = nullptr;
    AssetManager* m_AssetManager = nullptr;
};
REFLECT_CLASS(AnimationModule, IModule)
END_REFLECT_CLASS(AnimationModule)

