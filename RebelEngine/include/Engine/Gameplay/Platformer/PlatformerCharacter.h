#pragma once
#include "Engine/Gameplay/Framework/Character.h"
#include "Engine/Animation/AnimationAsset.h"
#include "Engine/Physics/Trace.h"
#include "Engine/Scene/World.h"

class PlatformerCharacter : public Character
{
    REFLECTABLE_CLASS(PlatformerCharacter,Character)
public:
    PlatformerCharacter() = default;
    ~PlatformerCharacter() override;
    void BeginPlay() override;
    void Tick(float dt) override;

    void HandleJumpPressed();
    bool TryDash();

private:
    enum class WallRunAnimationPhase : uint8
    {
        None = 0,
        Starting,
        Looping
    };

    bool TryStartWallRun();
    bool TraceWall(TraceHit& outHit, bool& bOutWallOnRight) const;
    void StartWallRun(const TraceHit& wallHit, bool bWallOnRight);
    void UpdateWallRun(float dt);
    void StopWallRun(bool bApplyExitLaunch = true, bool bPlayEndAnimation = true);
    void WallJump();
    void UpdateWallRunAnimation();
    void PlayWallRunStartAnimation();
    void PlayWallRunLoopAnimation();
    void PlayWallRunEndAnimation();
    void PlayWallJumpAnimation();

    float m_DashSpeed = 12.0f;
    float m_DashDuration = 0.20f;
    float m_GravityScaleWhileDashing = 0.0f;
    // <= 0 restores the gravity scale that was active before dash.
    float m_GravityScaleAfterDash = 0.0f;
    float m_DashAnimationPlaybackSpeed = 1.0f;
    float m_DashAnimationBlendIn = 0.08f;
    AssetPtr<AnimationAsset> m_DashAnimation{};
    bool m_bCanDash = true;
    float m_PreDashGravityScale = 1.0f;
    float m_PreWallRunGravityScale = 1.0f;
    World::TimerHandle m_DashCooldownTimerHandle{};
    float m_WallRunForwardSpeed = 1.0f;
    float m_WallRunForwardProbeDistance = 2.5f;
    float m_WallRunSideProbeDistance = 1.75f;
    float m_WallRunDownProbeDistance = 5.0f;
    float m_WallRunWallOffset = 0.55f;
    float m_WallRunMinHorizontalSpeed = 1.0f;
    float m_WallRunExitSpeed = 10.0f;
    float m_WallJumpSpeed = 8.0f;
    float m_WallJumpUpwardSpeed = 6.5f;
    float m_WallRunAnimationPlaybackSpeed = 1.0f;
    float m_WallRunAnimationBlendIn = 0.08f;
    AssetPtr<AnimationAsset> m_WallRunStartAnimationLeft{};
    AssetPtr<AnimationAsset> m_WallRunStartAnimationRight{};
    AssetPtr<AnimationAsset> m_WallRunLoopAnimationLeft{};
    AssetPtr<AnimationAsset> m_WallRunLoopAnimationRight{};
    AssetPtr<AnimationAsset> m_WallRunEndAnimationLeft{};
    AssetPtr<AnimationAsset> m_WallRunEndAnimationRight{};
    AssetPtr<AnimationAsset> m_WallJumpAnimationLeft{};
    AssetPtr<AnimationAsset> m_WallJumpAnimationRight{};
    bool m_bWallRunning = false;
    bool m_bWallOnRight = false;
    Vector3 m_WallRunNormal = Vector3(0.0f);
    WallRunAnimationPhase m_WallRunAnimationPhase = WallRunAnimationPhase::None;
};
REFLECT_CLASS(PlatformerCharacter,Character)
REFLECT_PROPERTY(PlatformerCharacter, m_DashSpeed, Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
REFLECT_PROPERTY(PlatformerCharacter, m_DashDuration, Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
REFLECT_PROPERTY(PlatformerCharacter, m_GravityScaleWhileDashing, Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
REFLECT_PROPERTY(PlatformerCharacter, m_GravityScaleAfterDash, Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
REFLECT_PROPERTY(PlatformerCharacter, m_DashAnimationPlaybackSpeed, Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
REFLECT_PROPERTY(PlatformerCharacter, m_DashAnimationBlendIn, Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
REFLECT_PROPERTY(PlatformerCharacter, m_DashAnimation, Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
REFLECT_PROPERTY(PlatformerCharacter, m_WallRunForwardSpeed, Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
REFLECT_PROPERTY(PlatformerCharacter, m_WallRunForwardProbeDistance, Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
REFLECT_PROPERTY(PlatformerCharacter, m_WallRunSideProbeDistance, Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
REFLECT_PROPERTY(PlatformerCharacter, m_WallRunDownProbeDistance, Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
REFLECT_PROPERTY(PlatformerCharacter, m_WallRunWallOffset, Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
REFLECT_PROPERTY(PlatformerCharacter, m_WallRunMinHorizontalSpeed, Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
REFLECT_PROPERTY(PlatformerCharacter, m_WallRunExitSpeed, Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
REFLECT_PROPERTY(PlatformerCharacter, m_WallJumpSpeed, Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
REFLECT_PROPERTY(PlatformerCharacter, m_WallJumpUpwardSpeed, Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
REFLECT_PROPERTY(PlatformerCharacter, m_WallRunAnimationPlaybackSpeed, Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
REFLECT_PROPERTY(PlatformerCharacter, m_WallRunAnimationBlendIn, Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
REFLECT_PROPERTY(PlatformerCharacter, m_WallRunStartAnimationLeft, Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
REFLECT_PROPERTY(PlatformerCharacter, m_WallRunStartAnimationRight, Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
REFLECT_PROPERTY(PlatformerCharacter, m_WallRunLoopAnimationLeft, Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
REFLECT_PROPERTY(PlatformerCharacter, m_WallRunLoopAnimationRight, Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
REFLECT_PROPERTY(PlatformerCharacter, m_WallRunEndAnimationLeft, Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
REFLECT_PROPERTY(PlatformerCharacter, m_WallRunEndAnimationRight, Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
REFLECT_PROPERTY(PlatformerCharacter, m_WallJumpAnimationLeft, Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
REFLECT_PROPERTY(PlatformerCharacter, m_WallJumpAnimationRight, Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
END_REFLECT_CLASS(PlatformerCharacter)
