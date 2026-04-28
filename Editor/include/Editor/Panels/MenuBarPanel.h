#pragma once

#include "Engine/Framework/EnginePch.h"

class Asset;

class MenuBarPanel
{
public:
    void SetAnimationImporterVisibility(bool* showAnimationImporterWindow);
    void SetStaticMeshImporterVisibility(bool* showStaticMeshImporterWindow);
    void SetSkeletalMeshImporterVisibility(bool* showSkeletalMeshImporterWindow);
    float GetBarHeight() const { return m_BarHeight; }

    void Draw();

private:
    bool HandleOpenScene();
    bool HandleSaveScene(bool saveAs);

    float m_BarHeight = 22.0f;
    Asset* m_TestAsset = nullptr;
    bool* m_ShowAnimationImporterWindow = nullptr;
    bool* m_ShowStaticMeshImporterWindow = nullptr;
    bool* m_ShowSkeletalMeshImporterWindow = nullptr;
};
