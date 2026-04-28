#pragma once

#include "Editor/Core/EditorSelection.h"

class LevelToolbarPanel
{
public:
    explicit LevelToolbarPanel(EditorSelection& selection);

    void Draw();
    float GetHeight() const { return m_Height; }

private:
    EditorSelection& m_Selection;
    float m_Height = 34.0f;
};
