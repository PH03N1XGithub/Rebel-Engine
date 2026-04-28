#pragma once

#include "Editor/Core/EditorCommandSystem.h"
#include "Editor/Core/EditorContext.h"

#include <memory>
#include <string>

class EditorCommandDispatcher
{
public:
    static void Execute(std::unique_ptr<IEditorCommand> command)
    {
        EditorContext& context = GetEditorContext();
        EditorTransactionManager* transactions = context.Transactions ? context.Transactions : &GetEditorTransactionManager();
        transactions->ExecuteCommand(std::move(command));
    }

    static void BeginTransaction(const std::string& name)
    {
        EditorContext& context = GetEditorContext();
        EditorTransactionManager* transactions = context.Transactions ? context.Transactions : &GetEditorTransactionManager();
        transactions->BeginTransaction(name);
    }

    static void CommitTransaction()
    {
        EditorContext& context = GetEditorContext();
        EditorTransactionManager* transactions = context.Transactions ? context.Transactions : &GetEditorTransactionManager();
        transactions->CommitTransaction();
    }

    static void CancelTransaction()
    {
        EditorContext& context = GetEditorContext();
        EditorTransactionManager* transactions = context.Transactions ? context.Transactions : &GetEditorTransactionManager();
        transactions->CancelTransaction();
    }
};
