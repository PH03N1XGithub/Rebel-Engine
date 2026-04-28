#pragma once

#include "Engine/Components/SceneComponent.h"
#include "Engine/Physics/Trace.h"

enum class EPhysicsShapeType
{
    Box,
    Sphere,
    Capsule
};

struct PhysicsShape
{
    EPhysicsShapeType Type;

    Vector3 HalfExtent;
    float Radius;
    float HalfHeight;

    static PhysicsShape MakeBox(const Vector3& halfExtent)
    {
        PhysicsShape s{};
        s.Type = EPhysicsShapeType::Box;
        s.HalfExtent = halfExtent;
        return s;
    }

    static PhysicsShape MakeSphere(float radius)
    {
        PhysicsShape s{};
        s.Type = EPhysicsShapeType::Sphere;
        s.Radius = radius;
        return s;
    }

    static PhysicsShape MakeCapsule(float radius, float halfHeight)
    {
        PhysicsShape s{};
        s.Type = EPhysicsShapeType::Capsule;
        s.Radius = radius;
        s.HalfHeight = halfHeight;
        return s;
    }
};

using PhysicsBodyHandle = uint32;

enum class ERBBodyType : uint8
{
    Static,
    Dynamic,
    Kinematic
};

REFLECT_ENUM(ERBBodyType)
    ENUM_OPTION(Static)
    ENUM_OPTION(Dynamic)
    ENUM_OPTION(Kinematic)
END_ENUM(ERBBodyType)

struct PrimitiveComponent : SceneComponent
{
    ERBBodyType BodyType = ERBBodyType::Dynamic;
    CollisionChannel ObjectChannel = CollisionChannel::Any;
    Bool bIsTrigger = false;

    virtual PhysicsShape CreatePhysicsShape() const = 0;

private:
    PhysicsBodyHandle m_Body = 0;
    Bool m_bCreated = false;

public:
    PhysicsBodyHandle GetBodyHandle() const { return m_Body; }
    Bool IsBodyCreated() const { return m_bCreated; }
    void SetBodyHandle(PhysicsBodyHandle bodyHandle)
    {
        m_Body = bodyHandle;
        m_bCreated = (bodyHandle != 0);
    }
    void ClearBodyHandle()
    {
        m_Body = 0;
        m_bCreated = false;
    }

    REFLECTABLE_CLASS(PrimitiveComponent, SceneComponent)
};

REFLECT_ABSTRACT_CLASS(PrimitiveComponent, SceneComponent)
