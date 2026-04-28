#pragma once

#include "Engine/Framework/EnginePch.h"
#include "Core/AssetPtrBase.h"

class AnimationImporterPanel
{
public:
    bool* GetVisibilityPtr() { return &m_ShowWindow; }
    void OpenWithFile(
        const String& sourcePath,
        const String& outputDirectory = "assets",
        AssetHandle skeletonHandle = 0);

    void Draw();

    static bool ImportFile(
        const String& sourcePath,
        const String& outputDirectory,
        AssetHandle skeletonHandle,
        int32& outSavedClipCount,
        String& outStatus);

private:
    static void CopyStringToBuffer(const String& value, char* buffer, size_t bufferSize);

    bool m_ShowWindow = false;
    char m_SourcePath[512] = "assets/models/Idle.fbx";
    char m_OutputDirectory[512] = "assets";
    AssetHandle m_SkeletonHandle = 0;
    String m_Status;
};
