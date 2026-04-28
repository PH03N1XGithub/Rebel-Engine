#pragma once

#include "Editor/AssetEditors/AssetEditor.h"
#include "Engine/Assets/BaseAsset.h"
#include "Engine/Rendering/Camera.h"
#include "Engine/Scene/Scene.h"
#include "Editor/Core/Graph/EditorGraphTypes.h"

struct AnimationAsset;
struct AnimGraphAsset;
struct SkeletonAsset;
struct SkeletalMeshAsset;
class Actor;

class PreviewAssetEditorBase : public AssetEditor
{
public:
    bool IsOpen() const override { return m_IsOpen; }
    AssetHandle GetAssetHandle() const override { return m_AssetHandle; }
    void RequestFocus() override { m_RequestFocus = true; }

protected:
    void BeginEditorWindow(const char* baseTitle, Asset* asset, ImGuiID documentDockId, ImGuiID documentClassId, bool& open, bool& visible);
    void DrawViewportImage(Scene* scene, const char* label, bool drawGrid = true);
    void ResetPreviewCamera();
    void UpdatePreviewCamera(const ImVec2& viewportSize);
    void EndPreviewCameraInteraction();
    void CloseBase();

protected:
    AssetHandle m_AssetHandle = 0;
    bool m_IsOpen = false;
    bool m_LayoutInitialized = false;
    Camera m_PreviewCamera;
    Vector3 m_PreviewPosition = Vector3(-4.0f, -4.0f, 2.5f);
    float m_PreviewYaw = 45.0f;
    float m_PreviewPitch = -20.0f;
    float m_PreviewMoveSpeed = 5.0f;
    bool m_PreviewFlyActive = false;
    bool m_PreviewViewportHovered = false;
    bool m_RequestFocus = false;
};

class AnimationAssetEditor : public PreviewAssetEditorBase
{
public:
    const Rebel::Core::Reflection::TypeInfo* GetSupportedAssetType() const override;
    const char* GetEditorName() const override { return "Animation Asset Editor"; }
    RUniquePtr<AssetEditor> CreateInstance() const override { return RMakeUnique<AnimationAssetEditor>(); }
    void Open(AssetHandle assetHandle) override;
    void Draw(ImGuiID documentDockId, ImGuiID documentClassId) override;

private:
    bool ReloadAsset();
    void EnsureLayout(const ImVec2& size);
    void DrawDetails();
    void DrawTimeline();
    void DrawTracks();

private:
    AnimationAsset* m_Animation = nullptr;
    SkeletonAsset* m_Skeleton = nullptr;
    Scene m_PreviewScene;
    Actor* m_PreviewActor = nullptr;
    float m_PlaybackTime = 0.0f;
    bool m_Playing = false;
    bool m_Looping = true;
    int32 m_TimelineCurrentFrame = 0;
    int32 m_TimelineFirstFrame = 0;
    int32 m_SelectedTimelineItem = -1;
    bool m_TimelineExpanded = true;
};

class SkeletonAssetEditor : public PreviewAssetEditorBase
{
public:
    const Rebel::Core::Reflection::TypeInfo* GetSupportedAssetType() const override;
    const char* GetEditorName() const override { return "Skeleton Asset Editor"; }
    RUniquePtr<AssetEditor> CreateInstance() const override { return RMakeUnique<SkeletonAssetEditor>(); }
    void Open(AssetHandle assetHandle) override;
    void Draw(ImGuiID documentDockId, ImGuiID documentClassId) override;

private:
    bool ReloadAsset();
    bool RebuildPreview();
    void EnsureLayout(const ImVec2& size);
    void DrawSkeletonTree();
    void DrawDetails();

private:
    SkeletonAsset* m_Skeleton = nullptr;
    Scene m_PreviewScene;
    Actor* m_PreviewActor = nullptr;
    int32 m_SelectedBone = -1;
};

class SkeletalMeshAssetEditor : public PreviewAssetEditorBase
{
public:
    const Rebel::Core::Reflection::TypeInfo* GetSupportedAssetType() const override;
    const char* GetEditorName() const override { return "Skeletal Mesh Asset Editor"; }
    RUniquePtr<AssetEditor> CreateInstance() const override { return RMakeUnique<SkeletalMeshAssetEditor>(); }
    void Open(AssetHandle assetHandle) override;
    void Draw(ImGuiID documentDockId, ImGuiID documentClassId) override;

private:
    bool ReloadAsset();
    bool RebuildPreview();
    void EnsureLayout(const ImVec2& size);
    void DrawDetails();

private:
    SkeletalMeshAsset* m_Mesh = nullptr;
    SkeletonAsset* m_Skeleton = nullptr;
    Scene m_PreviewScene;
    Actor* m_PreviewActor = nullptr;
};

class AnimGraphAssetEditor : public PreviewAssetEditorBase
{
public:
    const Rebel::Core::Reflection::TypeInfo* GetSupportedAssetType() const override;
    const char* GetEditorName() const override { return "Anim Graph Editor"; }
    RUniquePtr<AssetEditor> CreateInstance() const override { return RMakeUnique<AnimGraphAssetEditor>(); }
    void Open(AssetHandle assetHandle) override;
    void Draw(ImGuiID documentDockId, ImGuiID documentClassId) override;

private:
    bool ReloadAsset();
    bool RebuildPreview();
    bool SaveAsset();
    void MarkDirty();
    void EnsureLayout(const ImVec2& size);
    void DrawGraph();
    void DrawMainGraph();
    void DrawStateMachineGraph();
    void DrawStateGraph();
    void DrawDetails();
    void DrawValidation();

private:
    AnimGraphAsset* m_Graph = nullptr;
    AnimationAsset* m_PreviewAnimation = nullptr;
    SkeletonAsset* m_PreviewSkeleton = nullptr;
    Scene m_PreviewScene;
    Actor* m_PreviewActor = nullptr;
    uint64 m_EditingStateMachineID = 0;
    uint64 m_EditingStateID = 0;
    uint64 m_SelectedStateID = 0;
    uint64 m_SelectedTransitionID = 0;
    uint64 m_SelectedAliasID = 0;
    float m_PlaybackTime = 0.0f;
    bool m_Playing = false;
    bool m_Looping = true;
    bool m_PreviewDirty = false;
    bool m_Dirty = false;
    EditorGraphDocumentState m_RootGraphDocument;
    EditorGraphDocumentState m_StateMachineGraphDocument;
    EditorGraphDocumentState m_StateGraphDocument;
    String m_StatusMessage;
};


