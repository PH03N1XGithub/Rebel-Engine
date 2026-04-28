#include "Engine/Framework/EnginePch.h"
#include "Engine/Assets/AssetManagerModule.h"
#include "Engine/Framework/BaseEngine.h"
#include "Engine/Assets/AssetFileHeader.h"

DEFINE_LOG_CATEGORY(AssetManagerLog)

void AssetManagerModule::ScanDirectory()
{
    namespace fs = std::filesystem;

    m_Registry.Clear();

    std::vector<fs::path> roots;
    auto addRootIfExists = [&roots](const char* path)
    {
        fs::path rootPath(path);
        if (!fs::exists(rootPath))
            return;

        for (const fs::path& existing : roots)
        {
            if (existing == rootPath)
                return;
        }

        roots.push_back(rootPath);
    };

    addRootIfExists("Editor/assets");
    addRootIfExists("editor/assets");
    addRootIfExists("Assets");
    addRootIfExists("assets");

    if (roots.empty())
        return;

    for (const fs::path& root : roots)
    {
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

            if (!in || header.Magic != AssetFileHeader::MagicValue)
                continue;

            if (header.Version < 1)
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
}




