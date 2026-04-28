#pragma once

#include "Engine/Components/PrimitiveComponent.h"

struct BoxComponent : PrimitiveComponent
{
    REFLECTABLE_CLASS(BoxComponent, PrimitiveComponent)
    Vector3 HalfExtent = {1,1,1};
    PhysicsShape CreatePhysicsShape() const override
    {
        return PhysicsShape::MakeBox(HalfExtent);
    }
};

REFLECT_CLASS(BoxComponent, PrimitiveComponent)
    REFLECT_PROPERTY(BoxComponent, BodyType, EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);
    REFLECT_PROPERTY(BoxComponent, HalfExtent, EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);
END_REFLECT_CLASS(BoxComponent)
REFLECT_ECS_COMPONENT(BoxComponent)
