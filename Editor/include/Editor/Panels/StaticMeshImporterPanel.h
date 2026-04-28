#pragma once

#include "Engine/Framework/EnginePch.h"

class StaticMeshImporterPanel
{
public:
    bool* GetVisibilityPtr() { return &m_ShowWindow; }
    void OpenWithFile(const String& sourcePath, const String& outputDirectory = "assets");
    void Draw();

    static bool ImportFile(
        const String& sourcePath,
        const String& outputDirectory,
        const String& requestedAssetName,
        String& outStatus);

private:
    static void CopyStringToBuffer(const String& value, char* buffer, size_t bufferSize);

private:
    bool m_ShowWindow = false;
    char m_SourcePath[512] = "assets/models/ciri.fbx";
    char m_OutputDirectory[512] = "assets";
    char m_AssetName[256] = "";
    String m_Status;
};
