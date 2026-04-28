#include "Editor/Panels/PrefabEditorPanel.h"

#include "Editor/Core/EditorCommandDispatcher.h"
#include "Editor/Core/EditorCommands.h"
#include "EditorEngine.h"

#include "Engine/Animation/AnimationModule.h"
#include "Engine/Assets/AssetManagerModule.h"
#include "Engine/Assets/PrefabAsset.h"
#include "Engine/Framework/BaseEngine.h"
#include "Engine/Framework/EngineReflectionExtensions.h"
#include "Engine/Framework/Window.h"
#include "Engine/Input/InputModule.h"
#include "Engine/Rendering/RenderModule.h"
#include "Engine/Scene/Actor.h"
#include "Engine/Scene/ActorTemplateSerializer.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "ThirdParty/IconsFontAwesome6.h"

DEFINE_LOG_CATEGORY(PrefabEditorLog)

namespace
{
constexpr float kCameraSpeedStep = 0.5f;

String MakePrefabEditorName(const char* label, const char* idPrefix, const AssetHandle assetHandle)
{
    return String(label) +
        String("###") +
        String(idPrefix) +
        String("_") +
        String(std::to_string(static_cast<uint64>(assetHandle)).c_str());
}

String MakePrefabEditorId(const char* idPrefix, const AssetHandle assetHandle)
{
    return String(idPrefix) +
        String("_") +
        String(std::to_string(static_cast<uint64>(assetHandle)).c_str());
}

void PreparePrefabPreviewScene(Scene& previewScene)
{
    previewScene.UpdateTransforms();

    AnimationModule* animationModule = GEngine->GetModuleManager().GetModule<AnimationModule>();
    if (!animationModule)
        return;

    Scene* activeScene = GEngine->GetActiveScene();
    animationModule->SetSceneContext(&previewScene);
    animationModule->Tick(0.0f);
    animationModule->SetSceneContext(activeScene);
}

bool IsPrefabObjectComponentType(const Rebel::Core::Reflection::ComponentTypeInfo& info)
{
    return info.Type && info.Type->IsA(EntityComponent::StaticType());
}

const Rebel::Core::Reflection::ComponentTypeInfo* FindComponentInfoForType(const Rebel::Core::Reflection::TypeInfo* type)
{
    if (!type)
        return nullptr;

    for (const auto& info : ComponentRegistry::Get().GetComponents())
    {
        if (info.Type == type)
            return &info;
    }

    return nullptr;
}

void DrawPrefabComponentHierarchy(
    Actor& actor,
    Rebel::Core::Reflection::TypeInfo*& selectedComponentType,
    EntityComponent*& selectedComponent)
{
    ImGuiTreeNodeFlags rootFlags =
          ImGuiTreeNodeFlags_OpenOnArrow
        | ImGuiTreeNodeFlags_OpenOnDoubleClick
        | ImGuiTreeNodeFlags_DefaultOpen;

    const bool rootOpen = ImGui::TreeNodeEx("PrefabRoot", rootFlags, "%s Prefab Root", ICON_FA_CIRCLE_NODES);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
    {
        selectedComponentType = nullptr;
        selectedComponent = nullptr;
    }

    if (!rootOpen)
        return;

    for (auto& info : ComponentRegistry::Get().GetComponents())
    {
        if (!info.HasFn || !info.GetFn || !info.Type)
            continue;
        if (IsPrefabObjectComponentType(info))
            continue;
        if (!info.HasFn(actor))
            continue;
        if (info.Name == "SceneComponent" || info.Name == "IDComponent" || info.Name == "NameComponent")
            continue;

        ImGuiTreeNodeFlags flags =
              ImGuiTreeNodeFlags_Leaf
            | ImGuiTreeNodeFlags_NoTreePushOnOpen;

        if (selectedComponentType == info.Type)
            flags |= ImGuiTreeNodeFlags_Selected;

        ImGui::TreeNodeEx(info.Name.c_str(), flags, "%s %s", ICON_FA_PUZZLE_PIECE, info.Name.c_str());
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            selectedComponentType = const_cast<Rebel::Core::Reflection::TypeInfo*>(info.Type);
            selectedComponent = nullptr;
        }
    }

    int objectComponentIndex = 0;
    for (const auto& componentPtr : actor.GetObjectComponents())
    {
        EntityComponent* component = componentPtr.Get();
        if (!component)
            continue;

        const auto* info = FindComponentInfoForType(component->GetType());
        if (!info || info->Name == "SceneComponent" || info->Name == "IDComponent" || info->Name == "NameComponent")
            continue;

        ImGuiTreeNodeFlags flags =
              ImGuiTreeNodeFlags_Leaf
            | ImGuiTreeNodeFlags_NoTreePushOnOpen;

        if (selectedComponent == component)
            flags |= ImGuiTreeNodeFlags_Selected;

        const String displayName = component->GetEditorName().length() > 0 ? component->GetEditorName() : info->Name;
        const String label = info->Name + "##PrefabObjectComponent" + String(std::to_string(objectComponentIndex++).c_str());
        ImGui::TreeNodeEx(label.c_str(), flags, "%s %s", ICON_FA_PUZZLE_PIECE, displayName.c_str());
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            selectedComponentType = const_cast<Rebel::Core::Reflection::TypeInfo*>(info->Type);
            selectedComponent = component;
        }
    }

    ImGui::TreePop();
}

ImVec2 CalcOverlayTextSize(const char* text)
{
    const ImVec2 textSize = ImGui::CalcTextSize(text);
    return ImVec2(textSize.x + 16.0f, textSize.y + 10.0f);
}

void DrawOverlayChip(const ImVec2& pos, const char* text)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 size = CalcOverlayTextSize(text);
    const ImVec2 max(pos.x + size.x, pos.y + size.y);
    drawList->AddRectFilled(pos, max, IM_COL32(16, 18, 22, 210), 5.0f);
    drawList->AddRect(pos, max, IM_COL32(58, 61, 70, 210), 5.0f);
    drawList->AddText(ImVec2(pos.x + 8.0f, pos.y + 5.0f), IM_COL32(214, 219, 229, 255), text);
}
}

const Rebel::Core::Reflection::TypeInfo* PrefabEditorPanel::GetSupportedAssetType() const
{
    return PrefabAsset::StaticType();
}

void PrefabEditorPanel::Open(const AssetHandle prefabHandle)
{
    m_PrefabHandle = prefabHandle;
    m_IsOpen = ReloadPrefab();
}

bool PrefabEditorPanel::ReloadPrefab()
{
    auto* assetModule = GEngine->GetModuleManager().GetModule<AssetManagerModule>();
    if (!assetModule || !IsValidAssetHandle(m_PrefabHandle))
        return false;

    m_PrefabAsset = dynamic_cast<PrefabAsset*>(assetModule->GetManager().Load(m_PrefabHandle));
    if (!m_PrefabAsset)
        return false;

    return RebuildPreviewActor();
}

bool PrefabEditorPanel::RebuildPreviewActor()
{
    m_PreviewScene.Clear();
    m_SelectedComponentType = nullptr;
    m_SelectedComponent = nullptr;
    m_PreviewActor = nullptr;
    m_PreviewViewportHovered = false;
    m_LastLoggedHovered = false;
    m_LastLoggedRightMouseDown = false;
    m_DebugLogCooldown = 0.0f;

    if (!m_PrefabAsset)
        return false;

    m_PreviewActor = m_PreviewScene.SpawnActorFromPrefab(*m_PrefabAsset);
    m_LayoutInitialized = false;
    ResetPreviewCamera();
    return m_PreviewActor != nullptr;
}

void PrefabEditorPanel::ResetPreviewCamera()
{
    Vector3 target = Vector3(0.0f, 0.0f, 0.0f);
    if (m_PreviewActor && m_PreviewActor->IsValid())
        target = m_PreviewActor->GetActorLocation();

    m_PreviewYaw = 45.0f;
    m_PreviewPitch = -20.0f;
    m_PreviewMoveSpeed = 5.0f;
    m_PreviewPosition = target + Vector3(-4.0f, -4.0f, 2.5f);
    m_PreviewCamera = Camera(m_PreviewPosition, m_PreviewYaw, m_PreviewPitch);
    m_PreviewCamera.SetZoom(60.0f);
    m_PreviewCamera.SetMovementSpeed(m_PreviewMoveSpeed);
}

void PrefabEditorPanel::EnsureLayout(const ImVec2& size)
{
    const String dockspaceName = MakePrefabEditorId("PrefabEditorDockspace", m_PrefabHandle);
    const ImGuiID dockspaceId = ImGui::GetID(dockspaceName.c_str());
    if (m_LayoutInitialized || dockspaceId == 0 || size.x <= 1.0f || size.y <= 1.0f)
        return;

    const String viewportWindow = MakePrefabEditorName("Viewport", "PrefabEditorViewport", m_PrefabHandle);
    const String componentsWindow = MakePrefabEditorName("Components", "PrefabEditorComponents", m_PrefabHandle);
    const String detailsWindow = MakePrefabEditorName("Details", "PrefabEditorDetails", m_PrefabHandle);

    ImGuiDockNode* existingNode = ImGui::DockBuilderGetNode(dockspaceId);
    if (existingNode && existingNode->ChildNodes[0] != nullptr)
    {
        m_LayoutInitialized = true;
        return;
    }

    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, size);

    ImGuiID dockLeft = 0;
    ImGuiID dockBottomLeft = 0;
    ImGuiID dockMain = dockspaceId;
    ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.24f, &dockLeft, &dockMain);
    ImGui::DockBuilderSplitNode(dockLeft, ImGuiDir_Down, 0.52f, &dockBottomLeft, &dockLeft);

    ImGui::DockBuilderDockWindow(componentsWindow.c_str(), dockLeft);
    ImGui::DockBuilderDockWindow(detailsWindow.c_str(), dockBottomLeft);
    ImGui::DockBuilderDockWindow(viewportWindow.c_str(), dockMain);
    ImGui::DockBuilderFinish(dockspaceId);
    m_LayoutInitialized = true;
}

void PrefabEditorPanel::UpdatePreviewCamera(const ImVec2& viewportSize)
{
    if (viewportSize.x <= 0.0f || viewportSize.y <= 0.0f)
        return;

    ImGuiIO& io = ImGui::GetIO();
    const bool hovered = m_PreviewViewportHovered;
    const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    const bool rightMouseDown = InputModule::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);
    const InputModule::MouseDelta mouseDelta = InputModule::GetMouseDelta();
    EditorEngine* editor = dynamic_cast<EditorEngine*>(GEngine);

    m_DebugLogCooldown -= io.DeltaTime;
    if (hovered != m_LastLoggedHovered)
    {
        RB_LOG(
            PrefabEditorLog,
            info,
            "Prefab viewport hover changed: hovered={} focused={} fly={} cursorDisabled={} editorCaptured={}",
            hovered ? 1 : 0,
            focused ? 1 : 0,
            m_PreviewFlyActive ? 1 : 0,
            (GEngine && GEngine->GetWindow() && GEngine->GetWindow()->IsCursorDisabled()) ? 1 : 0,
            (editor && editor->IsEditorCameraCaptured()) ? 1 : 0);
        m_LastLoggedHovered = hovered;
    }

    if (rightMouseDown != m_LastLoggedRightMouseDown)
    {
        RB_LOG(
            PrefabEditorLog,
            info,
            "Prefab viewport RMB changed: down={} hovered={} focused={} fly={} mouseDelta=({}, {})",
            rightMouseDown ? 1 : 0,
            hovered ? 1 : 0,
            focused ? 1 : 0,
            m_PreviewFlyActive ? 1 : 0,
            mouseDelta.x,
            mouseDelta.y);
        m_LastLoggedRightMouseDown = rightMouseDown;
    }

    if (!hovered && !m_PreviewFlyActive)
        return;

    if (hovered && rightMouseDown && !m_PreviewFlyActive)
    {
        m_PreviewFlyActive = true;
        if (editor)
            editor->SetEditorCameraCaptured(true);
        RB_LOG(PrefabEditorLog, info, "Prefab fly mode entered");
    }
    else if (!rightMouseDown && m_PreviewFlyActive)
    {
        m_PreviewFlyActive = false;
        if (editor)
            editor->SetEditorCameraCaptured(false);
        RB_LOG(PrefabEditorLog, info, "Prefab fly mode exited");
    }

    if (!focused && !hovered && !m_PreviewFlyActive)
        return;

    if ((hovered || m_PreviewFlyActive) && m_DebugLogCooldown <= 0.0f)
    {
        RB_LOG(
            PrefabEditorLog,
            info,
            "Prefab camera tick: hovered={} focused={} fly={} rmb={} delta=({}, {}) keys(WASDQE)=({},{},{},{},{},{}) fov={} yaw={} pitch={} cursorDisabled={} editorCaptured={}",
            hovered ? 1 : 0,
            focused ? 1 : 0,
            m_PreviewFlyActive ? 1 : 0,
            rightMouseDown ? 1 : 0,
            mouseDelta.x,
            mouseDelta.y,
            InputModule::IsKeyPressed(GLFW_KEY_W) ? 1 : 0,
            InputModule::IsKeyPressed(GLFW_KEY_A) ? 1 : 0,
            InputModule::IsKeyPressed(GLFW_KEY_S) ? 1 : 0,
            InputModule::IsKeyPressed(GLFW_KEY_D) ? 1 : 0,
            InputModule::IsKeyPressed(GLFW_KEY_Q) ? 1 : 0,
            InputModule::IsKeyPressed(GLFW_KEY_E) ? 1 : 0,
            m_PreviewCamera.GetZoom(),
            m_PreviewYaw,
            m_PreviewPitch,
            (GEngine && GEngine->GetWindow() && GEngine->GetWindow()->IsCursorDisabled()) ? 1 : 0,
            (editor && editor->IsEditorCameraCaptured()) ? 1 : 0);
        m_DebugLogCooldown = 0.25f;
    }

    if (io.MouseWheel != 0.0f)
    {
        m_PreviewMoveSpeed = FMath::max(kCameraSpeedStep, glm::round((m_PreviewMoveSpeed + io.MouseWheel * kCameraSpeedStep) / kCameraSpeedStep) * kCameraSpeedStep);
        m_PreviewCamera.SetMovementSpeed(m_PreviewMoveSpeed);
    }

    if (m_PreviewFlyActive)
    {
        m_PreviewYaw -= mouseDelta.x * 0.15f;
        m_PreviewPitch = FMath::clamp(m_PreviewPitch - mouseDelta.y * 0.15f, -80.0f, 80.0f);
    }

    if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
    {
        const ImVec2 delta = io.MouseDelta;
        const CameraView cameraView = m_PreviewCamera.GetCameraView(
            viewportSize.y > 1.0f ? viewportSize.x / viewportSize.y : 1.0f);
        const Vector3 forward = FMath::normalize(Vector3(
            cameraView.View[0][2] * -1.0f,
            cameraView.View[1][2] * -1.0f,
            cameraView.View[2][2] * -1.0f));
        Vector3 right = FMath::normalize(FMath::cross(forward, Vector3(0.0f, 0.0f, 1.0f)));
        if (FMath::length(right) < 1e-3f)
            right = Vector3(1.0f, 0.0f, 0.0f);
        const Vector3 up = FMath::normalize(FMath::cross(right, forward));
        const float panScale = 0.01f;
        m_PreviewPosition -= right * (delta.x * panScale);
        m_PreviewPosition += up * (delta.y * panScale);
    }

    const float yawRad = FMath::radians(m_PreviewYaw);
    const float pitchRad = FMath::radians(m_PreviewPitch);
    const Vector3 forward(
        FMath::cos(pitchRad) * FMath::cos(yawRad),
        FMath::cos(pitchRad) * FMath::sin(yawRad),
        FMath::sin(pitchRad));
    const Vector3 normalizedForward = FMath::normalize(forward);
    const Vector3 up = Vector3(0.0f, 0.0f, 1.0f);
    Vector3 right = FMath::normalize(FMath::cross(normalizedForward, up));
    if (FMath::length(right) < 1e-3f)
        right = Vector3(1.0f, 0.0f, 0.0f);
    const Vector3 cameraUp = FMath::normalize(FMath::cross(right, normalizedForward));

    if (m_PreviewFlyActive)
    {
        const float dt = FMath::max(io.DeltaTime, 1.0f / 240.0f);
        float moveSpeed = InputModule::IsKeyPressed(GLFW_KEY_LEFT_SHIFT) ? m_PreviewMoveSpeed * 2.0f : m_PreviewMoveSpeed;
        moveSpeed *= dt;

        if (InputModule::IsKeyPressed(GLFW_KEY_W)) m_PreviewPosition += normalizedForward * moveSpeed;
        if (InputModule::IsKeyPressed(GLFW_KEY_S)) m_PreviewPosition -= normalizedForward * moveSpeed;
        if (InputModule::IsKeyPressed(GLFW_KEY_A)) m_PreviewPosition -= right * moveSpeed;
        if (InputModule::IsKeyPressed(GLFW_KEY_D)) m_PreviewPosition += right * moveSpeed;
        if (InputModule::IsKeyPressed(GLFW_KEY_E)) m_PreviewPosition += up * moveSpeed;
        if (InputModule::IsKeyPressed(GLFW_KEY_Q)) m_PreviewPosition -= up * moveSpeed;
    }

    m_PreviewCamera.SetPosition(m_PreviewPosition);
    m_PreviewCamera.SetRotation(m_PreviewYaw, m_PreviewPitch);
}

void PrefabEditorPanel::EndPreviewCameraInteraction()
{
    m_PreviewFlyActive = false;
    m_PreviewViewportHovered = false;
    if (EditorEngine* editor = dynamic_cast<EditorEngine*>(GEngine))
        editor->SetEditorCameraCaptured(false);
}

bool PrefabEditorPanel::SavePrefab()
{
    if (!m_PrefabAsset || !m_PreviewActor)
        return false;

    m_PrefabAsset->m_ActorTypeName =
        m_PreviewActor->GetType() ? m_PreviewActor->GetType()->Name : "Actor";

    if (!ActorTemplateSerializer::SerializeActorTemplateToString(
            *m_PreviewActor,
            m_PrefabAsset->m_TemplateYaml,
            { false }))
    {
        return false;
    }

    auto* assetModule = GEngine->GetModuleManager().GetModule<AssetManagerModule>();
    if (!assetModule)
        return false;

    if (m_PrefabAsset->Path.length() == 0)
    {
        RB_LOG(PrefabEditorLog, error, "Failed to save prefab: asset path is empty.");
        return false;
    }

    const bool bSaved = assetModule->SaveAssetToFile(m_PrefabAsset->Path + ".rasset", *m_PrefabAsset);
    if (bSaved)
    {
        RB_LOG(PrefabEditorLog, info, "Saved prefab '{}'", m_PrefabAsset->Path.c_str());
    }
    else
    {
        RB_LOG(PrefabEditorLog, error, "Failed to save prefab '{}'", m_PrefabAsset->Path.c_str());
    }
    return bSaved;
}

void PrefabEditorPanel::Close()
{
    m_PreviewScene.Clear();
    m_PreviewActor = nullptr;
    m_PrefabAsset = nullptr;
    m_PrefabHandle = 0;
    m_IsOpen = false;
    m_LayoutInitialized = false;
    EndPreviewCameraInteraction();
    m_SelectedComponentType = nullptr;
    m_SelectedComponent = nullptr;
}

void PrefabEditorPanel::Draw(const ImGuiID documentDockId, const ImGuiID documentClassId)
{
    if (!m_IsOpen)
        return;

    String title = "Prefab Editor";
    if (m_PrefabAsset && m_PrefabAsset->Path.length() > 0)
        title = title + " - " + m_PrefabAsset->Path;

    ImGuiWindowClass windowClass{};
    windowClass.ClassId = documentClassId;
    windowClass.DockingAllowUnclassed = false;
    ImGui::SetNextWindowClass(&windowClass);
    if (documentDockId != 0)
        ImGui::SetNextWindowDockID(documentDockId, ImGuiCond_Appearing);
    if (m_RequestFocus)
    {
        ImGui::SetNextWindowFocus();
        m_RequestFocus = false;
    }

    bool open = m_IsOpen;
    if (!ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_MenuBar))
    {
        ImGui::End();
        if (!open)
            Close();
        return;
    }

    if (!open)
    {
        ImGui::End();
        Close();
        return;
    }

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::Button((String(ICON_FA_FLOPPY_DISK) + " Save").c_str()))
            SavePrefab();

        ImGui::SameLine();
        if (ImGui::Button((String(ICON_FA_ARROWS_ROTATE) + " Reload").c_str()))
            ReloadPrefab();

        ImGui::SameLine();
        if (ImGui::Button((String(ICON_FA_CAMERA) + " Reset Camera").c_str()))
            ResetPreviewCamera();

        ImGui::SameLine();
        ImGui::Checkbox("Grid", &m_bShowPreviewGrid);

        ImGui::SameLine();
        ImGui::Checkbox("Collision", &m_bShowPreviewCollision);

        ImGui::EndMenuBar();
    }

    if (!m_PreviewActor || !m_PreviewActor->IsValid())
    {
        ImGui::TextDisabled("Prefab preview could not be created.");
        ImGui::End();
        return;
    }

    ImGui::BeginChild(
        "PrefabEditorDockHost",
        ImVec2(0.0f, 0.0f),
        ImGuiChildFlags_None,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const ImVec2 dockSize = ImGui::GetContentRegionAvail();
    const String dockspaceName = MakePrefabEditorId("PrefabEditorDockspace", m_PrefabHandle);
    ImGui::DockSpace(ImGui::GetID(dockspaceName.c_str()), ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    EnsureLayout(dockSize);
    ImGui::EndChild();
    ImGui::End();

    ImGuiWindowClass panelClass{};
    panelClass.ClassId = documentClassId;
    panelClass.DockingAllowUnclassed = false;

    const String viewportWindow = MakePrefabEditorName("Viewport", "PrefabEditorViewport", m_PrefabHandle);
    const String componentsWindow = MakePrefabEditorName("Components", "PrefabEditorComponents", m_PrefabHandle);
    const String detailsWindow = MakePrefabEditorName("Details", "PrefabEditorDetails", m_PrefabHandle);

    ImGui::SetNextWindowClass(&panelClass);

    if (ImGui::Begin(
            viewportWindow.c_str(),
            nullptr,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        RenderModule* renderer = GEngine->GetModuleManager().GetModule<RenderModule>();
        const ImVec2 previewSize = ImGui::GetContentRegionAvail();
        if (renderer && previewSize.x > 32.0f && previewSize.y > 32.0f)
        {
            const uint32 previewWidthPx = static_cast<uint32>(previewSize.x);
            const uint32 previewHeightPx = static_cast<uint32>(previewSize.y);
            const float aspect = previewSize.y > 1.0f ? previewSize.x / previewSize.y : 1.0f;
            UpdatePreviewCamera(previewSize);
            PreparePrefabPreviewScene(m_PreviewScene);
            renderer->RenderScenePreview(
                m_PreviewScene,
                m_PreviewCamera.GetCameraView(aspect),
                previewWidthPx,
                previewHeightPx,
                m_bShowPreviewGrid,
                m_bShowPreviewCollision);

            ImGui::Image(
                (ImTextureID)(intptr_t)renderer->GetPreviewTexture(),
                previewSize);
            m_PreviewViewportHovered = ImGui::IsItemHovered();

            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();
            const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
            const ImU32 borderColor = focused ? IM_COL32(178, 128, 51, 255) : IM_COL32(58, 61, 70, 255);
            ImGui::GetWindowDrawList()->AddRect(min, max, borderColor, 0.0f, 0, focused ? 2.0f : 1.0f);

            ImGui::SetCursorScreenPos(ImVec2(min.x + 12.0f, min.y + 12.0f));
            ImGui::BeginGroup();
            ImGui::TextDisabled("Prefab Viewport");
            ImGui::TextDisabled("RMB Look + WASDQE Move");
            ImGui::TextDisabled("MMB Pan  |  Wheel Speed");
            ImGui::EndGroup();

            char fovLabel[32];
            char speedLabel[32];
            std::snprintf(fovLabel, sizeof(fovLabel), "FOV %.0f", m_PreviewCamera.GetZoom());
            std::snprintf(speedLabel, sizeof(speedLabel), "Speed %.1f", m_PreviewMoveSpeed);

            const ImVec2 fovSize = CalcOverlayTextSize(fovLabel);
            const ImVec2 speedSize = CalcOverlayTextSize(speedLabel);
            const ImVec2 fovPos(max.x - fovSize.x - 12.0f, min.y + 12.0f);
            const ImVec2 speedPos(max.x - speedSize.x - 12.0f, min.y + 42.0f);
            DrawOverlayChip(fovPos, fovLabel);
            DrawOverlayChip(speedPos, speedLabel);

            ImGui::SetCursorScreenPos(fovPos);
            ImGui::InvisibleButton("##PrefabCameraSettingsButton", ImVec2(glm::max(fovSize.x, speedSize.x), speedPos.y + speedSize.y - fovPos.y));
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                ImGui::OpenPopup("PrefabCameraSettings");

            if (ImGui::BeginPopup("PrefabCameraSettings"))
            {
                float editableFov = m_PreviewCamera.GetZoom();
                float editableSpeed = m_PreviewMoveSpeed;
                ImGui::SetNextItemWidth(180.0f);
                if (ImGui::SliderFloat("FOV", &editableFov, 20.0f, 120.0f, "%.0f"))
                    m_PreviewCamera.SetZoom(editableFov);
                ImGui::SetNextItemWidth(180.0f);
                if (ImGui::SliderFloat("Speed", &editableSpeed, 0.5f, 50.0f, "%.1f"))
                {
                    m_PreviewMoveSpeed = glm::max(kCameraSpeedStep, glm::round(editableSpeed / kCameraSpeedStep) * kCameraSpeedStep);
                    m_PreviewCamera.SetMovementSpeed(m_PreviewMoveSpeed);
                }
                ImGui::EndPopup();
            }
        }
        else
        {
            EndPreviewCameraInteraction();
            m_PreviewViewportHovered = false;
            ImGui::TextDisabled("Prefab preview unavailable.");
        }
    }
    else
    {
        EndPreviewCameraInteraction();
    }
    ImGui::End();

    ImGui::SetNextWindowClass(&panelClass);
    if (ImGui::Begin(componentsWindow.c_str()))
    {
        DrawPrefabComponentHierarchy(*m_PreviewActor, m_SelectedComponentType, m_SelectedComponent);

        if (ImGui::Button((String(ICON_FA_CIRCLE_PLUS) + " Add Component").c_str(), ImVec2(-FLT_MIN, 0.0f)))
            ImGui::OpenPopup("PrefabAddComponentPopup");

        if (ImGui::BeginPopup("PrefabAddComponentPopup"))
        {
            for (auto& info : ComponentRegistry::Get().GetComponents())
            {
                if (info.Name == "SceneComponent" || info.Name == "TagComponent" || info.Name == "ActorTagComponent")
                    continue;

                const bool has = !IsPrefabObjectComponentType(info) && info.HasFn && info.HasFn(*m_PreviewActor);
                if (has)
                {
                    ImGui::BeginDisabled();
                    ImGui::MenuItem(info.Name.c_str());
                    ImGui::EndDisabled();
                }
                else if (ImGui::MenuItem(info.Name.c_str()))
                {
                    EditorCommandDispatcher::Execute(std::make_unique<AddComponentCommand>(m_PreviewActor, &info));
                    m_SelectedComponentType = const_cast<Rebel::Core::Reflection::TypeInfo*>(info.Type);
                    m_SelectedComponent = nullptr;
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndPopup();
        }
    }
    ImGui::End();

    ImGui::SetNextWindowClass(&panelClass);
    if (ImGui::Begin(detailsWindow.c_str()))
    {
        if (m_PreviewActor->HasComponent<NameComponent>())
        {
            auto& nameComp = m_PreviewActor->GetComponent<NameComponent>();
            char buffer[256];
            strncpy_s(buffer, nameComp.Name.c_str(), sizeof(buffer));
            buffer[sizeof(buffer) - 1] = '\0';

            const String beforeName = nameComp.Name;
            ImGui::TextDisabled("Actor Name");
            const bool changed = ImGui::InputText("##PrefabActorName", buffer, sizeof(buffer));
            if (ImGui::IsItemActivated())
                EditorCommandDispatcher::BeginTransaction("Rename Prefab Actor");

            if (changed)
            {
                EditorCommandDispatcher::Execute(std::make_unique<RenameActorCommand>(
                    m_PreviewActor,
                    beforeName,
                    String(buffer)));
            }

            if (ImGui::IsItemDeactivatedAfterEdit())
                EditorCommandDispatcher::CommitTransaction();
        }

        ImGui::Separator();
        m_PropertyEditor.DrawComponentsForActor(*m_PreviewActor, m_SelectedComponentType, m_SelectedComponent);
    }
    ImGui::End();
    return;
}
