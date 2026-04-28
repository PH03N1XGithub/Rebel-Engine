#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <cmath>

#include "Engine/Components/IdentityComponents.h"

struct SceneComponent : TagComponent
{
    friend Actor;
    friend Scene;
private:
    Vector3 m_Position{ 0.0f, 0.0f, 0.0f };
    Vector3 Rotation{ 0.0f, 0.0f, 0.0f };
    Vector3 Scale{ 1.0f, 1.0f, 1.0f };

    glm::quat m_RotationQuat{ 1.0f, 0.0f, 0.0f, 0.0f };

    SceneComponent* m_Parent = nullptr;
    entt::registry* m_SceneRegistry = nullptr;
public:

    SceneComponent() = default;
    SceneComponent(const SceneComponent&) = default;

    SceneComponent(const Vector3& position)
        : m_Position(position)
    {
        m_RotationQuat = glm::quat(glm::radians(Rotation));
    }

    Vector3 GetForwardVector() const
    {
        return glm::normalize(m_RotationQuat * Vector3(1, 0, 0));
    }

    Vector3 GetRightVector() const
    {
        return glm::normalize(m_RotationQuat * Vector3(0, -1, 0));
    }

    Vector3 GetUpVector() const
    {
        return glm::normalize(m_RotationQuat * Vector3(0, 0, 1));
    }

    const Vector3& GetPosition() const
    {
        return m_Position;
    }

    Vector3 GetWorldPosition() const
    {
        if (!m_Parent)
            return m_Position;

        return m_Parent->GetWorldPosition() +
               (m_Parent->GetWorldRotationQuat() * (m_Parent->GetWorldScale() * m_Position));
    }

    glm::quat GetWorldRotationQuat() const
    {
        if (m_Parent == nullptr)
            return m_RotationQuat;

        return m_Parent->GetWorldRotationQuat() * m_RotationQuat;
    }

    SceneComponent* GetParent() const
    {
        return m_Parent;
    }

    bool IsDescendantOf(const SceneComponent* candidateAncestor) const
    {
        if (!candidateAncestor)
            return false;

        const SceneComponent* cursor = this;
        while (cursor)
        {
            if (cursor == candidateAncestor)
                return true;

            cursor = cursor->m_Parent;
        }

        return false;
    }

    bool AttachTo(SceneComponent* newParent, bool bKeepWorldTransform = true)
    {
        if (newParent == this)
            return false;

        if (newParent && newParent->IsDescendantOf(this))
            return false;

        if (m_Parent == newParent)
            return true;

        Vector3 worldPosition(0.0f);
        glm::quat worldRotation(1.0f, 0.0f, 0.0f, 0.0f);
        Vector3 worldScale(1.0f);

        if (bKeepWorldTransform)
        {
            worldPosition = GetWorldPosition();
            worldRotation = GetWorldRotationQuat();
            worldScale = GetWorldScale();
        }

        m_Parent = newParent;

        if (m_Parent && !m_SceneRegistry)
            m_SceneRegistry = m_Parent->m_SceneRegistry;

        if (!bKeepWorldTransform)
            return true;

        SetWorldPosition(worldPosition);
        SetWorldRotationQuat(worldRotation);

        if (!m_Parent)
        {
            SetScale(worldScale);
            return true;
        }

        const Vector3 parentScale = m_Parent->GetWorldScale();
        Vector3 localScale = worldScale;
        localScale.x = std::fabs(parentScale.x) > 1e-6f ? worldScale.x / parentScale.x : worldScale.x;
        localScale.y = std::fabs(parentScale.y) > 1e-6f ? worldScale.y / parentScale.y : worldScale.y;
        localScale.z = std::fabs(parentScale.z) > 1e-6f ? worldScale.z / parentScale.z : worldScale.z;
        SetScale(localScale);

        return true;
    }

    bool Detach(bool bKeepWorldTransform = true)
    {
        return AttachTo(nullptr, bKeepWorldTransform);
    }

    void SetPosition(const Vector3& pos)
    {
        m_Position = pos;
    }

    void SetWorldPosition(const Vector3& worldPos)
    {
        if (m_Parent == nullptr)
        {
            m_Position = worldPos;
        }
        else
        {
            Vector3 parentPos = m_Parent->GetWorldPosition();
            glm::quat parentRot = m_Parent->GetWorldRotationQuat();
            Vector3 parentScale = m_Parent->GetWorldScale();

            Vector3 relative = worldPos - parentPos;
            relative = glm::inverse(parentRot) * relative;
            relative /= parentScale;

            m_Position = relative;
        }
    }

    void SetWorldRotationQuat(const glm::quat& worldRot)
    {
        if (m_Parent == nullptr)
        {
            m_RotationQuat = worldRot;
        }
        else
        {
            glm::quat parentWorld = m_Parent->GetWorldRotationQuat();
            m_RotationQuat = glm::inverse(parentWorld) * worldRot;
        }

        m_RotationQuat = glm::normalize(m_RotationQuat);
        Rotation = glm::degrees(glm::eulerAngles(m_RotationQuat));
    }

    const Vector3& GetScale() const
    {
        return Scale;
    }

    void SetScale(const Vector3& scale)
    {
        Scale = scale;
    }

    Vector3 GetWorldScale() const
    {
        if (!m_Parent)
            return Scale;

        return m_Parent->GetWorldScale() * Scale;
    }

    const Vector3& GetRotationEuler() const
    {
        return Rotation;
    }

    const glm::quat& GetRotationQuat() const
    {
        return m_RotationQuat;
    }

    void SetRotationEuler(const Vector3& eulerDeg)
    {
        Rotation = eulerDeg;
        Vector3 radians = glm::radians(eulerDeg);
        m_RotationQuat = glm::quat(radians);
    }

    void SetRotationQuat(const glm::quat& q)
    {
        m_RotationQuat = glm::normalize(q);
        Rotation = glm::degrees(glm::eulerAngles(m_RotationQuat));
    }

    void SetWorldTransform(const Vector3& worldPos, const glm::quat& worldRot)
    {
        if (m_Parent == nullptr)
        {
            m_Position = worldPos;
            m_RotationQuat = worldRot;
        }
        else
        {
            glm::quat parentWorldRot = m_Parent->GetWorldRotationQuat();
            glm::mat4 parentInv = glm::inverse(m_Parent->GetWorldTransform());

            glm::vec4 localPos = parentInv * glm::vec4(worldPos, 1.0f);
            m_Position = Vector3(localPos);

            m_RotationQuat = glm::inverse(parentWorldRot) * worldRot;
        }

        m_RotationQuat = glm::normalize(m_RotationQuat);
        Rotation = glm::degrees(glm::eulerAngles(m_RotationQuat));
    }

    Mat4 GetLocalTransform() const
    {
        Mat4 t = glm::translate(Mat4(1.0f), m_Position);
        Mat4 r = glm::toMat4(m_RotationQuat);
        Mat4 s = glm::scale(Mat4(1.0f), Scale);
        return t * r * s;
    }

    Mat4 GetWorldTransform() const
    {
        if (m_Parent == nullptr || !m_SceneRegistry) return GetLocalTransform();
        CHECK(m_SceneRegistry);

        const auto& parentWp = m_Parent->GetWorldTransform();
        Mat4 lc = GetLocalTransform();
        Mat4 wp = parentWp * lc;
        return wp;
    }

    REFLECTABLE_CLASS(SceneComponent, TagComponent)
};

REFLECT_CLASS(SceneComponent, TagComponent)
{
    REFLECT_PROPERTY(SceneComponent, m_Position,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);
    REFLECT_PROPERTY(SceneComponent, Rotation,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);
    REFLECT_PROPERTY(SceneComponent, Scale,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);
}
END_REFLECT_CLASS(SceneComponent)
REFLECT_ECS_COMPONENT(SceneComponent)
