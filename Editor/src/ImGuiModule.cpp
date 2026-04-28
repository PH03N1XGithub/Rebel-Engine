#include "EditorEngine.h"
#include "ImGUIModule.h"

#include "Editor/EditorLayer.h"
#include "Editor/Core/EditorContext.h"
#include "Editor/Core/EditorSelection.h"
#include "Editor/Core/EditorCommandSystem.h"
#include "Editor/UI/EditorImGui.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "ThirdParty/imgui_impl_glfw.h"
#include "ThirdParty/imgui_impl_opengl3.h"
#include "ImGuizmo.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

ImGuiModule::~ImGuiModule() {}

void ImGuiModule::SetColorTheme()
{
    Rebel::Editor::UI::ApplyDarkTheme();
}

void ImGuiModule::Init()
{
    RB_LOG(ImGuiLog, info, "Init ImGuiModule");

    GLFWwindow* glfwWindow = GEngine->GetWindow()->GetGLFWWindow();
    glfwMakeContextCurrent(glfwWindow);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;

    Rebel::Editor::UI::LoadEditorFonts();
    m_BaseFont = Rebel::Editor::UI::GetFont("Default");
    if (!m_BaseFont)
        m_BaseFont = io.Fonts->AddFontDefault();
    m_BoldFont = Rebel::Editor::UI::GetFont("Bold");
    io.FontDefault = m_BaseFont;

    SetColorTheme();

    CHECK_MSG(glfwWindow != nullptr, "GLFW window is null before ImGui init!");

    ImGui_ImplGlfw_InitForOpenGL(glfwWindow, true);
    ImGui_ImplOpenGL3_Init("#version 410");

    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    m_EditorLayer = std::make_unique<EditorLayer>();
}

void ImGuiModule::BeginFrame()
{
    ImGuiIO& io = ImGui::GetIO();
    const bool gameplayOwnsMouse = GEngine && GEngine->ShouldProcessGameplayInput();
    const bool cursorDisabled = GEngine && GEngine->GetWindow() && GEngine->GetWindow()->IsCursorDisabled();
    if (gameplayOwnsMouse || cursorDisabled)
        io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
    else
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;

    GLFWwindow* window = static_cast<GLFWwindow*>(GEngine->GetWindow()->GetNativeWindow());
    glfwMakeContextCurrent(window);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();

    io.DisplaySize = ImVec2(
        static_cast<float>(GEngine->GetWindow()->GetWidth()),
        static_cast<float>(GEngine->GetWindow()->GetHeight()));
}

void ImGuiModule::EndFrame()
{
    ImGuiIO& io = ImGui::GetIO();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        GLFWwindow* backup = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup);
    }
}

void ImGuiModule::Tick(float dt)
{
    BeginFrame();

    // Keep command execution bound to the live editor state.
    EditorContext& editorContext = GetEditorContext();
    editorContext.ActiveScene = GEngine->GetActiveScene();
    editorContext.Selection = &GetEditorSelection();
    editorContext.Transactions = &GetEditorTransactionManager();
    GetEditorTransactionManager().SetContext(&editorContext);
    

    if (m_EditorLayer)
        m_EditorLayer->TickShortcuts();

    if (m_EditorLayer)
        m_EditorLayer->Draw(dt);

    EndFrame();
}

void ImGuiModule::OnEvent(const Event& e)
{
    (void)e;
}

void ImGuiModule::Shutdown()
{
    GetEditorTransactionManager().SetContext(nullptr);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    m_EditorLayer.reset();

    RB_LOG(ImGuiLog, info, "Shutdown ImGuiModule");
}
