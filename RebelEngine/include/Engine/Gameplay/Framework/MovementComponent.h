#pragma once

#include "Engine/Components/Components.h"
#include "Engine/Gameplay/Framework/Pawn.h"

class MovementComponent : public ActorComponent
{
    REFLECTABLE_CLASS(MovementComponent, ActorComponent)
public:
    void BeginPlay() override
    {
        m_Velocity = Vector3(0.0f);
        m_Acceleration = Vector3(0.0f);

        Pawn* pawn = ResolvePawnOwner();
        if (!pawn)
        {
            m_bIsGrounded = false;
            return;
        }

        if (!m_UpdatedComponent)
            m_UpdatedComponent = ResolveDefaultUpdatedComponent(*pawn);

        UpdateGroundedState(*pawn);
    }

    void Tick(const float deltaTime) override
    {
        if (deltaTime <= 0.0f)
            return;

        Pawn* pawn = ResolvePawnOwner();
        if (!pawn)
        {
            m_Acceleration = Vector3(0.0f);
            m_bIsGrounded = false;
            return;
        }

        if (!m_UpdatedComponent)
            m_UpdatedComponent = ResolveDefaultUpdatedComponent(*pawn);

        UpdateGroundedState(*pawn);
        if (m_bIsGrounded && m_Velocity.z < 0.0f)
            m_Velocity.z = 0.0f;

        const Vector3 moveInput = SanitizeMoveInput(pawn->ConsumeMovementInput());

        Vector3 frameAcceleration = ComputeAcceleration(moveInput);
        if (!m_bIsGrounded)
            frameAcceleration.z -= m_Gravity;

        m_Acceleration = frameAcceleration;

        const Vector3 velocityDelta = m_Acceleration * deltaTime;
        m_Velocity += velocityDelta;
        if (m_bIsGrounded)
            ApplyGroundFriction(deltaTime);
        else
            ApplyAirDamping(deltaTime);

        ClampHorizontalSpeed();

        const Vector3 deltaLocation = m_Velocity * deltaTime;
        MoveUpdatedComponent(deltaLocation);

        ResolveGroundCollision(*pawn);
    }

    const Vector3& GetVelocity() const { return m_Velocity; }
    const Vector3& GetAcceleration() const { return m_Acceleration; }
    void SetVelocity(const Vector3& velocity) { m_Velocity = velocity; }
    void SetGroundHeight(const float groundHeight) { m_GroundHeight = groundHeight; }
    void SetGroundTolerance(const float groundTolerance) { m_GroundTolerance = glm::max(0.0f, groundTolerance); }
    void SetMoveAcceleration(const float moveAcceleration) { m_MoveAcceleration = glm::max(0.0f, moveAcceleration); }
    void SetMaxSpeed(const float maxSpeed) { m_MaxSpeed = glm::max(0.0f, maxSpeed); }
    void SetFriction(const float friction) { m_Friction = glm::max(0.0f, friction); }
    void SetAirDamping(const float airDamping) { m_AirDamping = glm::max(0.0f, airDamping); }
    void SetGravity(const float gravity) { m_Gravity = glm::max(0.0f, gravity); }

    float GetMoveAcceleration() const { return m_MoveAcceleration; }
    float GetMaxSpeed() const { return m_MaxSpeed; }
    float GetFriction() const { return m_Friction; }
    float GetAirDamping() const { return m_AirDamping; }
    float GetGravity() const { return m_Gravity; }
    float GetGroundHeight() const { return m_GroundHeight; }
    float GetGroundTolerance() const { return m_GroundTolerance; }
    bool IsGrounded() const { return m_bIsGrounded; }

    void SetUpdatedComponent(SceneComponent* updatedComponent) { m_UpdatedComponent = updatedComponent; }
    SceneComponent* GetUpdatedComponent() const { return m_UpdatedComponent; }
    bool HasValidUpdatedComponent() const { return m_UpdatedComponent != nullptr; }

protected:
    virtual Pawn* ResolvePawnOwner() const
    {
        return dynamic_cast<Pawn*>(GetOwner());
    }

    virtual SceneComponent* ResolveDefaultUpdatedComponent(Pawn& pawn) const
    {
        return pawn.GetRootComponent();
    }

    virtual void UpdateGroundedState(const Pawn& pawn)
    {
        const float groundHeight = QueryGroundHeight(pawn);
        const float currentHeight = GetCurrentHeight(pawn);
        const bool bBelowGround = currentHeight < (groundHeight - m_GroundTolerance);
        const bool bNearGround = currentHeight <= (groundHeight + m_GroundTolerance);
        const bool bNotMovingUp = m_Velocity.z <= 0.0f;
        m_bIsGrounded = bBelowGround || (bNearGround && bNotMovingUp);
    }

    virtual float QueryGroundHeight(const Pawn& pawn) const
    {
        (void)pawn;
        return m_GroundHeight;
    }

    Vector3 SanitizeMoveInput(Vector3 moveInput) const
    {
        moveInput.z = 0.0f;

        const float moveInputLengthSq = glm::dot(moveInput, moveInput);
        if (moveInputLengthSq > 1.0f)
            moveInput /= glm::sqrt(moveInputLengthSq);

        return moveInput;
    }

    void ResolveGroundCollision(Pawn& pawn)
    {
        UpdateGroundedState(pawn);
        if (!m_bIsGrounded)
            return;

        Vector3 location = GetCurrentLocation(pawn);
        const float groundHeight = QueryGroundHeight(pawn);
        if (location.z <= (groundHeight + m_GroundTolerance))
        {
            location.z = groundHeight;
            SetCurrentLocation(pawn, location);
        }

        if (m_Velocity.z < 0.0f)
            m_Velocity.z = 0.0f;
    }

    Vector3 ComputeAcceleration(const Vector3& moveInput) const
    {
        return moveInput * m_MoveAcceleration;
    }

    void ApplyGroundFriction(const float deltaTime)
    {
        if (m_Friction <= 0.0f)
            return;

        Vector2 horizontalVelocity(m_Velocity.x, m_Velocity.y);
        const float speed = glm::length(horizontalVelocity);
        if (speed <= 0.0f)
            return;

        const float speedDrop = m_Friction * deltaTime;
        const float newSpeed = glm::max(0.0f, speed - speedDrop);
        if (newSpeed <= 0.0f)
            horizontalVelocity = Vector2(0.0f);
        else
            horizontalVelocity = (horizontalVelocity / speed) * newSpeed;

        m_Velocity.x = horizontalVelocity.x;
        m_Velocity.y = horizontalVelocity.y;
    }

    void ApplyAirDamping(const float deltaTime)
    {
        if (m_AirDamping <= 0.0f)
            return;

        const float speed = glm::length(m_Velocity);
        if (speed <= 0.0f)
            return;

        const float speedDrop = m_AirDamping * deltaTime;
        const float newSpeed = glm::max(0.0f, speed - speedDrop);
        if (newSpeed <= 0.0f)
            m_Velocity = Vector3(0.0f);
        else
            m_Velocity = (m_Velocity / speed) * newSpeed;

    }

    void ClampHorizontalSpeed()
    {
        Vector3 horizontalVelocity(m_Velocity.x, m_Velocity.y, 0.0f);
        const float speed = glm::length(horizontalVelocity);
        if (speed <= m_MaxSpeed || speed <= 0.0f)
            return;

        horizontalVelocity = (horizontalVelocity / speed) * m_MaxSpeed;
        m_Velocity.x = horizontalVelocity.x;
        m_Velocity.y = horizontalVelocity.y;
    }

    Vector3 GetCurrentLocation(const Pawn& pawn) const
    {
        if (m_UpdatedComponent)
            return m_UpdatedComponent->GetWorldPosition();

        return pawn.GetActorLocation();
    }

    void SetCurrentLocation(Pawn& pawn, const Vector3& location)
    {
        if (m_UpdatedComponent)
        {
            m_UpdatedComponent->SetWorldPosition(location);
            return;
        }

        pawn.SetActorLocation(location);
    }

    float GetCurrentHeight(const Pawn& pawn) const
    {
        return GetCurrentLocation(pawn).z;
    }

    void MoveUpdatedComponent(const Vector3& deltaLocation)
    {
        if (m_UpdatedComponent)
        {
            m_UpdatedComponent->SetWorldPosition(m_UpdatedComponent->GetWorldPosition() + deltaLocation);
            return;
        }

        if (Pawn* pawn = ResolvePawnOwner())
            pawn->AddActorWorldOffset(deltaLocation);
    }

protected:
    SceneComponent* m_UpdatedComponent = nullptr;
    Vector3 m_Velocity = Vector3(0.0f);
    Vector3 m_Acceleration = Vector3(0.0f);

    float m_MoveAcceleration = 2200.0f;
    float m_MaxSpeed = 1000.0f;
    float m_Friction = 900.0f;
    float m_AirDamping = 0.0f;
    float m_Gravity = 980.0f;

    float m_GroundHeight = 0.0f;
    float m_GroundTolerance = 0.1f;
    bool m_bIsGrounded = false;
};

REFLECT_CLASS(MovementComponent, ActorComponent)
    REFLECT_PROPERTY(MovementComponent, m_MoveAcceleration,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(MovementComponent, m_MaxSpeed,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(MovementComponent, m_Friction,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(MovementComponent, m_AirDamping,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(MovementComponent, m_Gravity,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(MovementComponent, m_GroundHeight,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(MovementComponent, m_GroundTolerance,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
END_REFLECT_CLASS(MovementComponent)

