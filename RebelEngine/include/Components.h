// Components.h
#pragma once
#include "AssetManager/AssetPtr.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include "Core/CoreTypes.h"
#include "Core/String.h"
#include "Camera.h"
#include "Mesh.h"
#include <ThirdParty/entt.h>            

#include "CameraView.h"
#include "EngineReflectionExtensions.h"
#include "Animation/AnimationAsset.h"
#include "Animation/SkeletalMeshAsset.h"
#include "AssetManager/MeshAsset.h"
#include "Core/GUID.h"
#include "Trace.h"

// Tag / name / scene / camera / mesh components
using namespace Rebel::Core::Reflection;

struct IDComponent
{
    uint64 ID;
    IDComponent()
    {
        ID = (uint64)Rebel::Core::GUID();
    };
    IDComponent(uint64 id) : ID(id) {}
    REFLECTABLE_CLASS(IDComponent, void)
};
REFLECT_CLASS(IDComponent, void)
{
    REFLECT_PROPERTY(IDComponent, ID, EPropertyFlags::VisibleInEditor);
}
END_REFLECT_CLASS(IDComponent)
REFLECT_ECS_COMPONENT(IDComponent)


// NameComponent -------------------------------
struct NameComponent
{
    String Name = "Actor";

    NameComponent() = default;
    NameComponent(String& name)
        : Name(name) {}

    REFLECTABLE_CLASS(NameComponent, void)
};

REFLECT_CLASS(NameComponent, void)
{
    REFLECT_PROPERTY(NameComponent, Name,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);
}
END_REFLECT_CLASS(NameComponent)
REFLECT_ECS_COMPONENT(NameComponent)

struct ActorTagComponent
{
    String Tag; // TODO : add Array Support to reflection
    ActorTagComponent() = default;
    ActorTagComponent(String& tag)
        : Tag(tag) {}
    REFLECTABLE_CLASS(ActorTagComponent, void)
};

REFLECT_CLASS(ActorTagComponent, void)
{
    REFLECT_PROPERTY(ActorTagComponent, Tag,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);
}
END_REFLECT_CLASS(ActorTagComponent)
REFLECT_ECS_COMPONENT(ActorTagComponent)


struct EntityComponent
{
    class Actor* Owner = nullptr;
    entt::entity ECSHandle = entt::null;

    virtual ~EntityComponent() = default;
    REFLECTABLE_CLASS(EntityComponent, void)
};
REFLECT_CLASS(EntityComponent, void)
END_REFLECT_CLASS(EntityComponent)


struct TagComponent : EntityComponent
{

    ~TagComponent() = default;

public:
    String ComponentTag;

    TagComponent() = default;
    TagComponent(String& tag)
        : ComponentTag(tag) {}

    REFLECTABLE_CLASS(TagComponent, EntityComponent)
};

REFLECT_CLASS(TagComponent, EntityComponent)
{
    REFLECT_PROPERTY(TagComponent, ComponentTag,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);
}
END_REFLECT_CLASS(TagComponent)
REFLECT_ECS_COMPONENT(TagComponent)
struct ActorComponent : TagComponent
{
    virtual ~ActorComponent() = default;
    virtual void OnCreate() {}
    virtual void BeginPlay() {}
    virtual void Tick(float deltaTime) {}
};

// SceneComponent -------------------------------------------------
// SceneComponent -------------------------------------------------
struct SceneComponent : TagComponent
{
    friend Actor;
    // --- Local transform (editor-facing) ---
    Vector3 Position      { 0.0f, 0.0f, 0.0f };
    Vector3 Rotation { 0.0f, 0.0f, 0.0f }; // degrees
    Vector3 Scale         { 1.0f, 1.0f, 1.0f };

    // --- Internal rotation (used for math) ---
    glm::quat RotationQuat { 1.0f, 0.0f, 0.0f, 0.0f }; // identity

    // --- World transform (computed by Scene) ---
    Mat4 WorldTransform { 1.0f };  // always TRS in world space

private:
    entt::entity Parent = entt::null;
    entt::registry* SceneRegistry = nullptr;
public:

    SceneComponent() = default;
    SceneComponent(const SceneComponent&) = default;

    SceneComponent(const Vector3& position)
        : Position(position)
    {
        // keep Euler and quat consistent
        RotationQuat = glm::quat(glm::radians(Rotation));
    }

    // ---------------------------------------------------------
    // LOCAL ROTATION: setters to keep Euler <-> Quat in sync
    // ---------------------------------------------------------
    void SetRotationEuler(const Vector3& eulerDeg)
    {
        Rotation = eulerDeg;
        Vector3 radians = glm::radians(eulerDeg);
        RotationQuat    = glm::quat(radians);
    }

    void SetRotationQuat(const glm::quat& q)
    {
        RotationQuat = glm::normalize(q);
        Rotation = glm::degrees(glm::eulerAngles(RotationQuat));
    }

    // ---------------------------------------------------------
    // LOCAL MATRIX (relative to parent)
    // ---------------------------------------------------------
    Mat4 GetLocalTransform() const
    {
        Mat4 t = glm::translate(Mat4(1.0f), Position);
        Mat4 r = glm::toMat4(RotationQuat);
        Mat4 s = glm::scale(Mat4(1.0f), Scale);
        return t * r * s;
    }

    // ---------------------------------------------------------
    // WORLD MATRIX (already includes parents)
    // ---------------------------------------------------------
    Mat4 GetWorldTransform() const
    {
        if (!SceneRegistry) return GetLocalTransform();
        CHECK(SceneRegistry);
        
        const auto& sc = SceneRegistry->get<SceneComponent*>(Parent);
        Mat4 lc = GetLocalTransform();
        Mat4 wp = sc->WorldTransform * lc;
        return wp;
    }

    REFLECTABLE_CLASS(SceneComponent, TagComponent)
};

REFLECT_CLASS(SceneComponent, TagComponent)
{
    // For now we only expose Transform to the editor.
    // Parent / Children are driven by hierarchy, not edited directly.
    REFLECT_PROPERTY(SceneComponent, Position,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);
    REFLECT_PROPERTY(SceneComponent, Rotation,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);
    REFLECT_PROPERTY(SceneComponent, Scale,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);
    

    // If you *do* want to see them in a debug UI, you could add:
    // REFLECT_PROPERTY(SceneComponent, Parent,   EPropertyFlags::Transient);
    // REFLECT_PROPERTY(SceneComponent, Children, EPropertyFlags::Transient);
}
END_REFLECT_CLASS(SceneComponent)
REFLECT_ECS_COMPONENT(SceneComponent)




enum class EPhysicsShapeType
{
    Box,
    Sphere,
    Capsule
};

struct PhysicsShape
{
    EPhysicsShapeType Type;

    // union-style storage
    Vector3 HalfExtent;   // box
    float Radius;      // sphere / capsule
    float HalfHeight;  // capsule

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
    // Authoring
    ERBBodyType BodyType = ERBBodyType::Dynamic;
    CollisionChannel ObjectChannel = CollisionChannel::Any;
    Bool bIsTrigger = false;

    virtual PhysicsShape CreatePhysicsShape() const = 0;


    // Runtime
    PhysicsBodyHandle Body = 0;
    Bool bCreated = false;

    REFLECTABLE_CLASS(PrimitiveComponent, SceneComponent)
};

REFLECT_ABSTRACT_CLASS(PrimitiveComponent, SceneComponent)
/*{
    REFLECT_PROPERTY(PrimitiveComponent, BodyType,       EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);
}*/
//REFLECT_ECS_COMPONENT(PrimitiveComponent)

struct BoxComponent: PrimitiveComponent
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


struct CapsuleComponent : PrimitiveComponent
{
    REFLECTABLE_CLASS(CapsuleComponent, PrimitiveComponent)
    float Radius = 0.5f;
    float HalfHeight = 1.0f;
    PhysicsShape CreatePhysicsShape() const override
    {
        return PhysicsShape::MakeCapsule(Radius, HalfHeight);
    }
};
REFLECT_CLASS(CapsuleComponent, PrimitiveComponent)
    REFLECT_PROPERTY(CapsuleComponent, BodyType, EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);
    REFLECT_PROPERTY(CapsuleComponent, Radius, EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);
    REFLECT_PROPERTY(CapsuleComponent, HalfHeight, EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);
END_REFLECT_CLASS(CapsuleComponent)
REFLECT_ECS_COMPONENT(CapsuleComponent)


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

// CameraComponent ------------------------------------------------
struct CameraComponent : SceneComponent
{
    Float FOV       = 60.f;
    Float NearPlane = 0.1f;
    Float FarPlane  = 10000.f;
    Bool  bPrimary  = true;

    CameraView GetCameraView(Float aspect) const
    {
        const Mat4& world = GetWorldTransform();

        CameraView view;
        static const Mat4 ZUpToYUp =
        glm::rotate(Mat4(1.0f), glm::radians(-90.0f), Vector3(1, 0, 0)) * glm::scale (Mat4(1.0f), Vector3(1, -1, 1));

        Vector3 position = Vector3(world[3]);

        Vector3 forward = glm::normalize(-Vector3(world[1])); // your forward
        Vector3 up      = glm::normalize( Vector3(world[2])); // Z-up

        view.Position   = Vector3(world[3]);
        view.View = glm::lookAt(
        position,
        position + forward,
        up);
        view.Projection = glm::perspective(
            glm::radians(FOV),
            aspect,
            NearPlane,
            FarPlane
        );
        view.FOV = FOV;

        return view;
    }


    REFLECTABLE_CLASS(CameraComponent, SceneComponent)
};

REFLECT_CLASS(CameraComponent, SceneComponent)
{
    // Camera itself is a complex type; for now just mark it as visible/editable.
    // Later you can add PropertyTypeDeduce<Camera> and special UI.
    REFLECT_OBJECT_PROPERTY(CameraComponent, FOV,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);
    REFLECT_OBJECT_PROPERTY(CameraComponent, NearPlane,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);
    REFLECT_OBJECT_PROPERTY(CameraComponent, FarPlane,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);
    REFLECT_OBJECT_PROPERTY(CameraComponent, bPrimary,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);
}
END_REFLECT_CLASS(CameraComponent)
REFLECT_ECS_COMPONENT(CameraComponent)

// MeshComponent --------------------------------------------------
struct StaticMeshComponent : SceneComponent
{
    //MeshHandle     Mesh{};                    // which slice of the big VBO/IBO to draw
    AssetPtr<MeshAsset>     Mesh{};                    // which slice of the big VBO/IBO to draw
    MaterialHandle Material{};
    

    Bool bIsVisible   = true;   // quick toggle
    Bool bCastShadows = true;   // future shadow pass

    StaticMeshComponent() = default;

    StaticMeshComponent(AssetHandle meshAsset,
                  MaterialHandle mat = MaterialHandle(),
                  Bool visible = true,
                  Bool castShadows = true)
        : Mesh(meshAsset)
        , Material(mat)
        , bIsVisible(visible)
        , bCastShadows(castShadows)
    {}

    Bool IsValid() const
    {
        return (uint64)Mesh.GetHandle() != 0;
    }
    explicit operator Bool() const { return IsValid(); }

    REFLECTABLE_CLASS(StaticMeshComponent, SceneComponent)
};
REFLECT_CLASS(StaticMeshComponent, SceneComponent)
{
    // Mesh handle (which geometry slice)
    REFLECT_PROPERTY(StaticMeshComponent, Mesh,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    // Material handle (what material to use)
    REFLECT_PROPERTY(StaticMeshComponent, Material,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    // Simple toggles
    REFLECT_PROPERTY(StaticMeshComponent, bIsVisible,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(StaticMeshComponent, bCastShadows,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);
}
END_REFLECT_CLASS(StaticMeshComponent)
REFLECT_ECS_COMPONENT(StaticMeshComponent)

// SkeletalMeshComponent -----------------------------------------------------------------
struct SkeletalMeshComponent : SceneComponent
{
    AssetPtr<SkeletalMeshAsset> Mesh{};
    AssetPtr<AnimationAsset> Animation{};
    MaterialHandle Material{};

    Bool bIsVisible   = true;
    Bool bCastShadows = true;
    Bool bDrawSkeleton = false;
    Bool bPlayAnimation = true;
    Bool bLoopAnimation = true;
    Float PlaybackSpeed = 1.0f;
    Float PlaybackTime = 0.0f;

    // Runtime animation outputs (not serialized by reflection).
    TArray<Mat4> LocalPose;
    TArray<Mat4> GlobalPose;
    TArray<Mat4> FinalPalette;

    // Runtime pose data for debug tools (not serialized by reflection).
    TArray<Vector3> RuntimeBoneLocalTranslations;
    TArray<Vector3> RuntimeBoneGlobalTranslations;
    TArray<Vector3> RuntimeBoneLocalScales;
    TArray<Vector3> RuntimeBoneGlobalScales;
    TArray<Quaternion> RuntimeBoneLocalRotations;
    TArray<Quaternion> RuntimeBoneGlobalRotations;

    SkeletalMeshComponent() = default;

    SkeletalMeshComponent(AssetHandle meshAsset,
                          MaterialHandle mat = MaterialHandle(),
                          Bool visible = true,
                          Bool castShadows = true)
        : Mesh(meshAsset)
        , Material(mat)
        , bIsVisible(visible)
        , bCastShadows(castShadows)
    {}

    Bool IsValid() const
    {
        return (uint64)Mesh.GetHandle() != 0;
    }

    explicit operator Bool() const { return IsValid(); }

    REFLECTABLE_CLASS(SkeletalMeshComponent, SceneComponent)
};
REFLECT_CLASS(SkeletalMeshComponent, SceneComponent)
{
    REFLECT_PROPERTY(SkeletalMeshComponent, Mesh,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, Animation,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, Material,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, bIsVisible,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, bCastShadows,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, bDrawSkeleton,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, bPlayAnimation,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, bLoopAnimation,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, PlaybackSpeed,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, PlaybackTime,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);
}
END_REFLECT_CLASS(SkeletalMeshComponent)

REFLECT_ECS_COMPONENT(SkeletalMeshComponent)




