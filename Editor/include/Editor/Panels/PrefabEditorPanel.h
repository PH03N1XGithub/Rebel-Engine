#pragma once

#include "Editor/Core/PropertyEditor.h"
#include "Editor/AssetEditors/AssetEditor.h"
#include "Engine/Framework/EnginePch.h"
#include "Engine/Rendering/Camera.h"
#include "Engine/Scene/Scene.h"
#include "imgui.h"

class Actor;
struct EntityComponent;
struct PrefabAsset;

class PrefabEditorPanel : public AssetEditor
{
public:
    const Rebel::Core::Reflection::TypeInfo* GetSupportedAssetType() const override;
    const char* GetEditorName() const override { return "Prefab Editor"; }
    RUniquePtr<AssetEditor> CreateInstance() const override { return RMakeUnique<PrefabEditorPanel>(); }
    void Open(AssetHandle prefabHandle) override;
    void Draw(ImGuiID documentDockId, ImGuiID documentClassId) override;
    bool IsOpen() const override { return m_IsOpen; }
    AssetHandle GetAssetHandle() const override { return m_PrefabHandle; }
    void RequestFocus() override { m_RequestFocus = true; }

private:
    void EnsureLayout(const ImVec2& size);
    bool ReloadPrefab();
    bool RebuildPreviewActor();
    bool SavePrefab();
    void ResetPreviewCamera();
    void UpdatePreviewCamera(const ImVec2& viewportSize);
    void EndPreviewCameraInteraction();
    void Close();

private:
    AssetHandle m_PrefabHandle = 0;
    PrefabAsset* m_PrefabAsset = nullptr;
    bool m_IsOpen = false;
    bool m_LayoutInitialized = false;
    Scene m_PreviewScene;
    Actor* m_PreviewActor = nullptr;
    Camera m_PreviewCamera;
    Vector3 m_PreviewPosition = Vector3(0.0f, 0.0f, 0.0f);
    float m_PreviewYaw = 45.0f;
    float m_PreviewPitch = -20.0f;
    float m_PreviewMoveSpeed = 5.0f;
    bool m_PreviewFlyActive = false;
    bool m_PreviewViewportHovered = false;
    bool m_RequestFocus = false;
    bool m_LastLoggedHovered = false;
    bool m_LastLoggedRightMouseDown = false;
    float m_DebugLogCooldown = 0.0f;
    bool m_bShowPreviewGrid = true;
    bool m_bShowPreviewCollision = true;
    Rebel::Core::Reflection::TypeInfo* m_SelectedComponentType = nullptr;
    EntityComponent* m_SelectedComponent = nullptr;
    PropertyEditor m_PropertyEditor;
};
