#include "Editor/Panels/AnimationImporterPanel.h"

#include "Engine/Framework/EnginePch.h"
#include "Engine/Framework/BaseEngine.h"
#include "Engine/Assets/AssetFileHeader.h"
#include "Engine/Assets/AssetManagerModule.h"
#include "Engine/Animation/AnimationAsset.h"
#include "Engine/Animation/SkeletonAsset.h"
#include "Engine/Animation/SkeletalMeshAsset.h"
#include "Engine/Rendering/MeshLoader.h"
#include "Editor/Core/WindowsFileDialogs.h"

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

template<typename T>
bool SaveAssetToFile(const String& filePath, T& asset)
{
    FileStream fs(filePath.c_str(), "wb");
    BinaryWriter ar(fs);

    AssetFileHeader header{};
    header.AssetID = (uint64)asset.ID;
    header.TypeHash = TypeHash(T::StaticType()->Name.c_str());
    header.Version = 3;

    ar.Write(header);

    header.PayloadOffset = ar.Tell();

    asset.SerializedVersion = header.Version;
    asset.Serialize(ar);

    ar.Seek(0);
    ar.Write(header);

    return true;
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
            continue;
        }

        result.append("_");
    }

    if (result.length() == 0)
        result = "Clip";

    return result;
}

String BuildAnimationAssetPath(
    const String& outputDirectory,
    const String& sourceStem,
    const String& clipName,
    int32 clipIndex)
{
    const String safeSource = SanitizeAssetFileName(sourceStem);
    const String safeClip = SanitizeAssetFileName(clipName);
    const String indexStr = std::to_string(clipIndex).c_str();

    String path = outputDirectory;
    if (path.length() > 0 && path[path.length() - 1] != '/' && path[path.length() - 1] != '\\')
        path += "/";

    path += safeSource;
    path += "_";
    path += safeClip;
    path += "_";
    path += indexStr;
    return path;
}
}

void AnimationImporterPanel::CopyStringToBuffer(const String& value, char* buffer, size_t bufferSize)
{
    if (!buffer || bufferSize == 0)
        return;

    strncpy_s(buffer, bufferSize, value.c_str(), _TRUNCATE);
    buffer[bufferSize - 1] = '\0';
}

void AnimationImporterPanel::OpenWithFile(
    const String& sourcePath,
    const String& outputDirectory,
    AssetHandle skeletonHandle)
{
    m_ShowWindow = true;
    CopyStringToBuffer(sourcePath, m_SourcePath, IM_ARRAYSIZE(m_SourcePath));
    CopyStringToBuffer(outputDirectory, m_OutputDirectory, IM_ARRAYSIZE(m_OutputDirectory));
    m_SkeletonHandle = skeletonHandle;
}

bool AnimationImporterPanel::ImportFile(
    const String& sourcePath,
    const String& outputDirectory,
    AssetHandle skeletonHandle,
    int32& outSavedClipCount,
    String& outStatus)
{
    outSavedClipCount = 0;
    outStatus = "";

    if ((uint64)skeletonHandle == 0)
    {
        outStatus = "Select a skeleton asset before importing.";
        return false;
    }

    if (!std::filesystem::exists(Utf8Path(sourcePath)))
    {
        outStatus = "Source animation file was not found.";
        return false;
    }

    AssetManagerModule* assetModule = GEngine->GetModuleManager().GetModule<AssetManagerModule>();
    if (!assetModule)
    {
        outStatus = "Asset manager is unavailable.";
        return false;
    }

    SkeletonAsset* skeleton =
        dynamic_cast<SkeletonAsset*>(assetModule->GetManager().Load(skeletonHandle));
    if (!skeleton)
    {
        outStatus = "Failed to load selected skeleton asset.";
        return false;
    }

    TArray<AnimationAsset> clips;
    if (!MeshLoader::LoadAnimationClipsFromFile(sourcePath, *skeleton, clips))
    {
        outStatus = "No animation clips imported (see log).";
        return false;
    }

    const std::filesystem::path sourceFsPath = Utf8Path(sourcePath);
    const String sourceStem = sourceFsPath.stem().string().c_str();

    std::filesystem::create_directories(Utf8Path(outputDirectory));

    for (int32 i = 0; i < clips.Num(); ++i)
    {
        AnimationAsset& clip = clips[i];
        clip.Path = BuildAnimationAssetPath(outputDirectory, sourceStem, clip.m_ClipName, i);

        const String filePath = clip.Path + ".rasset";
        if (SaveAssetToFile(filePath, clip))
            ++outSavedClipCount;
    }

    assetModule->Init();
    outStatus =
        String("Imported ") +
        String(std::to_string(outSavedClipCount).c_str()) +
        String(" animation clip(s).");
    return outSavedClipCount > 0;
}

void AnimationImporterPanel::Draw()
{
    ImGui::BeginGroup();
    ImGui::InputText("Source Animation File", m_SourcePath, IM_ARRAYSIZE(m_SourcePath), ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine();
    if (ImGui::Button("Browse File..."))
    {
        const String path = Editor::WindowsFileDialogs::OpenFile(
            L"Select Animation Source",
            L"3D Animation Files (*.fbx;*.gltf;*.glb)\0*.fbx;*.gltf;*.glb\0All Files (*.*)\0*.*\0");
        if (path.length() > 0)
            CopyStringToBuffer(path, m_SourcePath, IM_ARRAYSIZE(m_SourcePath));
    }
    ImGui::EndGroup();

    ImGui::BeginGroup();
    ImGui::InputText("Output Directory", m_OutputDirectory, IM_ARRAYSIZE(m_OutputDirectory), ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine();
    if (ImGui::Button("Browse Folder..."))
    {
        const String path = Editor::WindowsFileDialogs::OpenFolder(L"Select Animation Output Folder");
        if (path.length() > 0)
            CopyStringToBuffer(path, m_OutputDirectory, IM_ARRAYSIZE(m_OutputDirectory));
    }
    ImGui::EndGroup();

    AssetManagerModule* assetModule = GEngine->GetModuleManager().GetModule<AssetManagerModule>();
    if (!assetModule)
    {
        ImGui::TextDisabled("Asset manager is unavailable.");
        ImGui::End();
        return;
    }

    const auto& registry = assetModule->GetRegistry().GetAll();

    const char* skeletonLabel = "None";
    if ((uint64)m_SkeletonHandle != 0)
    {
        if (const AssetMeta* meta = registry.Find(m_SkeletonHandle))
            skeletonLabel = meta->Path.c_str();
    }

    if (ImGui::BeginCombo("Skeleton", skeletonLabel))
    {
        if (ImGui::Selectable("None", (uint64)m_SkeletonHandle == 0))
            m_SkeletonHandle = 0;

        for (const auto& pair : registry)
        {
            const AssetMeta& meta = pair.Value;
            if (meta.Type != SkeletonAsset::StaticType())
                continue;

            const bool bSelected = (meta.ID == m_SkeletonHandle);
            if (ImGui::Selectable(meta.Path.c_str(), bSelected))
                m_SkeletonHandle = meta.ID;

            if (bSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    if (ImGui::Button("Import Clips"))
    {
        int32 savedCount = 0;
        ImportFile(m_SourcePath, m_OutputDirectory, m_SkeletonHandle, savedCount, m_Status);
    }

    if (m_Status.length() > 0)
        ImGui::TextWrapped("%s", m_Status.c_str());
}
