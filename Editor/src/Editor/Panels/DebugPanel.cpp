#include "Editor/Panels/DebugPanel.h"

#include "Engine/Framework/EnginePch.h"
#include "imgui.h"

void DebugPanel::Draw()
{
    if (ImGui::Button("Clear"))
        GEngineLogBuffer.Clear();

    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &GConsoleState.AutoScroll);

    ImGui::SameLine();
    ImGui::Checkbox("Show Time", &GConsoleState.ShowTime);

    ImGui::SameLine();

    const char* levels[] = { "Trace", "Debug", "Info", "Warn", "Error", "Critical" };
    ImGui::SetNextItemWidth(120);
    ImGui::Combo("Level", &GConsoleState.MinLogLevel, levels, IM_ARRAYSIZE(levels));

    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    ImGui::InputText("Search", GConsoleState.SearchBuffer, IM_ARRAYSIZE(GConsoleState.SearchBuffer));

    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    if (ImGui::BeginCombo("Categories", "Filter Categories"))
    {
        if (ImGui::Selectable("Enable All"))
        {
            for (auto& [cat, enabled] : GConsoleState.CategoryFilter)
                enabled = true;
        }

        if (ImGui::Selectable("Disable All"))
        {
            for (auto& [cat, enabled] : GConsoleState.CategoryFilter)
                enabled = false;
        }

        ImGui::Separator();

        for (auto& [cat, enabled] : GConsoleState.CategoryFilter)
            ImGui::Checkbox(cat.c_str(), &enabled);

        ImGui::EndCombo();
    }

    ImGui::Separator();

    ImGui::BeginChild("ConsoleScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    const auto& entries = GEngineLogBuffer.Get();

    for (const auto& e : entries)
    {
        if ((int)e.Level < GConsoleState.MinLogLevel)
            continue;

        if (!GConsoleState.CategoryFilter[e.Category])
            continue;

        if (strlen(GConsoleState.SearchBuffer) > 0)
        {
            if (e.Message.find(GConsoleState.SearchBuffer) == std::string::npos)
                continue;
        }

        ImVec4 color = ImVec4(1, 1, 1, 1);

        switch (e.Level)
        {
        case spdlog::level::trace: color = ImVec4(0.6f, 0.6f, 0.6f, 1); break;
        case spdlog::level::debug: color = ImVec4(0.4f, 0.8f, 1.0f, 1); break;
        case spdlog::level::info: color = ImVec4(1, 1, 1, 1); break;
        case spdlog::level::warn: color = ImVec4(1.0f, 0.8f, 0.2f, 1); break;
        case spdlog::level::err: color = ImVec4(1.0f, 0.3f, 0.3f, 1); break;
        case spdlog::level::critical: color = ImVec4(1.0f, 0.0f, 0.0f, 1); break;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, color);

        if (GConsoleState.ShowTime)
        {
            ImGui::TextUnformatted(e.Message.c_str());
        }
        else
        {
            const char* msg = e.Message.c_str();
            if (strlen(msg) > 10 && msg[0] == '[')
                msg += 10;

            ImGui::TextUnformatted(msg);
        }

        ImGui::PopStyleColor();
    }

    if (GConsoleState.AutoScroll &&
        ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
}
