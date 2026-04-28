#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "Engine/Physics/Trace.h"

class Scene;
class ModuleManager;
class PhysicsModule;
class PhysicsSystem;
class GameMode;

class REBELENGINE_API World
{
public:
    struct TimerHandle
    {
        uint64 Id = 0;

        bool IsValid() const { return Id != 0; }
        void Invalidate() { Id = 0; }
    };

    World(Scene* scene, ModuleManager* moduleManager);

    void SetScene(Scene* scene);
    Scene* GetScene() const { return m_Scene; }
    ModuleManager* GetModuleManager() const { return m_ModuleManager; }
    uint64 GetFrameId() const { return m_CurrentFrameId; }
    void SetGameMode(std::unique_ptr<GameMode> gameMode);
    GameMode* GetGameMode() const { return m_GameMode.get(); }
    void BeginPlay();

    template<typename T>
    T* SpawnActor();

    void Tick(float dt, bool bIsPlaying, uint64 frameId);
    void SetTimer(TimerHandle& handle, std::function<void()> callback, float intervalSeconds, bool bLooping);
    void ClearTimer(TimerHandle& handle);
    bool IsTimerActive(const TimerHandle& handle) const;

    PhysicsSystem* TryGetPhysics() const;
    PhysicsSystem& GetPhysics() const;

    bool LineTraceSingle(const Vector3& start, const Vector3& end, TraceHit& outHit, const TraceQueryParams& params) const;
    bool LineTraceMulti(const Vector3& start, const Vector3& end, std::vector<TraceHit>& outHits, const TraceQueryParams& params) const;
    bool SphereTraceSingle(const Vector3& start, const Vector3& end, float radius, TraceHit& outHit, const TraceQueryParams& params) const;
    bool CapsuleTraceSingle(const Vector3& start, const Vector3& end, float halfHeight, float radius, TraceHit& outHit, const TraceQueryParams& params) const;
    bool BoxTraceSingle(const Vector3& start, const Vector3& end, const Vector3& halfExtents, const Quaternion& rotation, TraceHit& outHit, const TraceQueryParams& params) const;

private:
    struct TimerData
    {
        uint64 Id = 0;
        std::function<void()> Callback{};
        float IntervalSeconds = 0.0f;
        float RemainingSeconds = 0.0f;
        bool bLooping = false;
        bool bActive = false;
    };

    void TickTimers(float dt);
    PhysicsModule* GetPhysicsModule() const;

private:
    Scene* m_Scene = nullptr;
    ModuleManager* m_ModuleManager = nullptr;
    std::unique_ptr<GameMode> m_GameMode;
    bool m_bHasBegunPlay = false;
    uint64 m_CurrentFrameId = 0;
    uint64 m_NextTimerId = 1;
    std::vector<TimerData> m_Timers;
};

#include "Engine/Scene/Scene.h"

template<typename T>
T* World::SpawnActor()
{
    if (!m_Scene)
        return nullptr;

    return &m_Scene->SpawnActor<T>();
}
