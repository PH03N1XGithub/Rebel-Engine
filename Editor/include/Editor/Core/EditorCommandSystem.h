#pragma once

#include "Engine/Framework/EnginePch.h"
#include "Editor/Core/EditorContext.h"

#include <memory>
#include <string>
#include <vector>

class IEditorCommand
{
public:
    virtual ~IEditorCommand() = default;

    virtual bool Execute(EditorContext& context) = 0;
    virtual void Undo(EditorContext& context) = 0;
    virtual const char* GetLabel() const = 0;
};

class EditorTransaction
{
public:
    explicit EditorTransaction(std::string name = "Transaction")
        : m_Name(std::move(name))
    {
    }

    bool IsEmpty() const
    {
        return m_Commands.empty();
    }

    const std::string& GetName() const
    {
        return m_Name;
    }

    void Add(std::unique_ptr<IEditorCommand> command)
    {
        if (command)
            m_Commands.push_back(std::move(command));
    }

    void Undo(EditorContext& context)
    {
        for (auto it = m_Commands.rbegin(); it != m_Commands.rend(); ++it)
            (*it)->Undo(context);
    }

    void Redo(EditorContext& context)
    {
        for (auto& command : m_Commands)
            command->Execute(context);
    }

private:
    std::string m_Name;
    std::vector<std::unique_ptr<IEditorCommand>> m_Commands;
};

class EditorTransactionManager
{
public:
    void SetContext(EditorContext* context)
    {
        m_Context = context;
    }

    bool ExecuteCommand(std::unique_ptr<IEditorCommand> command)
    {
        if (!command || m_IsApplyingHistory || !m_Context)
            return false;

        if (!command->Execute(*m_Context))
            return false;

        if (m_ActiveTransaction)
        {
            m_ActiveTransaction->Add(std::move(command));
            return true;
        }

        EditorTransaction transaction(command->GetLabel());
        transaction.Add(std::move(command));
        CommitTransaction(std::move(transaction));
        return true;
    }

    void BeginTransaction(const std::string& name)
    {
        if (m_IsApplyingHistory || m_ActiveTransaction)
            return;

        m_ActiveTransaction = std::make_unique<EditorTransaction>(name);
    }

    void CommitTransaction()
    {
        if (!m_ActiveTransaction)
            return;

        if (!m_ActiveTransaction->IsEmpty())
            CommitTransaction(std::move(*m_ActiveTransaction));

        m_ActiveTransaction.reset();
    }

    void EndTransaction()
    {
        CommitTransaction();
    }

    void CancelTransaction()
    {
        if (!m_ActiveTransaction)
            return;

        if (!m_ActiveTransaction->IsEmpty())
        {
            m_IsApplyingHistory = true;
            if (m_Context)
                m_ActiveTransaction->Undo(*m_Context);
            m_IsApplyingHistory = false;
        }

        m_ActiveTransaction.reset();
    }

    bool Undo()
    {
        if (m_UndoStack.empty() || m_IsApplyingHistory || !m_Context)
            return false;

        m_IsApplyingHistory = true;
        EditorTransaction transaction = std::move(m_UndoStack.back());
        m_UndoStack.pop_back();
        transaction.Undo(*m_Context);
        m_IsApplyingHistory = false;

        m_RedoStack.push_back(std::move(transaction));
        return true;
    }

    bool Redo()
    {
        if (m_RedoStack.empty() || m_IsApplyingHistory || !m_Context)
            return false;

        m_IsApplyingHistory = true;
        EditorTransaction transaction = std::move(m_RedoStack.back());
        m_RedoStack.pop_back();
        transaction.Redo(*m_Context);
        m_IsApplyingHistory = false;

        m_UndoStack.push_back(std::move(transaction));
        return true;
    }

    bool CanUndo() const
    {
        return !m_UndoStack.empty();
    }

    bool CanRedo() const
    {
        return !m_RedoStack.empty();
    }

    bool IsApplyingHistory() const
    {
        return m_IsApplyingHistory;
    }

    void Clear()
    {
        m_ActiveTransaction.reset();
        m_UndoStack.clear();
        m_RedoStack.clear();
    }

private:
    void CommitTransaction(EditorTransaction&& transaction)
    {
        m_UndoStack.push_back(std::move(transaction));
        m_RedoStack.clear();
    }

private:
    EditorContext* m_Context = nullptr;
    std::unique_ptr<EditorTransaction> m_ActiveTransaction;
    std::vector<EditorTransaction> m_UndoStack;
    std::vector<EditorTransaction> m_RedoStack;
    bool m_IsApplyingHistory = false;
};

inline EditorTransactionManager& GetEditorTransactionManager()
{
    static EditorTransactionManager manager;
    return manager;
}
