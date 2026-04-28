#include "Engine/Framework/EnginePch.h"
#include "Engine/Gameplay/Framework/PlayerCameraManager.h"

#include "Engine/Components/CameraAnchorComponent.h"
#include "Engine/Components/CameraComponent.h"
#include "Engine/Gameplay/Framework/Controller.h"
#include "Engine/Gameplay/Framework/PlayerController.h"
#include "Engine/Physics/Trace.h"
#include "Engine/Rendering/CameraView.h"
#include "Engine/Scene/World.h"

namespace
{
    constexpr float kTinyNumber = 1.0e-6f;

    Vector3 NormalizeOrFallback(const Vector3& vector, const Vector3& fallback)
    {
        const float lengthSq = glm::dot(vector, vector);
        if (lengthSq <= kTinyNumber)
            return fallback;

        return vector / glm::sqrt(lengthSq);
    }

    float NormalizeDegrees(float angleDegrees)
    {
        float normalized = std::fmod(angleDegrees, 360.0f);
        if (normalized > 180.0f)
            normalized -= 360.0f;
        else if (normalized < -180.0f)
            normalized += 360.0f;

        return normalized;
    }

    Quaternion MakeCameraRotation(const Vector3& forwardVector, const Vector3& upVector)
    {
        const Vector3 forward = NormalizeOrFallback(forwardVector, Vector3(1.0f, 0.0f, 0.0f));
        const Vector3 up = NormalizeOrFallback(upVector, Vector3(0.0f, 0.0f, 1.0f));
        const Vector3 localYAxis = -forward;
        const Vector3 localXAxis = NormalizeOrFallback(glm::cross(localYAxis, up), Vector3(0.0f, 1.0f, 0.0f));
        const Vector3 localZAxis = NormalizeOrFallback(glm::cross(localXAxis, localYAxis), up);

        glm::mat3 basis(1.0f);
        basis[0] = localXAxis;
        basis[1] = localYAxis;
        basis[2] = localZAxis;
        return glm::normalize(glm::quat_cast(basis));
    }
}

PlayerCameraManager::PlayerCameraManager()
{
    SetTickGroup(ActorTickGroup::PostUpdate);
    SetTickPriority(100);

    m_CameraComponent = &CreateDefaultSubobject<CameraComponent>();
    if (m_CameraComponent)
    {
        m_CameraComponent->bPrimary = true;
        SetRootComponent(m_CameraComponent);
    }
}

void PlayerCameraManager::BeginPlay()
{
    Actor::BeginPlay();

    if (!m_CameraComponent)
        m_CameraComponent = GetObjectComponent<CameraComponent>();

    if (m_CameraComponent)
        m_CameraComponent->bPrimary = true;

    m_bViewInitialized = false;
}

void PlayerCameraManager::EndPlay()
{
    m_TargetActor = nullptr;
    m_OwningController = nullptr;
    m_bViewInitialized = false;

    Actor::EndPlay();
}

void PlayerCameraManager::Tick(const float dt)
{
    UpdateView(dt);
    Actor::Tick(dt);
}

void PlayerCameraManager::SetTarget(Actor* target)
{
    m_TargetActor = target;
    m_bViewInitialized = false;
}

void PlayerCameraManager::ClearTarget()
{
    m_TargetActor = nullptr;
    m_bViewInitialized = false;
}

Mat4 PlayerCameraManager::GetCameraTransform() const
{
    return GetActorTransform();
}

CameraView PlayerCameraManager::GetCameraView(const float aspect) const
{
    if (!m_CameraComponent)
        return {};

    return m_CameraComponent->GetCameraView(aspect);
}

CameraPivotResult PlayerCameraManager::ResolveTargetPivot() const
{
    CameraPivotResult result{};

    if (!m_TargetActor)
        return result;

    const CameraAnchorComponent* anchor = m_TargetActor->GetObjectComponent<CameraAnchorComponent>();
    if (anchor)
    {
        result.Position = anchor->GetWorldPosition();
        result.Forward = NormalizeOrFallback(anchor->GetForwardVector(), m_TargetActor->GetActorForwardVector());
        result.Right = NormalizeOrFallback(anchor->GetRightVector(), m_TargetActor->GetActorRightVector());
        result.Up = NormalizeOrFallback(anchor->GetUpVector(), m_TargetActor->GetActorUpVector());
    }
    else
    {
        result.Position = m_TargetActor->GetActorLocation();
        result.Forward = NormalizeOrFallback(m_TargetActor->GetActorForwardVector(), Vector3(1.0f, 0.0f, 0.0f));
        result.Right = NormalizeOrFallback(m_TargetActor->GetActorRightVector(), Vector3(0.0f, -1.0f, 0.0f));
        result.Up = NormalizeOrFallback(m_TargetActor->GetActorUpVector(), Vector3(0.0f, 0.0f, 1.0f));
    }

    result.Position += result.Forward * m_PivotOffset.x;
    result.Position += result.Right * m_PivotOffset.y;
    result.Position += result.Up * m_PivotOffset.z;
    return result;
}

Vector3 PlayerCameraManager::ResolveViewForward() const
{
    float pitchDegrees = 0.0f;
    float yawDegrees = 0.0f;

    if (m_OwningController)
    {
        const Vector3 controlRotation = m_OwningController->GetControlRotation();
        pitchDegrees = controlRotation.x;
        yawDegrees = controlRotation.z;
    }
    else if (m_TargetActor)
    {
        const Vector3 targetRotation = m_TargetActor->GetActorRotationEuler();
        pitchDegrees = targetRotation.x;
        yawDegrees = targetRotation.z;
    }

    pitchDegrees = glm::clamp(pitchDegrees, -89.0f, 89.0f);
    yawDegrees = NormalizeDegrees(yawDegrees);

    const float pitchRadians = glm::radians(pitchDegrees);
    const float yawRadians = glm::radians(yawDegrees);

    Vector3 forward(0.0f);
    forward.x = std::cos(pitchRadians) * std::cos(yawRadians);
    forward.y = std::cos(pitchRadians) * std::sin(yawRadians);
    forward.z = std::sin(pitchRadians);
    return NormalizeOrFallback(forward, Vector3(1.0f, 0.0f, 0.0f));
}

void PlayerCameraManager::ComputeDesiredCameraTransform(Vector3& outPosition, Quaternion& outRotation) const
{
    const CameraPivotResult pivot = ResolveTargetPivot();
    const Vector3 viewForward = ResolveViewForward();
    const Vector3 worldUp(0.0f, 0.0f, 1.0f);
    const Vector3 viewRight = NormalizeOrFallback(glm::cross(viewForward, worldUp), Vector3(0.0f, -1.0f, 0.0f));
    const Vector3 viewUp = NormalizeOrFallback(glm::cross(viewRight, viewForward), worldUp);

    outPosition = pivot.Position - viewForward * m_FollowDistance;
    outPosition += viewForward * m_CameraOffset.x;
    outPosition += viewRight * m_CameraOffset.y;
    outPosition += viewUp * m_CameraOffset.z;
    outRotation = MakeCameraRotation(viewForward, viewUp);
}

Vector3 PlayerCameraManager::ApplyCameraLag(const float dt, const Vector3& desiredPosition)
{
    if (!m_bViewInitialized)
    {
        m_LastCameraPosition = desiredPosition;
        m_bViewInitialized = true;
        return desiredPosition;
    }

    if (!m_bLagEnabled || m_LagSpeed <= 0.0f || dt <= 0.0f)
    {
        m_LastCameraPosition = desiredPosition;
        return desiredPosition;
    }

    const float alpha = 1.0f - std::exp(-m_LagSpeed * dt);
    m_LastCameraPosition = glm::mix(m_LastCameraPosition, desiredPosition, glm::clamp(alpha, 0.0f, 1.0f));
    return m_LastCameraPosition;
}

Vector3 PlayerCameraManager::ApplyCameraCollision(const Vector3& pivotPosition, const Vector3& desiredPosition) const
{
    if (!m_bCollisionEnabled)
        return desiredPosition;

    World* world = GetWorld();
    if (!world)
        return desiredPosition;

    const Vector3 displacement = desiredPosition - pivotPosition;
    if (glm::dot(displacement, displacement) <= kTinyNumber)
        return desiredPosition;

    const EntityID ignoredEntities[] =
    {
        GetHandle(),
        m_TargetActor ? m_TargetActor->GetHandle() : entt::null
    };

    TraceHit hit{};
    TraceQueryParams params{};
    params.Channel = CollisionChannel::Camera;
    params.IgnoreEntities = ignoredEntities;
    params.IgnoreEntityCount = 2;

    if (!world->SphereTraceSingle(pivotPosition, desiredPosition, m_CollisionProbeRadius, hit, params) || !hit.bBlockingHit)
        return desiredPosition;

    return hit.Position + NormalizeOrFallback(hit.Normal, Vector3(0.0f, 0.0f, 1.0f)) * m_CollisionProbeRadius;
}

void PlayerCameraManager::UpdateView(const float dt)
{
    if (!m_CameraComponent)
        return;

    Vector3 desiredPosition(0.0f);
    Quaternion desiredRotation(1.0f, 0.0f, 0.0f, 0.0f);
    ComputeDesiredCameraTransform(desiredPosition, desiredRotation);

    const Vector3 pivotPosition = ResolveTargetPivot().Position;
    Vector3 finalPosition = ApplyCameraLag(dt, desiredPosition);
    finalPosition = ApplyCameraCollision(pivotPosition, finalPosition);

    m_CameraComponent->SetWorldPosition(finalPosition);
    m_CameraComponent->SetWorldRotationQuat(desiredRotation);
    //m_CameraComponent->bPrimary = true;
}
