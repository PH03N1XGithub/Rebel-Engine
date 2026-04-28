#pragma once

#include "Core/AssetPtrBase.h"
#include "Engine/Framework/EnginePch.h"
#include "imgui.h"

class AssetEditor
{
public:
    virtual ~AssetEditor() = default;

    virtual const Rebel::Core::Reflection::TypeInfo* GetSupportedAssetType() const = 0;
    virtual const char* GetEditorName() const = 0;
    virtual bool CanOpen(const Rebel::Core::Reflection::TypeInfo* assetType) const;
    virtual RUniquePtr<AssetEditor> CreateInstance() const = 0;
    virtual void Open(AssetHandle assetHandle) = 0;
    virtual void Draw(ImGuiID documentDockId, ImGuiID documentClassId) = 0;
    virtual bool IsOpen() const = 0;
    virtual AssetHandle GetAssetHandle() const = 0;
    virtual void RequestFocus() {}
};

class AssetEditorManager
{
public:
    void RegisterEditor(AssetEditor& editor);
    bool OpenAsset(AssetHandle assetHandle);
    void DrawOpenEditors(ImGuiID documentDockId, ImGuiID documentClassId);
    bool IsAssetOpen(AssetHandle assetHandle) const;

private:
    TArray<AssetEditor*> m_EditorPrototypes;
    TArray<RUniquePtr<AssetEditor>> m_OpenEditors;
};
