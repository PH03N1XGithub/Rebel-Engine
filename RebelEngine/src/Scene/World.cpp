#include "Engine/Framework/EnginePch.h"
#include "Engine/Scene/World.h"

#include "Engine/Gameplay/Framework/GameMode.h"
#include "Engine/Framework/BaseEngine.h"
#include "Engine/Framework/ModuleManager.h"
#include "Engine/Physics/PhysicsModule.h"
#include "Engine/Physics/PhysicsSystem.h"
#include "Engine/Gameplay/Framework/PlayerController.h"
#include "Engine/Scene/Scene.h"
#include <algorithm>

World::World(Scene* scene, ModuleManager* moduleManager)
    : m_Scene(scene)
    , m_ModuleManager(moduleManager)
{
}

void World::SetScene(Scene* scene)
{
    m_Scene = scene;
    m_bHasBegunPlay = false;
}

void World::SetGameMode(std::unique_ptr<GameMode> gameMode)
{
    m_GameMode = std::move(gameMode);
}

void World::BeginPlay()
{
    if (m_bHasBegunPlay)
        return;

    if (m_GameMode)
        m_GameMode->StartPlay(*this);

    if (m_Scene)
        m_Scene->BeginPlay();

    m_bHasBegunPlay = true;
}

void World::Tick(float dt, bool bIsPlaying, uint64 frameId)
{
    if (!m_Scene || !m_ModuleManager)
        return;

    m_CurrentFrameId = frameId;
    const bool bAllowGameplayInput = GEngine ? GEngine->ShouldProcessGameplayInput() : bIsPlaying;

    m_ModuleManager->TickModulesByType(TickType::PreSimulation, dt);

    for (const auto& actorPtr : m_Scene->GetActors())
    {
        Actor* actor = actorPtr.Get();
        if (!actor || actor->IsPendingDestroy())
            continue;

        PlayerController* playerController = dynamic_cast<PlayerController*>(actor);
        if (!playerController)
            continue;

        if (bAllowGameplayInput)
            playerController->PreSimulationInputUpdate(frameId, dt);
    }

    m_Scene->PrepareTick();
    m_Scene->TickGroup(ActorTickGroup::PrePhysics, dt);

    if (PhysicsSystem* physics = TryGetPhysics()) 
    {
        if (bIsPlaying)
            physics->Step(*m_Scene, dt);
        else
            physics->EditorDebugDraw(*m_Scene);
    }

    m_Scene->UpdateTransforms();
    m_Scene->TickGroup(ActorTickGroup::PostPhysics, dt);

    m_ModuleManager->TickModulesByType(TickType::PostSimulation, dt);
    m_Scene->TickGroup(ActorTickGroup::PostUpdate, dt);
    m_Scene->FinalizeTick();
    TickTimers(dt);
}

void World::SetTimer(TimerHandle& handle, std::function<void()> callback, const float intervalSeconds, const bool bLooping)
{
    if (intervalSeconds <= 0.0f || !callback)
    {
        ClearTimer(handle);
        return;
    }

    if (handle.IsValid())
    {
        for (TimerData& timer : m_Timers)
        {
            if (timer.Id != handle.Id)
                continue;

            timer.Callback = std::move(callback);
            timer.IntervalSeconds = intervalSeconds;
            timer.RemainingSeconds = intervalSeconds;
            timer.bLooping = bLooping;
            timer.bActive = true;
            return;
        }
    }

    TimerData timer{};
    timer.Id = m_NextTimerId++;
    timer.Callback = std::move(callback);
    timer.IntervalSeconds = intervalSeconds;
    timer.RemainingSeconds = intervalSeconds;
    timer.bLooping = bLooping;
    timer.bActive = true;
    m_Timers.push_back(std::move(timer));
    handle.Id = m_Timers.back().Id;
}

void World::ClearTimer(TimerHandle& handle)
{
    if (!handle.IsValid())
        return;

    for (TimerData& timer : m_Timers)
    {
        if (timer.Id != handle.Id)
            continue;

        timer.bActive = false;
        timer.Callback = nullptr;
        break;
    }

    handle.Invalidate();
}

bool World::IsTimerActive(const TimerHandle& handle) const
{
    if (!handle.IsValid())
        return false;

    for (const TimerData& timer : m_Timers)
    {
        if (timer.Id == handle.Id)
            return timer.bActive;
    }

    return false;
}

void World::TickTimers(const float dt)
{
    if (dt <= 0.0f || m_Timers.empty())
        return;

    for (TimerData& timer : m_Timers)
    {
        if (!timer.bActive || !timer.Callback)
            continue;

        timer.RemainingSeconds -= dt;
        if (timer.RemainingSeconds > 0.0f)
            continue;

        timer.Callback();

        if (!timer.bLooping)
        {
            timer.bActive = false;
            timer.Callback = nullptr;
            continue;
        }

        timer.RemainingSeconds += timer.IntervalSeconds;
        if (timer.RemainingSeconds <= 0.0f)
            timer.RemainingSeconds = timer.IntervalSeconds;
    }

    m_Timers.erase(
        std::remove_if(
            m_Timers.begin(),
            m_Timers.end(),
            [](const TimerData& timer)
            {
                return !timer.bActive;
            }),
        m_Timers.end());
}

PhysicsModule* World::GetPhysicsModule() const
{
    if (!m_ModuleManager)
        return nullptr;

    return m_ModuleManager->GetModule<PhysicsModule>();
}

PhysicsSystem* World::TryGetPhysics() const
{
    PhysicsModule* physicsModule = GetPhysicsModule();
    if (!physicsModule)
        return nullptr;

    return physicsModule->GetPhysicsSystem();
}

PhysicsSystem& World::GetPhysics() const
{
    PhysicsSystem* physics = TryGetPhysics();
    CHECK_MSG(physics, "World::GetPhysics failed: PhysicsSystem is unavailable.");
    return *physics;
}

bool World::LineTraceSingle(const Vector3& start, const Vector3& end, TraceHit& outHit, const TraceQueryParams& params) const
{
    PhysicsSystem* physics = TryGetPhysics();
    if (!physics)
    {
        outHit = {};
        return false;
    }

    return physics->LineTraceSingle(start, end, outHit, params);
}

bool World::LineTraceMulti(const Vector3& start, const Vector3& end, std::vector<TraceHit>& outHits, const TraceQueryParams& params) const
{
    PhysicsSystem* physics = TryGetPhysics();
    if (!physics)
    {
        outHits.clear();
        return false;
    }

    return physics->LineTraceMulti(start, end, outHits, params);
}

bool World::SphereTraceSingle(const Vector3& start, const Vector3& end, float radius, TraceHit& outHit, const TraceQueryParams& params) const
{
    PhysicsSystem* physics = TryGetPhysics();
    if (!physics)
    {
        outHit = {};
        return false;
    }

    return physics->SphereTraceSingle(start, end, radius, outHit, params);
}

bool World::CapsuleTraceSingle(const Vector3& start, const Vector3& end, float halfHeight, float radius, TraceHit& outHit, const TraceQueryParams& params) const
{
    PhysicsSystem* physics = TryGetPhysics();
    if (!physics)
    {
        outHit = {};
        return false;
    }

    return physics->CapsuleTraceSingle(start, end, halfHeight, radius, outHit, params);
}

bool World::BoxTraceSingle(const Vector3& start, const Vector3& end, const Vector3& halfExtents, const Quaternion& rotation, TraceHit& outHit, const TraceQueryParams& params) const
{
    PhysicsSystem* physics = TryGetPhysics();
    if (!physics)
    {
        outHit = {};
        return false;
    }

    return physics->BoxTraceSingle(start, end, halfExtents, rotation, outHit, params);
}


