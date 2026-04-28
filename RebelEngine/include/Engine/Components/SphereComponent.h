#pragma once

#include "Engine/Components/PrimitiveComponent.h"

struct SphereComponent : PrimitiveComponent
{
    REFLECTABLE_CLASS(SphereComponent, PrimitiveComponent)
    float Radius = 1.0f;
    PhysicsShape CreatePhysicsShape() const override
    {
        return PhysicsShape::MakeSphere(Radius);
    }
};

REFLECT_CLASS(SphereComponent, PrimitiveComponent)
    REFLECT_PROPERTY(SphereComponent, BodyType, EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);
    REFLECT_PROPERTY(SphereComponent, Radius, EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);
END_REFLECT_CLASS(SphereComponent)
REFLECT_ECS_COMPONENT(SphereComponent)
