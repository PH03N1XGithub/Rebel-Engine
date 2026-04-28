#pragma once

#include "Engine/Components/SceneComponent.h"

struct CameraAnchorComponent : SceneComponent
{
    REFLECTABLE_CLASS(CameraAnchorComponent, SceneComponent)
};

REFLECT_CLASS(CameraAnchorComponent, SceneComponent)
END_REFLECT_CLASS(CameraAnchorComponent)
REFLECT_ECS_COMPONENT(CameraAnchorComponent)
