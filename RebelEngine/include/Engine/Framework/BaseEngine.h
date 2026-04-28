#pragma once

#include "Engine/Rendering/Camera.h"
#include "Engine/Framework/Core.h"
#include "Engine/Framework/ModuleManager.h"
#include "glad/glad.h"
#include "Core/CoreTypes.h"
#include "Engine/Framework/Window.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Assets/AssetManagerModule.h"
#include "Engine/Rendering/RenderModule.h"
#include "Engine/Rendering/RenderModule.h"
#include "Engine/Input/InputModule.h"
#include "Engine/Physics/PhysicsModule.h"
#include "Engine/Animation/AnimationModule.h"

class World;

enum class EngineMode
{
    Editor,
    Runtime
};

struct EngineBootstrapOptions
{
    Bool bHeadless = false;
    Bool bCreateWindow = true;
    Bool bInitializeGraphics = true;
};

class REBELENGINE_API BaseEngine
{
public:
    BaseEngine();
    virtual ~BaseEngine();

    // Called by the application
    void Run();

    Window* GetWindow() const { return m_Window.Get(); }
    ModuleManager& GetModuleManager() { return m_ModuleManager; }
    uint64 GetFrameId() const { return m_FrameId; }

    void SetBootstrapOptions(const EngineBootstrapOptions& options)
    {
        m_BootstrapOptions = options;
        SanitizeBootstrapOptions();
    }
    const EngineBootstrapOptions& GetBootstrapOptions() const { return m_BootstrapOptions; }

    virtual CameraView GetActiveCamera(Float aspect) const
    {
        Scene* activeScene = GetActiveScene();

        if (m_Mode == EngineMode::Editor)
        {
            if (m_ActiveCamera)
                return m_ActiveCamera->GetCameraView(aspect);
            return CameraView{};
        }

        if (activeScene)
        {
            if (auto* cam = activeScene->FindPrimaryCamera())
                return cam->GetCameraView(aspect);
        }

        if (m_ActiveCamera)
            return m_ActiveCamera->GetCameraView(aspect);

        return CameraView{};
    }
    virtual bool ShouldProcessGameplayInput() const { return IsPlaying(); }

    Scene* GetActiveScene() const;
    World* GetWorld() const { return m_World.Get(); }

    EngineMode GetMode() const { return m_Mode; }
    bool IsPlaying() const { return m_Mode == EngineMode::Runtime; }

protected:
    // Initialize all core systems
    virtual Bool Initialize();
    // For derived classes (Editor / Runtime) to implement
    virtual void OnInit();
    // Main loop
    void MainLoop();
    // Fixed update / variable update
    virtual void Tick(Float deltaTime); // for game logic

    virtual void OnEngineEvent(const Event& event);

    void SetActiveCamera(Camera* camera) { m_ActiveCamera = camera; }
    void SetActiveScene(Scene* scene);

    // Shutdown all systems
    virtual void Shutdown();
    virtual void OnShutdown();

    RUniquePtr<Window> m_Window;
    Window::WindowProps m_WindowSpecs;

    ModuleManager m_ModuleManager;
    TArray<RUniquePtr<IModule>> m_Modules;

    Camera* m_ActiveCamera = nullptr;
    RUniquePtr<World> m_World;
    Scene* m_EditorScene = nullptr;
    Scene* m_RuntimeScene = nullptr;
    Scene* m_ActiveScene = nullptr;

    EngineMode m_Mode = EngineMode::Editor;

private:
    void SanitizeBootstrapOptions()
    {
        if (m_BootstrapOptions.bHeadless)
        {
            m_BootstrapOptions.bCreateWindow = false;
            m_BootstrapOptions.bInitializeGraphics = false;
        }

        if (m_BootstrapOptions.bInitializeGraphics)
            m_BootstrapOptions.bCreateWindow = true;
    }

private:
    Bool m_Running = true;
    EngineBootstrapOptions m_BootstrapOptions{};
    uint64 m_FrameId = 0;
};

extern REBELENGINE_API BaseEngine* GEngine;


