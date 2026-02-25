#include "EnginePch.h"
#include "AssetManager/AssetManagerModule.h"
#include "BaseEngine.h"
#include "AssetManager/AssetFileHeader.h"

DEFINE_LOG_CATEGORY(AssetManagerLog)

void AssetManagerModule::ScanDirectory()
{
    namespace fs = std::filesystem;

    m_Registry.Clear();

    fs::path root;
    if (fs::exists("Editor/assets"))      root = "Editor/assets";
    else if (fs::exists("editor/assets")) root = "editor/assets";
    else if (fs::exists("Assets"))        root = "Assets";
    else if (fs::exists("assets"))        root = "assets";
    else
        return;

    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(root))
    {
        if (!entry.is_regular_file())
            continue;

        const fs::path& filePath = entry.path();
        if (filePath.extension() != ".rasset")
            continue;

        std::ifstream in(filePath, std::ios::binary);
        if (!in.is_open())
            continue;

        AssetFileHeader header{};
        in.read((char*)&header, sizeof(header));

        if (!in || header.Magic != AssetFileHeader::MagicValue) // 'RAST'
            continue;

        if (header.Version < 1)
            continue;
        
        if (!in)
            continue;

        const Rebel::Core::Reflection::TypeInfo* type =
            Rebel::Core::Reflection::TypeRegistry::Get().GetTypeByHash(header.TypeHash);

        if (!type)
            continue;

        fs::path noExt = filePath;
        noExt.replace_extension("");

        AssetMeta meta;
        meta.ID         = AssetHandle(header.AssetID);
        meta.Type       = type;
        meta.Path       = String(noExt.generic_string().c_str());
        meta.FileSize   = (uint64)entry.file_size();

        m_Registry.Register(meta);
    }
}


