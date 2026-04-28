#include "Editor/Panels/SkeletalMeshImporterPanel.h"

#include "Editor/Core/WindowsFileDialogs.h"

#include "Engine/Framework/EnginePch.h"
#include "Engine/Framework/BaseEngine.h"
#include "Engine/Assets/AssetManagerModule.h"
#include "Engine/Animation/SkeletalMeshAsset.h"
#include "Engine/Animation/SkeletonAsset.h"
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
        result = "SkeletalMesh";

    return result;
}
}

void SkeletalMeshImporterPanel::CopyStringToBuffer(const String& value, char* buffer, size_t bufferSize)
{
    if (!buffer || bufferSize == 0)
        return;

    strncpy_s(buffer, bufferSize, value.c_str(), _TRUNCATE);
    buffer[bufferSize - 1] = '\0';
}

void SkeletalMeshImporterPanel::OpenWithFile(const String& sourcePath, const String& outputDirectory)
{
    m_ShowWindow = true;
    CopyStringToBuffer(sourcePath, m_SourcePath, IM_ARRAYSIZE(m_SourcePath));
    CopyStringToBuffer(outputDirectory, m_OutputDirectory, IM_ARRAYSIZE(m_OutputDirectory));

    const std::filesystem::path path(sourcePath.c_str());
    CopyStringToBuffer(path.stem().string().c_str(), m_AssetName, IM_ARRAYSIZE(m_AssetName));
}

bool SkeletalMeshImporterPanel::ImportFile(
    const String& sourcePath,
    const String& outputDirectory,
    const String& requestedAssetName,
    String& outStatus)
{
    outStatus = "";

    if (!std::filesystem::exists(Utf8Path(sourcePath)))
    {
        outStatus = "Source skeletal mesh file was not found.";
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
    SkeletonAsset skeleton;

    if (!MeshLoader::LoadSkeletalMeshFromFile(sourcePath, vertices, indices, skeleton))
    {
        outStatus = "Failed to import skeletal mesh (see log).";
        return false;
    }

    const std::filesystem::path sourceFsPath = Utf8Path(sourcePath);
    const String stem = sourceFsPath.stem().string().c_str();
    const String assetName = SanitizeAssetFileName(requestedAssetName.length() > 0 ? requestedAssetName : stem);

    skeleton.SerializedVersion = SkeletonAsset::kCurrentVersion;

    String normalizedOutputDir = outputDirectory;
    if (normalizedOutputDir.length() > 0 &&
        normalizedOutputDir[normalizedOutputDir.length() - 1] != '/' &&
        normalizedOutputDir[normalizedOutputDir.length() - 1] != '\\')
    {
        normalizedOutputDir += "/";
    }

    const bool savedSkeleton =
        assetModule->SaveAssetToFile(normalizedOutputDir + assetName + "_skeleton.rasset", skeleton);

    SkeletalMeshAsset skelMesh;
    skelMesh.Vertices = vertices;
    skelMesh.Indices = indices;
    skelMesh.m_Skeleton.SetHandle(skeleton.ID);

    const bool savedMesh =
        assetModule->SaveAssetToFile(normalizedOutputDir + assetName + "_skelmesh.rasset", skelMesh);

    if (!(savedSkeleton && savedMesh))
    {
        outStatus = "Failed to save skeleton or skeletal mesh asset.";
        return false;
    }

    outStatus = "Imported skeleton and skeletal mesh assets successfully.";
    return true;
}

void SkeletalMeshImporterPanel::Draw()
{
    ImGui::InputText("Source Skeletal File", m_SourcePath, IM_ARRAYSIZE(m_SourcePath), ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine();
    if (ImGui::Button("Browse File..."))
    {
        const String path = Editor::WindowsFileDialogs::OpenFile(
            L"Select Skeletal Mesh Source",
            L"3D Mesh Files (*.fbx;*.gltf;*.glb)\0*.fbx;*.gltf;*.glb\0All Files (*.*)\0*.*\0");
        if (path.length() > 0)
            CopyStringToBuffer(path, m_SourcePath, IM_ARRAYSIZE(m_SourcePath));
    }

    ImGui::InputText("Output Directory", m_OutputDirectory, IM_ARRAYSIZE(m_OutputDirectory), ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine();
    if (ImGui::Button("Browse Folder..."))
    {
        const String path = Editor::WindowsFileDialogs::OpenFolder(L"Select Skeletal Mesh Output Folder");
        if (path.length() > 0)
            CopyStringToBuffer(path, m_OutputDirectory, IM_ARRAYSIZE(m_OutputDirectory));
    }

    ImGui::InputText("Asset Name", m_AssetName, IM_ARRAYSIZE(m_AssetName));

    if (ImGui::Button("Import Skeletal Mesh"))
    {
        ImportFile(m_SourcePath, m_OutputDirectory, m_AssetName, m_Status);
    }

    if (m_Status.length() > 0)
        ImGui::TextWrapped("%s", m_Status.c_str());
}
