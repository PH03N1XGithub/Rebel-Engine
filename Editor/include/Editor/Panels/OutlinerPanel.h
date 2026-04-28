#pragma once

#include "Editor/Core/EditorSelection.h"

class OutlinerPanel
{
public:
    explicit OutlinerPanel(EditorSelection& selection);

    void Draw();

private:
    EditorSelection& m_Selection;
    char m_FilterBuffer[128] = {};
};
