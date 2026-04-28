#pragma once

#include "Engine/Components/PrimitiveComponent.h"
#include "Engine/Physics/Trace.h"

struct CapsuleComponent : PrimitiveComponent
{
    REFLECTABLE_CLASS(CapsuleComponent, PrimitiveComponent)

    float Radius = 0.5f;
    float HalfHeight = 1.0f;

    PhysicsShape CreatePhysicsShape() const override;

    float GetCapsuleRadius() const { return Radius; }
    float GetCapsuleHalfHeight() const { return HalfHeight; }

    bool SweepCapsule(
        const Vector3& start,
        const Vector3& end,
        TraceHit& outHit,
        const TraceQueryParams& params = TraceQueryParams{}) const;
};

REFLECT_CLASS(CapsuleComponent, PrimitiveComponent)
    REFLECT_PROPERTY(CapsuleComponent, BodyType, EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);
    REFLECT_PROPERTY(CapsuleComponent, Radius, EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);
    REFLECT_PROPERTY(CapsuleComponent, HalfHeight, EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);
END_REFLECT_CLASS(CapsuleComponent)
REFLECT_ECS_COMPONENT(CapsuleComponent)

