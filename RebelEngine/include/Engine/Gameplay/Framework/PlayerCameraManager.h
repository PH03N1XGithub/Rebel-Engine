#pragma once

#include "Engine/Framework/EngineReflectionExtensions.h"
#include "Engine/Scene/Actor.h"
#include "Engine/Scene/ActorReflection.h"

struct CameraView;
class Actor;
struct CameraAnchorComponent;
struct CameraComponent;
class PlayerController;

struct CameraPivotResult
{
    Vector3 Position = Vector3(0.0f);
    Vector3 Forward = Vector3(1.0f, 0.0f, 0.0f);
    Vector3 Right = Vector3(0.0f, -1.0f, 0.0f);
    Vector3 Up = Vector3(0.0f, 0.0f, 1.0f);
};

class PlayerCameraManager : public Actor
{
    REFLECTABLE_CLASS(PlayerCameraManager, Actor);

public:
    PlayerCameraManager();
    ~PlayerCameraManager() override = default;

    void BeginPlay() override;
    void EndPlay() override;
    void Tick(float dt) override;

    void SetOwningPlayerController(PlayerController* controller) { m_OwningController = controller; }
    PlayerController* GetOwningPlayerController() const { return m_OwningController; }

    void SetTarget(Actor* target);
    void ClearTarget();
    Actor* GetTarget() const { return m_TargetActor; }

    Mat4 GetCameraTransform() const;
    CameraView GetCameraView(float aspect) const;
    CameraComponent* GetCameraComponent() const { return m_CameraComponent; }

    void SetPivotOffset(const Vector3& pivotOffset) { m_PivotOffset = pivotOffset; }
    void SetCameraOffset(const Vector3& cameraOffset) { m_CameraOffset = cameraOffset; }
    void SetFollowDistance(float followDistance) { m_FollowDistance = glm::max(0.0f, followDistance); }
    void SetLagEnabled(bool bEnabled) { m_bLagEnabled = bEnabled; }
    void SetLagSpeed(float lagSpeed) { m_LagSpeed = glm::max(0.0f, lagSpeed); }
    void SetCollisionEnabled(bool bEnabled) { m_bCollisionEnabled = bEnabled; }
    void SetCollisionProbeRadius(float probeRadius) { m_CollisionProbeRadius = glm::max(0.0f, probeRadius); }

    const Vector3& GetPivotOffset() const { return m_PivotOffset; }
    const Vector3& GetCameraOffset() const { return m_CameraOffset; }
    float GetFollowDistance() const { return m_FollowDistance; }
    bool IsLagEnabled() const { return m_bLagEnabled; }
    float GetLagSpeed() const { return m_LagSpeed; }
    bool IsCollisionEnabled() const { return m_bCollisionEnabled; }
    float GetCollisionProbeRadius() const { return m_CollisionProbeRadius; }

protected:
    CameraPivotResult ResolveTargetPivot() const;
    Vector3 ResolveViewForward() const;
    void ComputeDesiredCameraTransform(Vector3& outPosition, Quaternion& outRotation) const;
    Vector3 ApplyCameraLag(float dt, const Vector3& desiredPosition);
    Vector3 ApplyCameraCollision(const Vector3& pivotPosition, const Vector3& desiredPosition) const;
    void UpdateView(float dt);

private:
    PlayerController* m_OwningController = nullptr;
    Actor* m_TargetActor = nullptr;
    CameraComponent* m_CameraComponent = nullptr;

    Vector3 m_PivotOffset = Vector3(0.0f);
    Vector3 m_CameraOffset = Vector3(0.0f);
    float m_FollowDistance = 4.0f;
    float m_LagSpeed = 10.0f;
    float m_CollisionProbeRadius = 0.2f;
    bool m_bLagEnabled = false;
    bool m_bCollisionEnabled = true;
    bool m_bViewInitialized = false;
    Vector3 m_LastCameraPosition = Vector3(0.0f);
};

REFLECT_CLASS(PlayerCameraManager, Actor)
    REFLECT_PROPERTY(PlayerCameraManager, m_PivotOffset,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(PlayerCameraManager, m_CameraOffset,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(PlayerCameraManager, m_FollowDistance,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(PlayerCameraManager, m_LagSpeed,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(PlayerCameraManager, m_CollisionProbeRadius,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(PlayerCameraManager, m_bLagEnabled,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(PlayerCameraManager, m_bCollisionEnabled,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
END_REFLECT_CLASS(PlayerCameraManager)
