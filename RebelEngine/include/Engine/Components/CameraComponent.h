#pragma once

#include "Engine/Rendering/CameraView.h"
#include "Engine/Components/SceneComponent.h"
#include <glm/gtc/matrix_transform.hpp>

struct CameraComponent : SceneComponent
{
    Float FOV = 60.f;
    Float NearPlane = 0.1f;
    Float FarPlane = 10000.f;
    Bool  bPrimary = true;

    CameraView GetCameraView(Float aspect) const
    {
        const Mat4& world = GetWorldTransform();

        CameraView view;
        static const Mat4 ZUpToYUp =
        glm::rotate(Mat4(1.0f), glm::radians(-90.0f), Vector3(1, 0, 0)) * glm::scale(Mat4(1.0f), Vector3(1, -1, 1));

        Vector3 position = Vector3(world[3]);

        Vector3 forward = glm::normalize(-Vector3(world[1]));
        Vector3 up = glm::normalize(Vector3(world[2]));

        view.Position = Vector3(world[3]);
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
