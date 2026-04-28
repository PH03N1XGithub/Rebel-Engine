#include "Editor/AssetEditors/AssetEditor.h"

#include "Engine/Assets/AssetManagerModule.h"
#include "Engine/Framework/BaseEngine.h"

bool AssetEditor::CanOpen(const Rebel::Core::Reflection::TypeInfo* assetType) const
{
    const Rebel::Core::Reflection::TypeInfo* supportedType = GetSupportedAssetType();
    return assetType && supportedType && assetType->IsA(supportedType);
}

void AssetEditorManager::RegisterEditor(AssetEditor& editor)
{
    for (AssetEditor* existing : m_EditorPrototypes)
    {
        if (existing == &editor)
            return;
    }

    m_EditorPrototypes.Add(&editor);
}

bool AssetEditorManager::OpenAsset(const AssetHandle assetHandle)
{
    if (!IsValidAssetHandle(assetHandle) || !GEngine)
        return false;

    AssetManagerModule* assetModule = GEngine->GetModuleManager().GetModule<AssetManagerModule>();
    if (!assetModule)
        return false;

    const AssetMeta* meta = assetModule->GetRegistry().Get(assetHandle);
    if (!meta || !meta->Type)
        return false;

    for (const RUniquePtr<AssetEditor>& editor : m_OpenEditors)
    {
        if (editor && editor->IsOpen() && editor->CanOpen(meta->Type) && editor->GetAssetHandle() == assetHandle)
        {
            editor->RequestFocus();
            return true;
        }
    }

    for (AssetEditor* editor : m_EditorPrototypes)
    {
        if (!editor || !editor->CanOpen(meta->Type))
            continue;

        RUniquePtr<AssetEditor> instance = editor->CreateInstance();
        if (!instance)
            return false;

        instance->Open(assetHandle);
        if (!instance->IsOpen())
            return false;

        m_OpenEditors.Add(std::move(instance));
        return true;
    }

    return false;
}

bool AssetEditorManager::IsAssetOpen(const AssetHandle assetHandle) const
{
    if (!IsValidAssetHandle(assetHandle))
        return false;

    for (const RUniquePtr<AssetEditor>& editor : m_OpenEditors)
    {
        if (editor && editor->IsOpen() && editor->GetAssetHandle() == assetHandle)
            return true;
    }

    return false;
}

void AssetEditorManager::DrawOpenEditors(const ImGuiID documentDockId, const ImGuiID documentClassId)
{
    for (int32 i = 0; i < m_OpenEditors.Num();)
    {
        AssetEditor* editor = m_OpenEditors[i].Get();
        if (!editor || !editor->IsOpen())
        {
            m_OpenEditors.RemoveAt(i);
            continue;
        }

        editor->Draw(documentDockId, documentClassId);

        if (!editor->IsOpen())
        {
            m_OpenEditors.RemoveAt(i);
            continue;
        }

        ++i;
    }
}
