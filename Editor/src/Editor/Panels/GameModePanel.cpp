#include "Editor/Panels/GameModePanel.h"

#include "Engine/Framework/BaseEngine.h"
#include "Engine/Gameplay/Framework/GameMode.h"
#include "Engine/Scene/World.h"

#include "imgui.h"
#include "ThirdParty/IconsFontAwesome6.h"

void GameModePanel::Draw()
{
    ImGui::SeparatorText(ICON_FA_FLAG_CHECKERED " Game Mode");
    ImGui::Dummy(ImVec2(0.0f, 2.0f));

    World* world = GEngine ? GEngine->GetWorld() : nullptr;
    if (!world)
    {
        ImGui::TextDisabled("World is unavailable.");
        return;
    }

    GameMode* gameMode = world->GetGameMode();
    if (!gameMode)
    {
        ImGui::TextDisabled("No GameMode assigned.");
        return;
    }

    const Rebel::Core::Reflection::TypeInfo* type = gameMode->GetType();
    if (!type)
    {
        ImGui::TextDisabled("GameMode reflection is unavailable.");
        return;
    }

    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_DefaultOpen |
        ImGuiTreeNodeFlags_Framed |
        ImGuiTreeNodeFlags_SpanAvailWidth |
        ImGuiTreeNodeFlags_FramePadding;

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(7.0f, 3.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 4.0f));

    const char* label = type->Name.c_str();
    if (ImGui::TreeNodeEx(label, flags, "%s %s", ICON_FA_GAMEPAD, label))
    {
        if (ImGui::BeginTable("GameModeProperties", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 138.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            PropertyEditor::DrawReflectedObjectUI(gameMode, *type);
            ImGui::EndTable();
        }
        ImGui::TreePop();
    }

    ImGui::PopStyleVar(2);
}
