#include "Editor/Panels/StaticMeshImporterPanel.h"

#include "Editor/Core/WindowsFileDialogs.h"

#include "Engine/Framework/EnginePch.h"
#include "Engine/Framework/BaseEngine.h"
#include "Engine/Assets/AssetManagerModule.h"
#include "Engine/Assets/MeshAsset.h"
#include "Engine/Rendering/MeshLoader.h"

#include "imgui.h"

#include <filesystem>
#include <cctype>
#include <cstring>

namespace
{
std::filesystem::path Utf8Path(const String& path)
{
    return std::filesystem::u8path(path.c_str());
}

String SanitizeAssetFileName(const String& input)
{
    String result;
    for (size_t i = 0; i < input.length(); ++i)
    {
        const unsigned char c = static_cast<unsigned char>(input[i]);
        if (std::isalnum(c) || c == '_')
        {
            const char ch[2] = { static_cast<char>(c), '\0' };
            result.append(ch);
        }
        else
        {
            result.append("_");
        }
    }

    if (result.length() == 0)
        result = "StaticMesh";

    return result;
}
}

void StaticMeshImporterPanel::CopyStringToBuffer(const String& value, char* buffer, size_t bufferSize)
{
    if (!buffer || bufferSize == 0)
        return;

    strncpy_s(buffer, bufferSize, value.c_str(), _TRUNCATE);
    buffer[bufferSize - 1] = '\0';
}

void StaticMeshImporterPanel::OpenWithFile(const String& sourcePath, const String& outputDirectory)
{
    m_ShowWindow = true;
    CopyStringToBuffer(sourcePath, m_SourcePath, IM_ARRAYSIZE(m_SourcePath));
    CopyStringToBuffer(outputDirectory, m_OutputDirectory, IM_ARRAYSIZE(m_OutputDirectory));

    const std::filesystem::path path(sourcePath.c_str());
    CopyStringToBuffer(path.stem().string().c_str(), m_AssetName, IM_ARRAYSIZE(m_AssetName));
}

bool StaticMeshImporterPanel::ImportFile(
    const String& sourcePath,
    const String& outputDirectory,
    const String& requestedAssetName,
    String& outStatus)
{
    outStatus = "";

    if (!std::filesystem::exists(Utf8Path(sourcePath)))
    {
        outStatus = "Source mesh file was not found.";
        return false;
    }

    auto* assetModule = GEngine->GetModuleManager().GetModule<AssetManagerModule>();
    if (!assetModule)
    {
        outStatus = "Asset manager is unavailable.";
        return false;
    }

    TArray<Vertex> vertices;
    TArray<uint32> indices;
    if (!MeshLoader::LoadMeshFromFile(sourcePath, vertices, indices))
    {
        outStatus = "Failed to import static mesh (see log).";
        return false;
    }

    const std::filesystem::path sourceFsPath = Utf8Path(sourcePath);
    const String stem = sourceFsPath.stem().string().c_str();
    const String assetName = SanitizeAssetFileName(requestedAssetName.length() > 0 ? requestedAssetName : stem);

    MeshAsset mesh;
    mesh.Vertices = vertices;
    mesh.Indices = indices;

    String normalizedOutputDir = outputDirectory;
    if (normalizedOutputDir.length() > 0 &&
        normalizedOutputDir[normalizedOutputDir.length() - 1] != '/' &&
        normalizedOutputDir[normalizedOutputDir.length() - 1] != '\\')
    {
        normalizedOutputDir += "/";
    }

    if (!assetModule->SaveAssetToFile(normalizedOutputDir + assetName + ".rasset", mesh))
    {
        outStatus = "Failed to save static mesh asset.";
        return false;
    }

    outStatus = "Imported static mesh asset successfully.";
    return true;
}

void StaticMeshImporterPanel::Draw()
{
    ImGui::InputText("Source Mesh File", m_SourcePath, IM_ARRAYSIZE(m_SourcePath), ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine();
    if (ImGui::Button("Browse File..."))
    {
        const String path = Editor::WindowsFileDialogs::OpenFile(
            L"Select Static Mesh Source",
            L"3D Mesh Files (*.fbx;*.obj;*.gltf;*.glb)\0*.fbx;*.obj;*.gltf;*.glb\0All Files (*.*)\0*.*\0");
        if (path.length() > 0)
            CopyStringToBuffer(path, m_SourcePath, IM_ARRAYSIZE(m_SourcePath));
    }

    ImGui::InputText("Output Directory", m_OutputDirectory, IM_ARRAYSIZE(m_OutputDirectory), ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine();
    if (ImGui::Button("Browse Folder..."))
    {
        const String path = Editor::WindowsFileDialogs::OpenFolder(L"Select Static Mesh Output Folder");
        if (path.length() > 0)
            CopyStringToBuffer(path, m_OutputDirectory, IM_ARRAYSIZE(m_OutputDirectory));
    }

    ImGui::InputText("Asset Name", m_AssetName, IM_ARRAYSIZE(m_AssetName));

    if (ImGui::Button("Import Static Mesh"))
    {
        ImportFile(m_SourcePath, m_OutputDirectory, m_AssetName, m_Status);
    }

    if (m_Status.length() > 0)
        ImGui::TextWrapped("%s", m_Status.c_str());
}
