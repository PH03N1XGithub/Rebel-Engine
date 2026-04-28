#include "Engine/Framework/EnginePch.h"
#include "Engine/Framework/BaseEngine.h"
#include "Engine/Framework/Window.h"
#include "Engine/Assets/AssetManager.h"
#include "Engine/Gameplay/Framework/GameMode.h"
#include "Engine/Physics/PhysicsModule.h"
#include "Engine/Scene/World.h"

// This creates the single definition that the linker needs
REBELENGINE_API BaseEngine* GEngine = nullptr;

DEFINE_LOG_CATEGORY(EventLog);
DEFINE_LOG_CATEGORY(EngineLog);

BaseEngine::BaseEngine()
{
    GEngine = this;
    SanitizeBootstrapOptions();
}

BaseEngine::~BaseEngine()
{
}

void BaseEngine::Run()
{
    if (Initialize())
    {
        MainLoop();
        Shutdown();
    }
}

bool BaseEngine::Initialize()
{
    SanitizeBootstrapOptions();

    if (m_BootstrapOptions.bCreateWindow)
    {
        m_Window = RMakeUnique<Window>(m_WindowSpecs);
        Window::GetEventDelegate().AddRaw(this, &BaseEngine::OnEngineEvent);
    }

    if (m_BootstrapOptions.bInitializeGraphics)
    {
        CHECK_MSG(m_Window, "Graphics initialization requires a valid window/context");
        const Bool status = gladLoadGLLoader(GLADloadproc(glfwGetProcAddress));
        CHECK(status);
    }

    m_EditorScene = new Scene();
    m_World = RMakeUnique<World>(m_EditorScene, &m_ModuleManager);
    m_World->SetGameMode(std::make_unique<GameMode>());
    SetActiveScene(m_EditorScene);

    OnInit();
    return true;
}

Scene* BaseEngine::GetActiveScene() const
{
    if (m_World)
        return m_World->GetScene();

    return m_ActiveScene;
}

void BaseEngine::SetActiveScene(Scene* scene)
{
    m_ActiveScene = scene;

    if (m_World)
        m_World->SetScene(scene);

    if (scene)
        scene->SetWorld(m_World.Get());

    if (AnimationModule* animationModule = m_ModuleManager.GetModule<AnimationModule>())
    {
        animationModule->SetSceneContext(scene);
    }
}

void BaseEngine::OnEngineEvent(const Event& e)
{
    // Forward to modules that care about events
    for (auto& module : m_ModuleManager.m_Modules)
    {
        module.Get()->OnEvent(e);
    }

    switch (e.Type)
    {
    case Event::Type::WindowClose:
        RB_LOG(EventLog, trace, "Window Close Event received, stopping engine");
        m_Running = false; // stop the main loop
        break;

    case Event::Type::WindowResize:
        RB_LOG(EventLog, trace, "Window resized to {}x{}", e.X, e.Y);
        if (m_Window)
            m_Window->Resize(static_cast<int>(e.X), static_cast<int>(e.Y));
        break;

    case Event::Type::KeyPressed:
        break;

    case Event::Type::MouseMoved:
        break;

    case Event::Type::MouseButtonPressed:
        break;

    case Event::Type::KeyReleased:
        break;

    case Event::Type::MouseButtonReleased:
        break;

    case Event::Type::MouseScrolled:
        break;
    }
}

void BaseEngine::MainLoop()
{
    Rebel::Core::Timer timer; // global timer for frame timing
    float lastTime = timer.Elapsed();

    while (m_Running)
    {
        ++m_FrameId;

        float currentTime = timer.Elapsed();
        float deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        if (m_Window)
            m_Window->OnUpdate();

        Tick(deltaTime);
        InputModule::ResetFrameState();

        if (m_Window)
            m_Window->SwapBuffers();
    }
}

void BaseEngine::Shutdown()
{
    OnShutdown();
}

void BaseEngine::Tick(float deltaTime)
{
    if (!m_World)
        return;

    m_World->Tick(deltaTime, IsPlaying(), m_FrameId);

    // Phase 5: render pipeline
    m_ModuleManager.TickModulesByType(TickType::Render, deltaTime);
    m_ModuleManager.TickModulesByType(TickType::PostRender, deltaTime);
}

void BaseEngine::OnInit()
{
    m_ModuleManager.RegisterModules();
    m_ModuleManager.InitModules();

    if (AnimationModule* animationModule = m_ModuleManager.GetModule<AnimationModule>())
    {
        animationModule->SetSceneContext(GetActiveScene());

        if (AssetManagerModule* assetModule = m_ModuleManager.GetModule<AssetManagerModule>())
            animationModule->SetAssetManagerContext(&assetModule->GetManager());
    }
}

void BaseEngine::OnShutdown()
{
    m_ModuleManager.ShutdownModules();
}


