#include "Engine/Framework/EnginePch.h"
#include "Engine/Input/InputStack.h"

#include <algorithm>

#include "Engine/Input/InputComponent.h"
#include "Engine/Input/PlayerInput.h"

void InputStack::Push(InputComponent* component, const int priority, const bool bBlockLowerPriority)
{
    if (!component)
        return;

    Remove(component);
    m_Entries.Emplace(Entry{component, priority, bBlockLowerPriority});
}

void InputStack::Remove(InputComponent* component)
{
    if (!component)
        return;

    for (uint32 i = 0; i < m_Entries.Num();)
    {
        if (m_Entries[i].Component == component)
        {
            m_Entries.EraseAtSwap(i);
            continue;
        }

        ++i;
    }
}

void InputStack::Clear()
{
    m_Entries.Clear();
}

void InputStack::Dispatch(const PlayerInput& playerInput) const
{
    if (m_Entries.IsEmpty())
        return;

    TArray<Entry, 8> sortedEntries = m_Entries;
    std::stable_sort(
        sortedEntries.begin(),
        sortedEntries.end(),
        [](const Entry& lhs, const Entry& rhs)
        {
            return lhs.Priority > rhs.Priority;
        });

    std::array<bool, kInputActionCount> consumedActions{};

    for (const Entry& entry : sortedEntries)
    {
        if (!entry.Component)
            continue;

        entry.Component->ProcessBindings(playerInput, consumedActions);
        if (entry.bBlockLowerPriority)
            break;
    }
}

