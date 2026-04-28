#include "Engine/Framework/EnginePch.h"
#include "Engine/Components/CapsuleComponent.h"

#include <vector>

#include "Engine/Scene/Actor.h"
#include "Engine/Scene/World.h"

PhysicsShape CapsuleComponent::CreatePhysicsShape() const
{
    return PhysicsShape::MakeCapsule(Radius, HalfHeight);
}

bool CapsuleComponent::SweepCapsule(
    const Vector3& start,
    const Vector3& end,
    TraceHit& outHit,
    const TraceQueryParams& params) const
{
    outHit = {};

    const Actor* owner = GetOwner();
    if (!owner)
        return false;

    World* world = owner->GetWorld();
    if (!world)
        return false;

    TraceQueryParams finalParams = params;
    std::vector<EntityID> ignoreEntities;

    if (finalParams.IgnoreEntities && finalParams.IgnoreEntityCount > 0)
    {
        ignoreEntities.assign(
            finalParams.IgnoreEntities,
            finalParams.IgnoreEntities + finalParams.IgnoreEntityCount);
    }

    ignoreEntities.push_back(owner->GetHandle());
    finalParams.IgnoreEntities = ignoreEntities.data();
    finalParams.IgnoreEntityCount = ignoreEntities.size();

    return world->CapsuleTraceSingle(start, end, HalfHeight, Radius, outHit, finalParams);
}

