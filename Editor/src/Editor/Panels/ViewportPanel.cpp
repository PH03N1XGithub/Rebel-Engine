#include "Editor/Panels/ViewportPanel.h"

#include "Editor/UI/EditorImGui.h"
#include "EditorEngine.h"

#include "Engine/Framework/EnginePch.h"
#include "Engine/Framework/BaseEngine.h"
#include "Engine/Components/StaticMeshComponent.h"
#include "Engine/Components/SkeletalMeshComponent.h"
#include "Engine/Rendering/RenderModule.h"
#include "Engine/Assets/AssetManagerModule.h"
#include "Engine/Scene/Scene.h"

#include "imgui.h"
#include "ThirdParty/IconsFontAwesome6.h"
#include "ThirdParty/stb_image.h"

#include <filesystem>
#include <unordered_map>

namespace
{
namespace fs = std::filesystem;

struct IconTexture
{
    GLuint Handle = 0;
    float Width = 0.0f;
    float Height = 0.0f;
};

bool IsPointInRect(const ImVec2& point, const ImVec2& min, const ImVec2& max)
{
    return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y;
}

fs::path FindViewportUiAssetPath(const char* fileName)
{
    static const char* kPrefixes[] =
    {
        "Editor/assets/ui",
        "../Editor/assets/ui",
        "../../Editor/assets/ui",
        "../../../Editor/assets/ui",
        "../../../../Editor/assets/ui"
    };

    for (const char* prefix : kPrefixes)
    {
        const fs::path candidate = fs::path(prefix) / fileName;
        if (fs::exists(candidate))
            return candidate;
    }

    return {};
}

IconTexture LoadViewportIconTexture(const fs::path& path)
{
    if (path.empty())
        return {};

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_set_flip_vertically_on_load(0);
    stbi_uc* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
    stbi_set_flip_vertically_on_load(1);
    if (!pixels || width <= 0 || height <= 0)
    {
        if (pixels)
            stbi_image_free(pixels);
        return {};
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    stbi_image_free(pixels);
    return { texture, static_cast<float>(width), static_cast<float>(height) };
}

const IconTexture& GetViewportToolTexture(const char* relativePath)
{
    static std::unordered_map<std::string, IconTexture> cache;
    static IconTexture empty;
    if (!relativePath || relativePath[0] == '\0')
        return empty;

    const auto it = cache.find(relativePath);
    if (it != cache.end())
        return it->second;

    return cache.emplace(relativePath, LoadViewportIconTexture(FindViewportUiAssetPath(relativePath))).first->second;
}

void DrawFittedImage(ImDrawList* drawList, const IconTexture& texture, const ImVec2& min, const ImVec2& max)
{
    if (!drawList || texture.Handle == 0 || texture.Width <= 0.0f || texture.Height <= 0.0f)
        return;

    const float boxWidth = max.x - min.x;
    const float boxHeight = max.y - min.y;
    if (boxWidth <= 0.0f || boxHeight <= 0.0f)
        return;

    const float scale = std::min(boxWidth / texture.Width, boxHeight / texture.Height);
    const ImVec2 size(texture.Width * scale, texture.Height * scale);
    const ImVec2 offset((boxWidth - size.x) * 0.5f, (boxHeight - size.y) * 0.5f);
    drawList->AddImage(
        ((texture.Handle)),
        ImVec2(min.x + offset.x, min.y + offset.y),
        ImVec2(min.x + offset.x + size.x, min.y + offset.y + size.y));
}

bool DrawViewportToolButton(const char* id, const IconTexture& texture, const char* fallbackIcon, bool active, const char* tooltip)
{
    ImGui::PushID(id);
    const ImVec2 size(24.0f, 22.0f);
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const ImVec2 max(min.x + size.x, min.y + size.y);
    const bool pressed = ImGui::InvisibleButton("##ViewportTool", size);
    const bool hovered = ImGui::IsItemHovered();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImU32 fillColor = active ? IM_COL32(178, 128, 51, 224) : hovered ? IM_COL32(58, 61, 70, 196) : IM_COL32(20, 22, 27, 148);
    const ImU32 borderColor = active ? IM_COL32(200, 143, 61, 255) : hovered ? IM_COL32(101, 105, 116, 255) : IM_COL32(48, 52, 60, 180);
    drawList->AddRectFilled(min, max, fillColor, 4.0f);
    drawList->AddRect(min, max, borderColor, 4.0f);

    DrawFittedImage(drawList, texture, ImVec2(min.x + 4.0f, min.y + 3.0f), ImVec2(max.x - 4.0f, max.y - 3.0f));
    if (texture.Handle == 0 && fallbackIcon)
    {
        ImFont* iconFont = Rebel::Editor::UI::GetFont("Small");
        drawList->AddText(
            iconFont ? iconFont : ImGui::GetFont(),
            iconFont ? iconFont->LegacySize : ImGui::GetFontSize(),
            ImVec2(min.x + 6.0f, min.y + 4.0f),
            IM_COL32(224, 228, 236, 255),
            fallbackIcon);
    }

    if (hovered && tooltip)
        ImGui::SetTooltip("%s", tooltip);

    ImGui::PopID();
    return pressed;
}

EntityComponent* ResolvePickedComponent(Scene& scene, entt::entity pickedEntity)
{
    if (pickedEntity == entt::null)
        return nullptr;

    auto& registry = scene.GetRegistry();
    if (!registry.valid(pickedEntity))
        return nullptr;

    if (registry.all_of<StaticMeshComponent*>(pickedEntity))
        return registry.get<StaticMeshComponent*>(pickedEntity);

    if (registry.all_of<SkeletalMeshComponent*>(pickedEntity))
        return registry.get<SkeletalMeshComponent*>(pickedEntity);

    if (registry.all_of<SceneComponent*>(pickedEntity))
        return registry.get<SceneComponent*>(pickedEntity);

    return nullptr;
}

void SelectViewportComponent(EditorSelection& selection, Actor& owner, EntityComponent* hitComponent, const bool bDrillIntoComponent)
{
    const bool ownerWasSelected = selection.SelectedActor == &owner;
    SceneComponent* rootComponent = owner.GetRootComponent();
    if (!ownerWasSelected)
    {
        selection.SetSingleActor(&owner);
        selection.SelectedComponent = rootComponent;
        selection.SelectedComponentType = rootComponent ? rootComponent->GetType() : nullptr;
        return;
    }

    const bool bAlreadySelectingSubComponent =
        selection.SelectedComponent &&
        selection.SelectedComponent != rootComponent;

    if (!bDrillIntoComponent && !bAlreadySelectingSubComponent)
    {
        selection.SelectedComponent = rootComponent;
        selection.SelectedComponentType = rootComponent ? rootComponent->GetType() : nullptr;
        return;
    }

    selection.SelectedComponent = hitComponent ? hitComponent : rootComponent;
    selection.SelectedComponentType = selection.SelectedComponent ? selection.SelectedComponent->GetType() : nullptr;
}

void DrawStatusChip(const ImVec2& pos, const String& text, ImU32 backgroundColor, ImU32 textColor)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImFont* font = Rebel::Editor::UI::GetFont("Small");
    const float fontSize = font ? font->LegacySize : ImGui::GetFontSize();
    const ImVec2 textSize = font ? font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text.c_str()) : ImGui::CalcTextSize(text.c_str());
    const ImVec2 padding(8.0f, 5.0f);
    const ImVec2 max(pos.x + textSize.x + padding.x * 2.0f, pos.y + textSize.y + padding.y * 2.0f);

    Rebel::Editor::UI::DrawSoftShadow(drawList, pos, max, 5.0f, 3, 2.0f, 0.85f);
    drawList->AddRectFilled(pos, max, backgroundColor, 5.0f);
    drawList->AddText(
        font ? font : ImGui::GetFont(),
        fontSize,
        ImVec2(pos.x + padding.x, pos.y + padding.y),
        textColor,
        text.c_str());
}

ImVec2 GetStatusChipSize(const String& text)
{
    ImFont* font = Rebel::Editor::UI::GetFont("Small");
    const float fontSize = font ? font->LegacySize : ImGui::GetFontSize();
    const ImVec2 textSize = font ? font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text.c_str()) : ImGui::CalcTextSize(text.c_str());
    const ImVec2 padding(8.0f, 5.0f);
    return ImVec2(textSize.x + padding.x * 2.0f, textSize.y + padding.y * 2.0f);
}
}

ViewportPanel::ViewportPanel(EditorSelection& selection)
    : m_Selection(selection)
    , m_Gizmo(selection)
{
}

void ViewportPanel::SetDeltaTime(float dt)
{
    m_DeltaTime = dt;
}

bool ViewportPanel::HandleViewportPicking(
    RenderModule* renderer,
    const ImVec2& imagePos,
    const ImVec2& viewportSize,
    entt::entity& outHitEntity) const
{
    outHitEntity = entt::null;

    if (!renderer)
        return false;

    if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        return false;

    const int32 width = static_cast<int32>(viewportSize.x);
    const int32 height = static_cast<int32>(viewportSize.y);
    if (width <= 0 || height <= 0)
        return false;

    const ImVec2 imageMin = imagePos;
    const ImVec2 imageMax = ImVec2(imagePos.x + viewportSize.x, imagePos.y + viewportSize.y);
    const ImVec2 mouse = ImGui::GetMousePos();

    const bool mouseInImage =
        (mouse.x >= imageMin.x && mouse.x < imageMax.x &&
         mouse.y >= imageMin.y && mouse.y < imageMax.y);

    if (!mouseInImage)
        return false;

    int32 mouseX = static_cast<int32>(mouse.x - imageMin.x);
    int32 mouseY = static_cast<int32>(mouse.y - imageMin.y);

    if (mouseX < 0) mouseX = 0;
    if (mouseY < 0) mouseY = 0;
    if (mouseX >= width) mouseX = width - 1;
    if (mouseY >= height) mouseY = height - 1;

    const uint32 id = renderer->ReadPickID(static_cast<uint32>(mouseX), static_cast<uint32>(mouseY));
    if (id == 0)
        return false;

    outHitEntity = static_cast<entt::entity>(id - 1);
    return true;
}

bool ViewportPanel::DrawViewportTools(const ImVec2& imagePos, int fps)
{
    const ImVec2 overlayMin(imagePos.x + 10.0f, imagePos.y + 10.0f);
    const ImVec2 overlayMax(overlayMin.x + 154.0f, overlayMin.y + 30.0f);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    Rebel::Editor::UI::DrawSoftShadow(drawList, overlayMin, overlayMax, 6.0f, 4, 2.5f, 0.95f);
    drawList->AddRectFilled(overlayMin, overlayMax, IM_COL32(10, 12, 16, 148), 6.0f);
    drawList->AddRect(overlayMin, overlayMax, IM_COL32(92, 79, 56, 156), 6.0f);

    ImGui::SetCursorScreenPos(ImVec2(overlayMin.x + 5.0f, overlayMin.y + 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 0.0f));

    if (DrawViewportToolButton("Translate", GetViewportToolTexture("hazel/Viewport/MoveTool.png"), ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT, m_Gizmo.GetOperation() == GizmoOperation::Translate, "Translate (W)"))
        m_Gizmo.SetOperation(GizmoOperation::Translate);

    ImGui::SameLine();
    if (DrawViewportToolButton("Rotate", GetViewportToolTexture("hazel/Viewport/RotateTool.png"), ICON_FA_ARROWS_ROTATE, m_Gizmo.GetOperation() == GizmoOperation::Rotate, "Rotate (E)"))
        m_Gizmo.SetOperation(GizmoOperation::Rotate);

    ImGui::SameLine();
    if (DrawViewportToolButton("Scale", GetViewportToolTexture("hazel/Viewport/ScaleTool.png"), ICON_FA_EXPAND, m_Gizmo.GetOperation() == GizmoOperation::Scale, "Scale (R)"))
        m_Gizmo.SetOperation(GizmoOperation::Scale);

    ImGui::SameLine(0.0f, 6.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.13f, 0.15f, 0.50f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.22f, 0.25f, 0.78f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(153.0f / 255.0f, 112.0f / 255.0f, 46.0f / 255.0f, 0.92f));
    if (ImGui::Button(m_Gizmo.GetMode() == GizmoSpace::World ? ICON_FA_GLOBE : ICON_FA_COMPASS, ImVec2(34.0f, 22.0f)))
        m_Gizmo.SetMode(m_Gizmo.GetMode() == GizmoSpace::World ? GizmoSpace::Local : GizmoSpace::World);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s space", m_Gizmo.GetMode() == GizmoSpace::World ? "World" : "Local");
    ImGui::PopStyleColor(3);

    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(146, 152, 163, 255));
    ImGui::SameLine(0.0f, 6.0f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
    ImGui::TextUnformatted("Ctrl");
    ImGui::PopStyleColor();

    ImGui::PopStyleVar(2);
    const bool hovered = IsPointInRect(ImGui::GetMousePos(), overlayMin, overlayMax);
    (void)fps;
    return hovered;
}

void ViewportPanel::DrawViewportStatus(const ImVec2& imagePos, const ImVec2& imageMax, const ImVec2& viewportSize, int fps)
{
    float fov = 90.0f;
    float cameraSpeed = 5.0f;
    Camera* editorCamera = nullptr;
    if (EditorEngine* editor = dynamic_cast<EditorEngine*>(GEngine))
    {
        if (Camera* camera = editor->GetEditorCamera())
        {
            editorCamera = camera;
            fov = camera->GetZoom();
            cameraSpeed = camera->GetMovementSpeed();
        }
    }

    const std::string fovValue = std::to_string(static_cast<int>(glm::round(fov)));
    char speedBuffer[32];
    std::snprintf(speedBuffer, sizeof(speedBuffer), "%.1f", cameraSpeed);
    const String fovLabel = String(ICON_FA_VIDEO) + " FOV " + fovValue.c_str();
    const String speedLabel = String(ICON_FA_GAUGE) + " Speed " + speedBuffer;
    const String fpsLabel = String(ICON_FA_GAUGE_HIGH) + " " + std::to_string(fps).c_str() + " FPS";
    const String sizeLabel = String(ICON_FA_EXPAND) + " " +
        std::to_string(static_cast<int>(viewportSize.x)).c_str() + " x " +
        std::to_string(static_cast<int>(viewportSize.y)).c_str();
    const String selectionLabel = String(ICON_FA_ARROW_POINTER) + " " +
        std::to_string(m_Selection.GetSelectedActorCount()).c_str() + " selected";

    const ImVec2 fovChipSize = GetStatusChipSize(fovLabel);
    const ImVec2 speedChipSize = GetStatusChipSize(speedLabel);
    const float chipRight = imageMax.x - 12.0f;
    const ImVec2 fovChipPos(chipRight - fovChipSize.x, imagePos.y + 12.0f);
    const ImVec2 speedChipPos(chipRight - speedChipSize.x, imagePos.y + 42.0f);

    DrawStatusChip(fovChipPos, fovLabel, IM_COL32(16, 18, 22, 210), IM_COL32(214, 219, 229, 255));
    DrawStatusChip(speedChipPos, speedLabel, IM_COL32(16, 18, 22, 210), IM_COL32(214, 219, 229, 255));

    ImGui::SetCursorScreenPos(fovChipPos);
    ImGui::InvisibleButton("##ViewportCameraSettingsButton", ImVec2(fovChipSize.x, speedChipSize.y + 30.0f));
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        ImGui::OpenPopup("ViewportCameraSettings");

    if (ImGui::BeginPopup("ViewportCameraSettings"))
    {
        float editableFov = fov;
        float editableSpeed = cameraSpeed;
        ImGui::SetNextItemWidth(180.0f);
        if (ImGui::SliderFloat("FOV", &editableFov, 20.0f, 120.0f, "%.0f") && editorCamera)
            editorCamera->SetZoom(editableFov);
        ImGui::SetNextItemWidth(180.0f);
        if (ImGui::SliderFloat("Speed", &editableSpeed, 0.5f, 50.0f, "%.1f") && editorCamera)
            editorCamera->SetMovementSpeed(editableSpeed);
        ImGui::EndPopup();
    }

    DrawStatusChip(ImVec2(imagePos.x + 12.0f, imageMax.y - 34.0f), fpsLabel, IM_COL32(16, 18, 22, 220), IM_COL32(214, 219, 229, 255));

    ImFont* font = Rebel::Editor::UI::GetFont("Small");
    const float fontSize = font ? font->LegacySize : ImGui::GetFontSize();
    const ImVec2 sizeText = font ? font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, sizeLabel.c_str()) : ImGui::CalcTextSize(sizeLabel.c_str());
    const ImVec2 selText = font ? font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, selectionLabel.c_str()) : ImGui::CalcTextSize(selectionLabel.c_str());

    DrawStatusChip(
        ImVec2(imageMax.x - sizeText.x - 24.0f, imagePos.y + 72.0f),
        sizeLabel,
        IM_COL32(16, 18, 22, 210),
        IM_COL32(153, 161, 174, 255));

    DrawStatusChip(
        ImVec2(imageMax.x - selText.x - 24.0f, imagePos.y + 102.0f),
        selectionLabel,
        IM_COL32(16, 18, 22, 210),
        IM_COL32(230, 233, 238, 255));
}

void ViewportPanel::Draw()
{
    using namespace Rebel::Editor::UI;

    if (Scene* activeScene = GEngine->GetActiveScene())
        m_Selection.SyncWithScene(activeScene);

    RenderModule* renderer = GEngine->GetModuleManager().GetModule<RenderModule>();
    if (!renderer)
    {
        DrawEmptyState(ICON_FA_TRIANGLE_EXCLAMATION, "Viewport unavailable", "RenderModule is not available, so the editor viewport cannot present a frame.");
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.10f, 1.0f));
    if (ImGui::BeginChild("ViewportSurface", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        const ImVec2 surfaceSize = ImGui::GetContentRegionAvail();
        if (surfaceSize.x <= 0.0f || surfaceSize.y <= 0.0f)
        {
            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            return;
        }

        renderer->SetViewportSize(static_cast<int32>(surfaceSize.x), static_cast<int32>(surfaceSize.y));
        GLuint tex = renderer->GetViewportTexture();

        ImGui::Image((ImTextureID)(intptr_t)tex, surfaceSize);

        const ImVec2 imagePos = ImGui::GetItemRectMin();
        const ImVec2 imageMax = ImGui::GetItemRectMax();
        ImGuiIO& io = ImGui::GetIO();

        const bool imageHovered =
            ImGui::IsItemHovered() ||
            (io.MousePos.x >= imagePos.x &&
             io.MousePos.x <= imageMax.x &&
             io.MousePos.y >= imagePos.y &&
             io.MousePos.y <= imageMax.y);

        const int fps = (m_DeltaTime > 0.000001f) ? static_cast<int>(1.0f / m_DeltaTime) : 0;
        const bool toolsHovered = DrawViewportTools(imagePos, fps);
        const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        const bool viewportInteractive = imageHovered && !toolsHovered;
        m_Inside = viewportInteractive;

        const ImU32 borderColor = focused
            ? IM_COL32(178, 128, 51, 255)
            : (m_Inside ? IM_COL32(86, 92, 102, 255) : IM_COL32(38, 40, 46, 255));
        ImGui::GetWindowDrawList()->AddRect(imagePos, imageMax, borderColor, 0.0f, 0, focused ? 2.0f : 1.0f);

        DrawViewportStatus(imagePos, imageMax, surfaceSize, fps);

        if (EditorEngine* editor = dynamic_cast<EditorEngine*>(GEngine))
        {
            const bool rightMouseDown = InputModule::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);
            if (viewportInteractive && rightMouseDown && !editor->IsEditorCameraCaptured())
                editor->SetEditorCameraCaptured(true);
            else if (!rightMouseDown && editor->IsEditorCameraCaptured())
                editor->SetEditorCameraCaptured(false);
        }

        ImGui::SetItemAllowOverlap();
        m_Gizmo.SetViewportHovered(viewportInteractive || focused);
        m_Gizmo.Draw(imagePos, surfaceSize);

        if (!m_Gizmo.IsUsing() && viewportInteractive)
        {
            entt::entity hit = entt::null;
            if (HandleViewportPicking(renderer, imagePos, surfaceSize, hit))
            {
                Scene* activeScene = GEngine->GetActiveScene();
                EntityComponent* hitComponent = activeScene ? ResolvePickedComponent(*activeScene, hit) : nullptr;
                Actor* hitActor = hitComponent ? hitComponent->GetOwner() : (activeScene ? activeScene->GetActor(hit) : nullptr);
                const bool multiSelectToggle = ImGui::GetIO().KeyCtrl;
                if (multiSelectToggle)
                    m_Selection.ToggleActor(hitActor);
                else if (hitActor)
                    SelectViewportComponent(
                        m_Selection,
                        *hitActor,
                        hitComponent,
                        ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left));
                else
                    m_Selection.Clear();
            }
            else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::GetIO().KeyCtrl)
            {
                m_Selection.Clear();
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    AssetHandle testMeshID;
    for (auto& pair : GEngine->GetModuleManager().GetModule<AssetManagerModule>()->GetRegistry().GetAll())
    {
        if (pair.Value.Path == "assets/Test.mesh")
        {
            testMeshID = pair.Value.ID;
            break;
        }
    }
    (void)testMeshID;
}
