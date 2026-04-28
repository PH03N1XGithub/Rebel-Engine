#pragma once

#include "Engine/Framework/EnginePch.h"
#include "Core/AssetPtrBase.h"

class ContentBrowserPanel
{
public:
    void Draw();
    AssetHandle ConsumeOpenAssetRequest();

    enum class BrowserEntryKind
    {
        None,
        Folder,
        Asset
    };

    struct BrowserEntry
    {
        BrowserEntryKind Kind = BrowserEntryKind::None;
        String Path;
    };

    void QueueRename(BrowserEntryKind kind, const String& path, const String& currentName);
    void QueueDelete(BrowserEntryKind kind, const String& path);
    String m_CurrentDirectory;
    String m_AssetRootDirectory;
    String m_SelectedEntryPath;
    AssetHandle m_PendingOpenAsset = 0;
    char m_FilterBuffer[128] = {};
    BrowserEntry m_PendingRename;
    BrowserEntry m_PendingDelete;
    BrowserEntry m_PendingMove;
    char m_RenameBuffer[128] = {};
    float m_ThumbnailSize = 72.0f;
    String m_PendingMoveTargetPath;
    TArray<String> m_CreatedDirectories;
    TArray<String> m_DeleteReferencers;
    bool m_OpenRenamePopupPending = false;
    bool m_CreateFolderPending = false;
    String m_PendingCreateFolderParentPath;
    bool m_DeletePending = false;
    bool m_DeleteAnalysisReady = false;
};
