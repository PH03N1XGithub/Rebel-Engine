#pragma once

class Scene;
struct EditorSelection;
class EditorTransactionManager;

struct EditorContext
{
    Scene* ActiveScene = nullptr;
    EditorSelection* Selection = nullptr;
    EditorTransactionManager* Transactions = nullptr;
};

inline EditorContext& GetEditorContext()
{
    static EditorContext context;
    return context;
}
