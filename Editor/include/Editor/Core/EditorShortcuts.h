#pragma once

#include <functional>

class EditorShortcuts
{
public:
    void SetToggleContentBrowserAction(std::function<void()> action);
    void Tick();

private:
    std::function<void()> m_ToggleContentBrowserAction;
};
