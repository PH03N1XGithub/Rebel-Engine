#pragma once

#include "Engine/Framework/EnginePch.h"
#include "Editor/Core/EditorSelection.h"

class AnimationDebuggerPanel
{
public:
    explicit AnimationDebuggerPanel(EditorSelection& selection);

    void Draw();

private:
    EditorSelection& m_Selection;
    int32 m_SelectedBone = 0;
};
