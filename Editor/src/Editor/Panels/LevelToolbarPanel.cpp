#include "Editor/Panels/LevelToolbarPanel.h"

#include "Engine/Framework/EnginePch.h"
#include "Engine/Assets/AssetManagerModule.h"
#include "Engine/Assets/PrefabAsset.h"
#include "Engine/Framework/BaseEngine.h"
#include "Engine/Rendering/RenderModule.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/ActorTemplateSerializer.h"

#include "EditorEngine.h"
#include "Editor/Core/EditorCommandDispatcher.h"
#include "Editor/Core/EditorCommands.h"
#include "Editor/Core/WindowsFileDialogs.h"
#include "imgui.h"
#include "ThirdParty/IconsFontAwesome6.h"

#include <cctype>

DEFINE_LOG_CATEGORY(EditorGUI)

namespace
{
bool DrawToolbarButton(const char* id, const char* icon, const char* label, bool bAccent = false)
{
    const ImVec4 accent = ImVec4(178.0f / 255.0f, 128.0f / 255.0f, 51.0f / 255.0f, 1.0f);
    const ImVec4 accentHover = ImVec4(200.0f / 255.0f, 143.0f / 255.0f, 61.0f / 255.0f, 1.0f);
    const ImVec4 accentActive = ImVec4(153.0f / 255.0f, 112.0f / 255.0f, 46.0f / 255.0f, 1.0f);

    ImGui::PushID(id);
    if (bAccent)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, accent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, accentHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, accentActive);
    }

    const bool pressed = ImGui::Button((String(icon) + "  " + label).c_str(), ImVec2(0.0f, 26.0f));

    if (bAccent)
        ImGui::PopStyleColor(3);
    ImGui::PopID();
    return pressed;
}

String MakePrefabAssetPath(const Actor& actor)
{
    String name = actor.GetName();
    if (name.length() == 0)
        name = actor.GetType() ? actor.GetType()->Name : "PrefabActor";

    for (size_t i = 0; i < name.length(); ++i)
    {
        const unsigned char ch = static_cast<unsigned char>(name[i]);
        if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-'))
            name[i] = '_';
    }

    return "assets/Prefabs/" + name + ".rasset";
}

bool SaveActorAsPrefab(Actor& actor)
{
    auto* assetModule = GEngine->GetModuleManager().GetModule<AssetManagerModule>();
    if (!assetModule)
        return false;

    PrefabAsset prefab;
    prefab.m_ActorTypeName = actor.GetType() ? actor.GetType()->Name : "Actor";
    prefab.SerializedVersion = 1;

    if (!ActorTemplateSerializer::SerializeActorTemplateToString(
            actor,
            prefab.m_TemplateYaml,
            { false }))
    {
        return false;
    }

    return assetModule->SaveAssetToFile(MakePrefabAssetPath(actor), prefab);
}

void DrawPrefabPlacementMenuItems(EditorSelection& selection)
{
    AssetManagerModule* assetModule = GEngine->GetModuleManager().GetModule<AssetManagerModule>();
    if (!assetModule)
    {
        ImGui::TextDisabled("Asset manager unavailable.");
        return;
    }

    const auto& assets = assetModule->GetRegistry().GetAll();
    bool bFoundPrefab = false;
    for (const auto& pair : assets)
    {
        const AssetMeta& meta = pair.Value;
        if (!meta.Type || meta.Type != PrefabAsset::StaticType())
            continue;

        bFoundPrefab = true;
        const String assetName = std::filesystem::path(meta.Path.c_str()).filename().string().c_str();
        if (!ImGui::MenuItem(assetName.c_str()))
            continue;

        PrefabAsset* prefab = dynamic_cast<PrefabAsset*>(assetModule->GetManager().Load(meta.ID));
        if (!prefab)
            continue;

        if (Actor* actor = GEngine->GetActiveScene()->SpawnActorFromPrefab(*prefab))
            selection.SetSingleActor(actor);
    }

    if (!bFoundPrefab)
        ImGui::TextDisabled("No prefab assets found.");
}
}

LevelToolbarPanel::LevelToolbarPanel(EditorSelection& selection)
    : m_Selection(selection)
{
}

void LevelToolbarPanel::Draw()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 3.0f));

    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ImVec4(0.48f, 0.56f, 0.64f, 1.0f), "%s", ICON_FA_WAND_MAGIC_SPARKLES);
    ImGui::SameLine();
    ImGui::TextDisabled("Level Tools");
    ImGui::SameLine(0.0f, 12.0f);

    if (DrawToolbarButton("PlaceActors", ICON_FA_CUBE, "Place Actors"))
        ImGui::OpenPopup("PlaceActorsPopup");

    ImGui::SameLine();
    if (DrawToolbarButton("PlacePrefab", ICON_FA_BOX_OPEN, "Place Prefab"))
        ImGui::OpenPopup("PlacePrefabPopup");

    ImGui::SameLine();
    const bool bCanSavePrefab = m_Selection.SelectedActor != nullptr && m_Selection.SelectedActor->IsValid();
    if (!bCanSavePrefab)
        ImGui::BeginDisabled();
    if (DrawToolbarButton("SavePrefab", ICON_FA_FLOPPY_DISK, "Save Prefab"))
    {
        if (m_Selection.SelectedActor && SaveActorAsPrefab(*m_Selection.SelectedActor))
        {
            RB_LOG(EditorGUI, info, "Saved prefab for actor '{}'", m_Selection.SelectedActor->GetName().c_str());
        }
        else
            RB_LOG(EditorGUI, warn, "Failed to save selected actor as prefab.");
    }
    if (!bCanSavePrefab)
        ImGui::EndDisabled();

    ImGui::SameLine();
    if (DrawToolbarButton("PlaceController", ICON_FA_GAMEPAD, "Place Controller"))
    {
        EditorCommandDispatcher::Execute(std::make_unique<SpawnPawnControllerCommand>(GEngine->GetActiveScene()));
    }

    if (ImGui::BeginPopup("PlaceActorsPopup"))
    {
        for (auto types = TypeRegistry::Get().GetTypes(); const auto& type : types)
        {
            if (!type.Value->IsA(Actor::StaticType()))
                continue;
            if (!ImGui::MenuItem(type.Key.c_str()))
                continue;

            EditorCommandDispatcher::Execute(std::make_unique<SpawnActorCommand>(
                GEngine->GetActiveScene(),
                type.Value));
        }

        ImGui::SeparatorText("Prefabs");
        DrawPrefabPlacementMenuItems(m_Selection);
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("PlacePrefabPopup"))
    {
        DrawPrefabPlacementMenuItems(m_Selection);
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (DrawToolbarButton("SaveScene", ICON_FA_FLOPPY_DISK, "Save"))
    {
        if (EditorEngine* editor = static_cast<EditorEngine*>(GEngine))
            editor->SaveEditorScene();
    }

    ImGui::SameLine();
    if (DrawToolbarButton("LoadScene", ICON_FA_FOLDER_OPEN, "Load"))
    {
        m_Selection.Clear();
        GetEditorTransactionManager().Clear();

        const String path = Editor::WindowsFileDialogs::OpenFile(
            L"Open Scene",
            L"Rebel Scene Files (*.Ryml)\0*.Ryml\0All Files (*.*)\0*.*\0");
        if (path.length() > 0)
        {
            if (EditorEngine* editor = static_cast<EditorEngine*>(GEngine))
            {
                if (!editor->LoadEditorScene(path))
                    RB_LOG(EditorGUI, warn, "Failed to load scene '{}'", path.c_str());
            }
        }
    }

    ImGui::SameLine();
    const bool bIsPlaying = static_cast<EditorEngine*>(GEngine)->IsPlaying();
    if (DrawToolbarButton("PlayToggle", bIsPlaying ? ICON_FA_STOP : ICON_FA_PLAY, bIsPlaying ? "Stop PIE" : "Play PIE", true))
    {
        m_Selection.Clear();
        GetEditorTransactionManager().Clear();

        if (EditorEngine* editor = static_cast<EditorEngine*>(GEngine))
        {
            RB_LOG(EditorGUI, info, "ActiveScene = {}", (void*)GEngine->GetActiveScene());
            if (editor->IsPlaying())
                editor->StopPlayInEditor();
            else
                editor->StartPlayInEditor();
        }
    }

    if (RenderModule* renderer = GEngine->GetModuleManager().GetModule<RenderModule>())
    {
        ImGui::SameLine();
        bool bShowGrid = renderer->IsEditorGridEnabled();
        if (ImGui::Checkbox((String(ICON_FA_TABLE_CELLS) + " Grid").c_str(), &bShowGrid))
            renderer->SetEditorGridEnabled(bShowGrid);

        ImGui::SameLine();
        if (DrawToolbarButton("GridSettings", ICON_FA_SLIDERS, "Grid"))
            ImGui::OpenPopup("EditorGridSettingsPopup");

        if (ImGui::BeginPopup("EditorGridSettingsPopup"))
        {
            RenderModule::EditorGridSettings settings = renderer->GetEditorGridSettings();
            bool bChanged = false;

            bChanged |= ImGui::DragFloat("Grid Extent (0 = Infinite)", &settings.GridExtent, 1.0f, 0.0f, 10000.0f, "%.1f");
            bChanged |= ImGui::DragFloat("Cell Spacing", &settings.CellSpacing, 0.05f, 0.05f, 100.0f, "%.2f");
            bChanged |= ImGui::DragFloat("Major Spacing", &settings.MajorLineSpacing, 0.25f, 0.1f, 500.0f, "%.2f");
            bChanged |= ImGui::DragFloat("Fade Distance", &settings.FadeDistance, 1.0f, 8.0f, 10000.0f, "%.1f");
            bChanged |= ImGui::DragFloat("Minor Line Width", &settings.MinorLineWidth, 0.05f, 0.5f, 4.0f, "%.2f");
            bChanged |= ImGui::DragFloat("Major Line Width", &settings.MajorLineWidth, 0.05f, 0.5f, 6.0f, "%.2f");

            bChanged |= ImGui::ColorEdit4("Minor Color", &settings.MinorColor[0], ImGuiColorEditFlags_AlphaBar);
            bChanged |= ImGui::ColorEdit4("Major Color", &settings.MajorColor[0], ImGuiColorEditFlags_AlphaBar);
            bChanged |= ImGui::ColorEdit4("X Axis Color", &settings.XAxisColor[0], ImGuiColorEditFlags_AlphaBar);
            bChanged |= ImGui::ColorEdit4("Y Axis Color", &settings.YAxisColor[0], ImGuiColorEditFlags_AlphaBar);

            if (bChanged)
            {
                settings.CellSpacing = FMath::max(settings.CellSpacing, 0.01f);
                settings.MajorLineSpacing = FMath::max(settings.MajorLineSpacing, settings.CellSpacing);
                settings.FadeDistance = FMath::max(settings.FadeDistance, 1.0f);
                settings.GridExtent = FMath::max(settings.GridExtent, 0.0f);
                settings.MinorLineWidth = FMath::max(settings.MinorLineWidth, 0.5f);
                settings.MajorLineWidth = FMath::max(settings.MajorLineWidth, settings.MinorLineWidth);
                renderer->SetEditorGridSettings(settings);
            }

            ImGui::EndPopup();
        }

        ImGui::SameLine();
        if (DrawToolbarButton("LightingSettings", ICON_FA_LIGHTBULB, "Lighting"))
            ImGui::OpenPopup("LightingSettingsPopup");

        if (ImGui::BeginPopup("LightingSettingsPopup"))
        {
            RenderModule::DirectionalLightSettings dirLight = renderer->GetDirectionalLight();
            RenderModule::SkyAmbientSettings skyAmbient = renderer->GetSkyAmbient();
            RenderModule::EnvironmentLightingSettings envLighting = renderer->GetEnvironmentLighting();

            bool dirChanged = false;
            bool skyChanged = false;
            bool envChanged = false;

            ImGui::TextUnformatted("Directional Light");
            dirChanged |= ImGui::DragFloat3("Direction", &dirLight.Direction[0], 0.01f, -1.0f, 1.0f, "%.2f");
            dirChanged |= ImGui::ColorEdit3("Color", &dirLight.Color[0]);
            dirChanged |= ImGui::DragFloat("Intensity", &dirLight.Intensity, 0.01f, 0.0f, 20.0f, "%.2f");
            dirChanged |= ImGui::DragFloat("Specular Intensity", &dirLight.SpecularIntensity, 0.01f, 0.0f, 8.0f, "%.2f");
            dirChanged |= ImGui::DragFloat("Specular Power", &dirLight.SpecularPower, 0.25f, 1.0f, 256.0f, "%.1f");

            ImGui::Separator();
            ImGui::TextUnformatted("Sky Ambient");
            skyChanged |= ImGui::ColorEdit3("Sky Color", &skyAmbient.SkyColor[0]);
            skyChanged |= ImGui::ColorEdit3("Ground Color", &skyAmbient.GroundColor[0]);
            skyChanged |= ImGui::DragFloat("Ambient Intensity", &skyAmbient.Intensity, 0.01f, 0.0f, 5.0f, "%.2f");

            ImGui::Separator();
            ImGui::TextUnformatted("Environment (Future)");
            envChanged |= ImGui::Checkbox("Use Environment Map", &envLighting.UseEnvironmentMap);
            envChanged |= ImGui::DragFloat("Diffuse IBL Intensity", &envLighting.DiffuseIBLIntensity, 0.01f, 0.0f, 8.0f, "%.2f");
            envChanged |= ImGui::DragFloat("Specular IBL Intensity", &envLighting.SpecularIBLIntensity, 0.01f, 0.0f, 8.0f, "%.2f");

            if (dirChanged)
            {
                if (FMath::length(dirLight.Direction) < 1e-4f)
                    dirLight.Direction = Vector3(0.0f, -1.0f, 0.0f);

                dirLight.Intensity = FMath::max(dirLight.Intensity, 0.0f);
                dirLight.SpecularIntensity = FMath::max(dirLight.SpecularIntensity, 0.0f);
                dirLight.SpecularPower = FMath::max(dirLight.SpecularPower, 1.0f);
                renderer->SetDirectionalLight(dirLight);
            }

            if (skyChanged)
            {
                skyAmbient.Intensity = FMath::max(skyAmbient.Intensity, 0.0f);
                renderer->SetSkyAmbient(skyAmbient);
            }

            if (envChanged)
            {
                envLighting.DiffuseIBLIntensity = FMath::max(envLighting.DiffuseIBLIntensity, 0.0f);
                envLighting.SpecularIBLIntensity = FMath::max(envLighting.SpecularIBLIntensity, 0.0f);
                renderer->SetEnvironmentLighting(envLighting);
            }

            ImGui::EndPopup();
        }
    }

    ImGui::PopStyleVar(2);
}
