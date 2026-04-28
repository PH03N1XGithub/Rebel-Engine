#pragma once

#include "ThirdParty/IconsFontAwesome6.h"
#include "imgui.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Rebel::Editor::UI
{
struct FontConfiguration
{
    const char* Name = "";
    const char* FilePath = "";
    float Size = 15.0f;
    const ImWchar* GlyphRanges = nullptr;
    bool MergeWithLast = false;
    bool PixelSnapH = false;
};

class ScopedFont
{
public:
    explicit ScopedFont(ImFont* font)
        : m_Active(font != nullptr)
    {
        if (m_Active)
            ImGui::PushFont(font);
    }

    ~ScopedFont()
    {
        if (m_Active)
            ImGui::PopFont();
    }

private:
    bool m_Active = false;
};

inline std::unordered_map<std::string, ImFont*>& GetFontRegistry()
{
    static std::unordered_map<std::string, ImFont*> registry;
    return registry;
}

inline void ClearFonts()
{
    GetFontRegistry().clear();
}

inline ImFont* AddFont(const FontConfiguration& config, bool isDefault = false)
{
    ImGuiIO& io = ImGui::GetIO();

    ImFontConfig imguiConfig{};
    imguiConfig.MergeMode = config.MergeWithLast;
    imguiConfig.PixelSnapH = config.PixelSnapH;

    ImFont* font = io.Fonts->AddFontFromFileTTF(
        config.FilePath,
        config.Size,
        &imguiConfig,
        config.GlyphRanges ? config.GlyphRanges : io.Fonts->GetGlyphRangesDefault());

    if (!font)
        return nullptr;

    GetFontRegistry()[config.Name] = font;
    if (isDefault)
        io.FontDefault = font;

    return font;
}

inline ImFont* GetFont(std::string_view name)
{
    const auto it = GetFontRegistry().find(std::string(name));
    return it != GetFontRegistry().end() ? it->second : nullptr;
}

inline void LoadEditorFonts()
{
    ClearFonts();

    static const ImWchar kIconRanges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    constexpr const char* kRobotoPath = "vendor/fonts/Roboto-Medium.ttf";
    constexpr const char* kFontAwesomePath = "vendor/fonts/fa-solid-900.ttf";

    auto addMergedFont = [&](const char* name, float textSize, float iconSize, bool isDefault = false)
    {
        AddFont({ name, kRobotoPath, textSize }, isDefault);
        AddFont({ name, kFontAwesomePath, iconSize, kIconRanges, true, true });
    };

    addMergedFont("Bold", 18.0f, 16.0f);
    addMergedFont("Large", 24.0f, 22.0f);
    addMergedFont("Default", 15.0f, 13.0f, true);
    addMergedFont("Medium", 18.0f, 16.0f);
    addMergedFont("Small", 12.0f, 11.0f);
    addMergedFont("ExtraSmall", 10.0f, 10.0f);
    addMergedFont("BoldTitle", 16.0f, 14.0f);
}

inline bool DrawSearchField(const char* id, char* buffer, size_t bufferSize, const char* hint = "Search...")
{
    bool changed = false;

    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", ICON_FA_MAGNIFYING_GLASS);
    ImGui::SameLine(0.0f, 6.0f);
    ImGui::SetNextItemWidth(-1.0f);
    changed |= ImGui::InputTextWithHint(id, hint, buffer, bufferSize);

    if (buffer[0] != '\0')
    {
        ImGui::SameLine();
        if (ImGui::SmallButton(ICON_FA_XMARK))
        {
            buffer[0] = '\0';
            changed = true;
        }
    }

    return changed;
}

inline void DrawEmptyState(const char* icon, const char* title, const char* message)
{
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x <= 0.0f || avail.y <= 0.0f)
        return;

    const float wrapWidth = std::max(160.0f, avail.x * 0.62f);
    ImFont* iconFont = GetFont("Large");
    ImFont* titleFont = GetFont("Bold");
    ImFont* bodyFont = GetFont("Small");
    const ImVec2 iconSize = iconFont ? iconFont->CalcTextSizeA(iconFont->LegacySize, FLT_MAX, 0.0f, icon) : ImGui::CalcTextSize(icon);
    const ImVec2 titleSize = titleFont ? titleFont->CalcTextSizeA(titleFont->LegacySize, FLT_MAX, 0.0f, title) : ImGui::CalcTextSize(title);
    const ImVec2 bodySize = bodyFont ? bodyFont->CalcTextSizeA(bodyFont->LegacySize, FLT_MAX, wrapWidth, message) : ImGui::CalcTextSize(message, nullptr, false, wrapWidth);

    const float totalHeight = iconSize.y + 10.0f + titleSize.y + 8.0f + bodySize.y;
    const float startY = ImGui::GetCursorPosY() + std::max(18.0f, (avail.y - totalHeight) * 0.35f);

    ImGui::SetCursorPosY(startY);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (avail.x - iconSize.x) * 0.5f));
    {
        ScopedFont iconScope(iconFont);
        ImGui::TextDisabled("%s", icon);
    }

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (avail.x - titleSize.x) * 0.5f));
    {
        ScopedFont titleScope(titleFont);
        ImGui::TextUnformatted(title);
    }

    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.62f, 0.62f, 0.62f, 1.0f));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrapWidth);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (avail.x - wrapWidth) * 0.5f));
    {
        ScopedFont bodyScope(bodyFont);
        ImGui::TextUnformatted(message);
    }
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
}

inline void DrawSoftShadow(
    ImDrawList* drawList,
    const ImVec2& min,
    const ImVec2& max,
    float rounding = 6.0f,
    int layers = 5,
    float spread = 2.5f,
    float opacity = 1.0f)
{
    if (!drawList || layers <= 0 || spread <= 0.0f || opacity <= 0.0f)
        return;

    for (int layer = layers; layer >= 1; --layer)
    {
        const float grow = spread * static_cast<float>(layer);
        const float t = static_cast<float>(layer) / static_cast<float>(layers);
        const float alpha = (0.028f + 0.032f * t) * opacity;
        drawList->AddRectFilled(
            ImVec2(min.x - grow, min.y - grow),
            ImVec2(max.x + grow, max.y + grow),
            ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, alpha)),
            rounding + grow);
    }
}

inline void ApplyDarkTheme()
{
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.Alpha = 1.0f;
    style.DisabledAlpha = 0.6f;
    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.FramePadding = ImVec2(8.0f, 4.0f);
    style.CellPadding = ImVec2(7.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(5.0f, 3.0f);
    style.IndentSpacing = 12.0f;
    style.ScrollbarSize = 11.0f;
    style.GrabMinSize = 10.0f;

    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.TabBorderSize = 0.0f;
    style.WindowRounding = 4.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 2.5f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 2.5f;
    style.DockingSeparatorSize = 1.0f;
    style.WindowMenuButtonPosition = ImGuiDir_Left;

    const ImVec4 accent = ImVec4(178.0f / 255.0f, 128.0f / 255.0f, 51.0f / 255.0f, 1.0f);
    const ImVec4 accentHover = ImVec4(200.0f / 255.0f, 143.0f / 255.0f, 61.0f / 255.0f, 1.0f);
    const ImVec4 accentActive = ImVec4(153.0f / 255.0f, 112.0f / 255.0f, 46.0f / 255.0f, 1.0f);
    const ImVec4 accentMuted = ImVec4(178.0f / 255.0f, 128.0f / 255.0f, 51.0f / 255.0f, 0.28f);
    const ImVec4 titlebar = ImVec4(33.0f / 255.0f, 33.0f / 255.0f, 33.0f / 255.0f, 1.0f);
    const ImVec4 background = ImVec4(40.0f / 255.0f, 40.0f / 255.0f, 40.0f / 255.0f, 1.0f);
    const ImVec4 backgroundDark = ImVec4(28.0f / 255.0f, 28.0f / 255.0f, 28.0f / 255.0f, 1.0f);
    const ImVec4 propertyField = ImVec4(52.0f / 255.0f, 52.0f / 255.0f, 52.0f / 255.0f, 1.0f);
    const ImVec4 propertyFieldHover = ImVec4(62.0f / 255.0f, 62.0f / 255.0f, 62.0f / 255.0f, 1.0f);
    const ImVec4 border = ImVec4(74.0f / 255.0f, 74.0f / 255.0f, 74.0f / 255.0f, 1.0f);
    const ImVec4 text = ImVec4(0.90f, 0.91f, 0.94f, 1.0f);
    const ImVec4 textMuted = ImVec4(0.62f, 0.62f, 0.62f, 1.0f);
    
    

    colors[ImGuiCol_Text] = text;
    colors[ImGuiCol_TextDisabled] = textMuted;

    colors[ImGuiCol_WindowBg] = titlebar;
    colors[ImGuiCol_ChildBg] = background;
    colors[ImGuiCol_PopupBg] = ImVec4(36.0f / 255.0f, 36.0f / 255.0f, 36.0f / 255.0f, 0.98f);
    colors[ImGuiCol_Border] = border;
    colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    colors[ImGuiCol_FrameBg] = propertyField;
    colors[ImGuiCol_FrameBgHovered] = propertyFieldHover;
    colors[ImGuiCol_FrameBgActive] = propertyFieldHover;

    colors[ImGuiCol_TitleBg] = titlebar;
    colors[ImGuiCol_TitleBgActive] = titlebar;
    colors[ImGuiCol_TitleBgCollapsed] = titlebar;
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    colors[ImGuiCol_Header] = ImVec4(48.0f / 255.0f, 48.0f / 255.0f, 48.0f / 255.0f, 1.0f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(79.0f / 255.0f, 67.0f / 255.0f, 46.0f / 255.0f, 0.46f);
    colors[ImGuiCol_HeaderActive] = accentMuted;

    colors[ImGuiCol_Button] = ImVec4(48.0f / 255.0f, 48.0f / 255.0f, 48.0f / 255.0f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(89.0f / 255.0f, 75.0f / 255.0f, 51.0f / 255.0f, 0.62f);
    colors[ImGuiCol_ButtonActive] = ImVec4(153.0f / 255.0f, 112.0f / 255.0f, 46.0f / 255.0f, 0.88f);

    colors[ImGuiCol_CheckMark] = accent;
    colors[ImGuiCol_SliderGrab] = accent;
    colors[ImGuiCol_SliderGrabActive] = accentHover;

    colors[ImGuiCol_Separator] = backgroundDark;
    colors[ImGuiCol_SeparatorHovered] = accentMuted;
    colors[ImGuiCol_SeparatorActive] = accentActive;

    colors[ImGuiCol_ResizeGrip] = ImVec4(0.30f, 0.30f, 0.30f, 0.30f);
    colors[ImGuiCol_ResizeGripHovered] = accentMuted;
    colors[ImGuiCol_ResizeGripActive] = accentActive;

    colors[ImGuiCol_ScrollbarBg] = backgroundDark;
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.38f, 0.38f, 0.38f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabActive] = accentMuted;

    colors[ImGuiCol_Tab] = titlebar;
    colors[ImGuiCol_TabHovered] = ImVec4(79.0f / 255.0f, 67.0f / 255.0f, 46.0f / 255.0f, 0.36f);
    colors[ImGuiCol_TabActive] = ImVec4(54.0f / 255.0f, 49.0f / 255.0f, 42.0f / 255.0f, 1.0f);
    colors[ImGuiCol_TabUnfocused] = titlebar;
    colors[ImGuiCol_TabUnfocusedActive] = colors[ImGuiCol_TabHovered];
    colors[ImGuiCol_TabSelectedOverline] = accent;
    colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(178.0f / 255.0f, 128.0f / 255.0f, 51.0f / 255.0f, 0.0f);

    colors[ImGuiCol_DockingPreview] = accentMuted;
    colors[ImGuiCol_DockingEmptyBg] = backgroundDark;

    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);
    colors[ImGuiCol_TableBorderStrong] = border;
    colors[ImGuiCol_TableBorderLight] = backgroundDark;
    colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.025f);

    colors[ImGuiCol_TextSelectedBg] = accentMuted;
    colors[ImGuiCol_DragDropTarget] = accentActive;
    colors[ImGuiCol_NavHighlight] = accent;
    colors[ImGuiCol_NavWindowingHighlight] = accentHover;
    
    style.Colors[ImGuiCol_HeaderActive]   = ImVec4(0.70f, 0.50f, 0.20f, 1.0f);
    style.Colors[ImGuiCol_HeaderHovered]  = ImVec4(0.78f, 0.56f, 0.24f, 1.0f);
    style.Colors[ImGuiCol_Header]         = ImVec4(0.19f, 0.19f, 0.19f, 1.0f);
}
}
