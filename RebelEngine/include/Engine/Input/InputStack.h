#pragma once

#include <array>

#include "Core/Containers/TArray.h"
#include "Engine/Input/InputAction.h"

class InputComponent;
class PlayerInput;

class InputStack
{
public:
    struct Entry
    {
        InputComponent* Component = nullptr;
        int Priority = 0;
        bool bBlockLowerPriority = false;
    };

    void Push(InputComponent* component, int priority, bool bBlockLowerPriority);
    void Remove(InputComponent* component);
    void Clear();
    void Dispatch(const PlayerInput& playerInput) const;

private:
    TArray<Entry, 8> m_Entries;
};

