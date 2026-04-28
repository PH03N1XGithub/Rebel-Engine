#pragma once

#include "Editor/Core/EditorSelection.h"
#include "Editor/Core/PropertyEditor.h"

class DetailsPanel
{
public:
    explicit DetailsPanel(EditorSelection& selection);

    void Draw();

private:
    EditorSelection& m_Selection;
    PropertyEditor m_PropertyEditor;
    bool m_WindowOpen = true;
};
