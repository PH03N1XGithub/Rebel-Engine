#include "Editor/Panels/ContentBrowserPanel.h"
#include "EditorEngine.h"

#include "Editor/UI/EditorImGui.h"

#include "Core/AssetPtrBase.h"
#include "Core/Serialization/YamlSerializer.h"
#include "Engine/Framework/EnginePch.h"
#include "Engine/Framework/BaseEngine.h"
#include "Engine/Animation/AnimGraphAsset.h"
#include "Engine/Animation/AnimationAsset.h"
#include "Engine/Animation/SkeletalMeshAsset.h"
#include "Engine/Assets/AssetManagerModule.h"
#include "Engine/Assets/PrefabAsset.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "ThirdParty/IconsFontAwesome6.h"
#include "ThirdParty/stb_image.h"

#include <cstdio>
#include <filesystem>
#include <memory>
#include <unordered_map>

DEFINE_LOG_CATEGORY(ContentBrowserLog)

namespace
{
namespace fs = std::filesystem;

constexpr const char* kAssetPayloadType = "CONTENT_BROWSER_ASSET";
constexpr const char* kFolderPayloadType = "CONTENT_BROWSER_FOLDER";

struct IconTexture
{
    GLuint Handle = 0;
    float Width = 0.0f;
    float Height = 0.0f;
};

using DirectoryList = TArray<String>;
using AssetList = TArray<const AssetMeta*>;

String NormalizePath(const fs::path& path)
{
    return String(path.lexically_normal().generic_string().c_str());
}

DirectoryList GetContentRootCandidates()
{
    DirectoryList roots;
    static const char* kRoots[] =
    {
        "Editor/assets",
        "editor/assets",
        "../Editor/assets",
        "../editor/assets",
        "../../Editor/assets",
        "../../editor/assets",
        "../../../Editor/assets",
        "../../../editor/assets",
        "../../../../Editor/assets",
        "../../../../editor/assets",
        "Assets",
        "assets"
    };

    for (const char* root : kRoots)
    {
        const fs::path rootPath(root);
        if (fs::exists(rootPath) && fs::is_directory(rootPath))
        {
            const String normalized = NormalizePath(rootPath);
            bool exists = false;
            for (const String& existing : roots)
            {
                if (Rebel::Core::ToLower(existing) == Rebel::Core::ToLower(normalized))
                {
                    exists = true;
                    break;
                }
            }

            if (!exists)
                roots.Add(normalized);
        }
    }

    return roots;
}

String GetLeafName(const String& path)
{
    return String(fs::path(path.c_str()).filename().generic_string().c_str());
}

String GetDirectoryPath(const String& path)
{
    return String(fs::path(path.c_str()).parent_path().generic_string().c_str());
}

bool EqualsInsensitive(const String& lhs, const String& rhs)
{
    return Rebel::Core::ToLower(lhs) == Rebel::Core::ToLower(rhs);
}

bool ContainsInsensitive(const String& text, const String& filterLower)
{
    if (filterLower.length() == 0)
        return true;

    const String lowerText = Rebel::Core::ToLower(text);
    if (filterLower.length() > lowerText.length())
        return false;

    for (size_t start = 0; start + filterLower.length() <= lowerText.length(); ++start)
    {
        bool matches = true;
        for (size_t i = 0; i < filterLower.length(); ++i)
        {
            if (lowerText[start + i] != filterLower[i])
            {
                matches = false;
                break;
            }
        }

        if (matches)
            return true;
    }

    return false;
}

bool StartsWithInsensitive(const String& text, const String& prefix)
{
    return Rebel::Core::StartsWith(Rebel::Core::ToLower(text), Rebel::Core::ToLower(prefix));
}

bool EndsWithInsensitive(const String& text, const String& suffix)
{
    const String lowerText = Rebel::Core::ToLower(text);
    const String lowerSuffix = Rebel::Core::ToLower(suffix);
    if (lowerSuffix.length() > lowerText.length())
        return false;

    const size_t offset = lowerText.length() - lowerSuffix.length();
    for (size_t i = 0; i < lowerSuffix.length(); ++i)
    {
        if (lowerText[offset + i] != lowerSuffix[i])
            return false;
    }

    return true;
}

String BuildUniqueAssetPath(const String& parentPath, const String& baseName)
{
    String candidate = parentPath.length() > 0 ? parentPath + "/" + baseName : baseName;
    int suffix = 1;
    while (fs::exists(fs::path((candidate + ".rasset").c_str())))
    {
        candidate = (parentPath.length() > 0 ? parentPath + "/" : String()) +
            baseName +
            "_" +
            String(std::to_string(suffix++).c_str());
    }
    return candidate;
}

bool IsSameOrDescendantPath(const String& path, const String& parentPath)
{
    if (parentPath.length() == 0)
        return true;

    const String lowerPath = Rebel::Core::ToLower(path);
    const String lowerParent = Rebel::Core::ToLower(parentPath);
    if (lowerPath == lowerParent)
        return true;

    if (lowerPath.length() <= lowerParent.length() || lowerPath[lowerParent.length()] != '/')
        return false;

    for (size_t i = 0; i < lowerParent.length(); ++i)
    {
        if (lowerPath[i] != lowerParent[i])
            return false;
    }

    return true;
}

bool ContainsChar(const String& text, char value)
{
    for (size_t i = 0; i < text.length(); ++i)
    {
        if (text[i] == value)
            return true;
    }

    return false;
}

bool TryStripPrefixPath(const String& path, const String& prefix, String& suffix)
{
    if (!IsSameOrDescendantPath(path, prefix))
        return false;

    if (EqualsInsensitive(path, prefix))
    {
        suffix = String();
        return true;
    }

    suffix = path.Substr(prefix.length() + 1, path.length() - prefix.length() - 1);
    return true;
}

fs::path ResolveContentPath(const String& browserPath)
{
    if (browserPath.length() == 0)
        return {};

    const fs::path direct(browserPath.c_str());
    if (fs::exists(direct) || fs::exists(direct.parent_path()))
        return direct;

    const DirectoryList contentRoots = GetContentRootCandidates();
    for (const String& root : contentRoots)
    {
        String suffix;
        if (TryStripPrefixPath(browserPath, root, suffix))
            return suffix.length() > 0 ? fs::path(root.c_str()) / suffix.c_str() : fs::path(root.c_str());
    }

    static const char* kLogicalRoots[] = { "Editor/assets", "editor/assets", "Assets", "assets" };
    for (const char* logicalRoot : kLogicalRoots)
    {
        String suffix;
        if (!TryStripPrefixPath(browserPath, String(logicalRoot), suffix))
            continue;

        for (const String& root : contentRoots)
            return suffix.length() > 0 ? fs::path(root.c_str()) / suffix.c_str() : fs::path(root.c_str());
    }

    return direct;
}

bool ContainsTargetHandle(const TArray<AssetHandle>& targetHandles, const AssetHandle handle)
{
    if (!IsValidAssetHandle(handle))
        return false;

    for (const AssetHandle& targetHandle : targetHandles)
    {
        if (targetHandle == handle)
            return true;
    }

    return false;
}

void AddUniqueString(TArray<String>& values, const String& value)
{
    if (value.length() == 0)
        return;

    for (const String& existing : values)
    {
        if (EqualsInsensitive(existing, value))
            return;
    }

    values.Add(value);
}

void CollectAssetReferenceKeysFromType(
    const Rebel::Core::Reflection::TypeInfo* typeInfo,
    TArray<String>& referenceKeys)
{
    if (!typeInfo)
        return;

    if (typeInfo->Super)
        CollectAssetReferenceKeysFromType(typeInfo->Super, referenceKeys);

    for (const Rebel::Core::Reflection::PropertyInfo& prop : typeInfo->Properties)
    {
        if (prop.Type == Rebel::Core::Reflection::EPropertyType::Asset)
            AddUniqueString(referenceKeys, prop.Name);
    }
}

TArray<String> BuildAssetReferenceKeyList()
{
    TArray<String> referenceKeys;
    const auto& types = Rebel::Core::Reflection::TypeRegistry::Get().GetTypes();
    for (const auto& typePair : types)
        CollectAssetReferenceKeysFromType(typePair.Value, referenceKeys);

    AddUniqueString(referenceKeys, "m_SkeletonID");
    return referenceKeys;
}

bool NodeReferencesTargetHandle(
    const YAML::Node& node,
    const TArray<String>& referenceKeys,
    const TArray<AssetHandle>& targetHandles)
{
    if (!node || !node.IsDefined())
        return false;

    if (node.IsMap())
    {
        for (const auto& entry : node)
        {
            const YAML::Node& keyNode = entry.first;
            const YAML::Node& valueNode = entry.second;
            if (keyNode && keyNode.IsScalar())
            {
                const String key = keyNode.as<String>();
                bool isReferenceKey = false;
                for (const String& referenceKey : referenceKeys)
                {
                    if (key == referenceKey)
                    {
                        isReferenceKey = true;
                        break;
                    }
                }

                if (isReferenceKey && valueNode && valueNode.IsScalar())
                {
                    try
                    {
                        if (ContainsTargetHandle(targetHandles, AssetHandle(valueNode.as<uint64>())))
                            return true;
                    }
                    catch (const YAML::Exception&)
                    {
                    }
                }
            }

            if (NodeReferencesTargetHandle(valueNode, referenceKeys, targetHandles))
                return true;
        }

        return false;
    }

    if (node.IsSequence())
    {
        for (const auto& valueNode : node)
        {
            if (NodeReferencesTargetHandle(valueNode, referenceKeys, targetHandles))
                return true;
        }
    }

    return false;
}

bool PrefabYamlReferencesTargetHandle(
    const String& yamlText,
    const TArray<String>& referenceKeys,
    const TArray<AssetHandle>& targetHandles)
{
    if (yamlText.length() == 0)
        return false;

    try
    {
        return NodeReferencesTargetHandle(YAML::Load(yamlText.c_str()), referenceKeys, targetHandles);
    }
    catch (const YAML::Exception&)
    {
        return false;
    }
}

bool FileYamlReferencesTargetHandle(
    const fs::path& path,
    const TArray<String>& referenceKeys,
    const TArray<AssetHandle>& targetHandles)
{
    if (path.empty() || !fs::exists(path) || !fs::is_regular_file(path))
        return false;

    try
    {
        return NodeReferencesTargetHandle(YAML::LoadFile(path.string().c_str()), referenceKeys, targetHandles);
    }
    catch (const YAML::Exception&)
    {
        return false;
    }
}

bool AssetObjectReferencesTargetHandle(
    const Rebel::Core::Reflection::TypeInfo* typeInfo,
    void* object,
    const TArray<AssetHandle>& targetHandles)
{
    if (!typeInfo || !object)
        return false;

    if (typeInfo->Super && AssetObjectReferencesTargetHandle(typeInfo->Super, object, targetHandles))
        return true;

    for (const Rebel::Core::Reflection::PropertyInfo& prop : typeInfo->Properties)
    {
        void* propertyPtr = reinterpret_cast<uint8*>(object) + prop.Offset;
        if (!propertyPtr)
            continue;

        if (prop.Type == Rebel::Core::Reflection::EPropertyType::Asset)
        {
            AssetPtrBase* assetPtr = reinterpret_cast<AssetPtrBase*>(propertyPtr);
            if (assetPtr && ContainsTargetHandle(targetHandles, assetPtr->GetHandle()))
                return true;
        }
        else if (prop.ClassType && prop.Type != Rebel::Core::Reflection::EPropertyType::Class)
        {
            if (AssetObjectReferencesTargetHandle(prop.ClassType, propertyPtr, targetHandles))
                return true;
        }
    }

    return false;
}

bool AssetReferencesTargetHandle(
    AssetManagerModule& assetModule,
    const AssetMeta& meta,
    const TArray<String>& referenceKeys,
    const TArray<AssetHandle>& targetHandles)
{
    Asset* asset = assetModule.GetManager().Load(meta.ID);
    if (!asset)
        return false;

    if (meta.Type && AssetObjectReferencesTargetHandle(meta.Type, asset, targetHandles))
        return true;

    if (const auto* animationAsset = dynamic_cast<const AnimationAsset*>(asset))
    {
        if (ContainsTargetHandle(targetHandles, animationAsset->m_SkeletonID))
            return true;
    }

    if (const auto* skeletalMeshAsset = dynamic_cast<const SkeletalMeshAsset*>(asset))
    {
        if (ContainsTargetHandle(targetHandles, skeletalMeshAsset->m_Skeleton.GetHandle()))
            return true;
    }

    if (const auto* animGraphAsset = dynamic_cast<const AnimGraphAsset*>(asset))
    {
        for (const AnimGraphNode& node : animGraphAsset->m_Nodes)
        {
            if (ContainsTargetHandle(targetHandles, node.AnimationClip))
                return true;
        }
    }

    if (const auto* prefabAsset = dynamic_cast<const PrefabAsset*>(asset))
    {
        if (PrefabYamlReferencesTargetHandle(prefabAsset->m_TemplateYaml, referenceKeys, targetHandles))
            return true;
    }

    return false;
}

TArray<AssetHandle> BuildDeleteTargetHandles(
    const ContentBrowserPanel& panel,
    const TMap<AssetHandle, AssetMeta>& registry)
{
    TArray<AssetHandle> targetHandles;
    if (panel.m_PendingDelete.Kind == ContentBrowserPanel::BrowserEntryKind::Asset)
    {
        for (const auto& assetPair : registry)
        {
            if (EqualsInsensitive(assetPair.Value.Path, panel.m_PendingDelete.Path))
            {
                targetHandles.Add(assetPair.Value.ID);
                break;
            }
        }

        return targetHandles;
    }

    if (panel.m_PendingDelete.Kind == ContentBrowserPanel::BrowserEntryKind::Folder)
    {
        for (const auto& assetPair : registry)
        {
            if (IsSameOrDescendantPath(assetPair.Value.Path, panel.m_PendingDelete.Path))
                targetHandles.Add(assetPair.Value.ID);
        }
    }

    return targetHandles;
}

void RefreshDeleteReferencerAnalysis(ContentBrowserPanel& panel, AssetManagerModule& assetModule)
{
    panel.m_DeleteReferencers.Clear();
    panel.m_DeleteAnalysisReady = true;

    if (panel.m_PendingDelete.Kind == ContentBrowserPanel::BrowserEntryKind::None)
        return;

    const auto& registry = assetModule.GetRegistry().GetAll();
    const TArray<AssetHandle> targetHandles = BuildDeleteTargetHandles(panel, registry);
    if (targetHandles.Num() == 0)
        return;

    const TArray<String> referenceKeys = BuildAssetReferenceKeyList();

    for (const auto& assetPair : registry)
    {
        const AssetMeta& meta = assetPair.Value;
        if (panel.m_PendingDelete.Kind == ContentBrowserPanel::BrowserEntryKind::Asset)
        {
            if (EqualsInsensitive(meta.Path, panel.m_PendingDelete.Path))
                continue;
        }
        else if (panel.m_PendingDelete.Kind == ContentBrowserPanel::BrowserEntryKind::Folder)
        {
            if (IsSameOrDescendantPath(meta.Path, panel.m_PendingDelete.Path))
                continue;
        }

        if (AssetReferencesTargetHandle(assetModule, meta, referenceKeys, targetHandles))
            AddUniqueString(panel.m_DeleteReferencers, String("Asset: ") + meta.Path);
    }

    if (EditorEngine* editor = dynamic_cast<EditorEngine*>(GEngine))
    {
        const String& currentScenePath = editor->GetCurrentScenePath();
        if (currentScenePath.length() > 0 && FileYamlReferencesTargetHandle(fs::path(currentScenePath.c_str()), referenceKeys, targetHandles))
            AddUniqueString(panel.m_DeleteReferencers, String("Level: ") + currentScenePath);
    }

    static const char* kSceneSearchRoots[] = { ".", "Editor", "RebelEngine" };
    for (const char* rootText : kSceneSearchRoots)
    {
        const fs::path root(rootText);
        std::error_code ec;
        if (!fs::exists(root, ec))
            continue;

        for (fs::recursive_directory_iterator it(root, ec), end; it != end; it.increment(ec))
        {
            if (ec)
                break;

            const fs::path scenePath = it->path();
            if (!it->is_regular_file())
                continue;

            const String extension = Rebel::Core::ToLower(String(scenePath.extension().generic_string().c_str()));
            if (extension != ".ryml")
                continue;

            const String normalizedScenePath = NormalizePath(scenePath);
            if (EqualsInsensitive(normalizedScenePath, "TempPIE.Ryml") || EndsWithInsensitive(normalizedScenePath, "/TempPIE.Ryml"))
                continue;

            if (FileYamlReferencesTargetHandle(scenePath, referenceKeys, targetHandles))
                AddUniqueString(panel.m_DeleteReferencers, String("Level: ") + normalizedScenePath);
        }
    }
}

void AddUniqueDirectory(DirectoryList& directories, const String& directory)
{
    if (directory.length() == 0)
        return;

    for (const String& existing : directories)
    {
        if (EqualsInsensitive(existing, directory))
            return;
    }

    directories.Add(directory);
}

String FindCommonDirectoryRoot(const AssetList& assets)
{
    if (assets.Num() == 0)
        return {};

    auto rootParts = Rebel::Core::Split(GetDirectoryPath(assets[0]->Path), '/');
    for (size_t assetIndex = 1; assetIndex < assets.Num() && rootParts.Num() > 0; ++assetIndex)
    {
        auto pathParts = Rebel::Core::Split(GetDirectoryPath(assets[assetIndex]->Path), '/');
        size_t matchCount = 0;
        const size_t compareCount = std::min(rootParts.Num(), pathParts.Num());
        for (; matchCount < compareCount; ++matchCount)
        {
            if (Rebel::Core::ToLower(rootParts[matchCount]) != Rebel::Core::ToLower(pathParts[matchCount]))
                break;
        }

        while (rootParts.Num() > matchCount)
            rootParts.PopBack();
    }

    return rootParts.Num() > 0 ? Rebel::Core::Join(rootParts, "/") : String();
}

DirectoryList BuildDirectoryList(const AssetList& assets, const String& assetRoot)
{
    DirectoryList directories;
    AddUniqueDirectory(directories, assetRoot);

    for (const AssetMeta* meta : assets)
    {
        String directory = GetDirectoryPath(meta->Path);
        while (directory.length() > 0)
        {
            AddUniqueDirectory(directories, directory);
            if (EqualsInsensitive(directory, assetRoot))
                break;

            const String parent = GetDirectoryPath(directory);
            if (parent.length() == 0 || EqualsInsensitive(parent, directory))
                break;
            directory = parent;
        }
    }

    std::sort(directories.begin(), directories.end(), [](const String& lhs, const String& rhs)
    {
        return std::strcmp(Rebel::Core::ToLower(lhs).c_str(), Rebel::Core::ToLower(rhs).c_str()) < 0;
    });

    return directories;
}

void CollectCreatedDirectories(const DirectoryList& createdDirectories, const String& assetRoot, DirectoryList& directories)
{
    for (const String& createdDirectory : createdDirectories)
    {
        if (createdDirectory.length() == 0)
            continue;

        String directory = createdDirectory;
        while (directory.length() > 0)
        {
            AddUniqueDirectory(directories, directory);
            if (EqualsInsensitive(directory, assetRoot))
                break;

            const String parent = GetDirectoryPath(directory);
            if (parent.length() == 0 || EqualsInsensitive(parent, directory))
                break;
            directory = parent;
        }
    }
}

DirectoryList GetChildDirectories(const String& parentPath, const DirectoryList& directories)
{
    DirectoryList children;
    const String lowerParent = Rebel::Core::ToLower(parentPath);

    for (const String& directory : directories)
    {
        if (EqualsInsensitive(directory, parentPath))
            continue;

        const String lowerDirectory = Rebel::Core::ToLower(directory);
        if (lowerDirectory.length() <= lowerParent.length())
            continue;
        bool hasPrefix = true;
        for (size_t i = 0; i < lowerParent.length(); ++i)
        {
            if (lowerDirectory[i] != lowerParent[i])
            {
                hasPrefix = false;
                break;
            }
        }

        if (!hasPrefix || lowerDirectory[lowerParent.length()] != '/')
            continue;

        const String remaining = directory.Substr(parentPath.length() + 1, directory.length() - parentPath.length() - 1);
        if (!ContainsChar(remaining, '/'))
            children.Add(directory);
    }

    std::sort(children.begin(), children.end(), [](const String& lhs, const String& rhs)
    {
        return std::strcmp(Rebel::Core::ToLower(lhs).c_str(), Rebel::Core::ToLower(rhs).c_str()) < 0;
    });

    return children;
}

fs::path FindEditorUiAssetPath(const char* fileName)
{
    static const char* kPrefixes[] =
    {
        "Editor/assets/ui",
        "../Editor/assets/ui",
        "../../Editor/assets/ui",
        "../../../Editor/assets/ui",
        "../../../../Editor/assets/ui"
    };

    for (const char* prefix : kPrefixes)
    {
        const fs::path candidate = fs::path(prefix) / fileName;
        if (fs::exists(candidate))
            return candidate;
    }

    return {};
}

IconTexture LoadEditorIconTexture(const fs::path& path)
{
    if (path.empty())
        return {};

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_set_flip_vertically_on_load(0);
    stbi_uc* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
    stbi_set_flip_vertically_on_load(1);
    if (!pixels || width <= 0 || height <= 0)
    {
        if (pixels)
            stbi_image_free(pixels);
        return {};
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    stbi_image_free(pixels);
    return { texture, static_cast<float>(width), static_cast<float>(height) };
}

const IconTexture& GetBrowserPreviewTexture(const char* relativePath)
{
    static std::unordered_map<std::string, IconTexture> cache;
    static IconTexture empty;
    if (!relativePath || relativePath[0] == '\0')
        return empty;

    const auto it = cache.find(relativePath);
    if (it != cache.end())
        return it->second;

    return cache.emplace(relativePath, LoadEditorIconTexture(FindEditorUiAssetPath(relativePath))).first->second;
}

void DrawFittedImage(ImDrawList* drawList, const IconTexture& texture, const ImVec2& min, const ImVec2& max, ImU32 tintColor = IM_COL32_WHITE)
{
    if (!drawList || texture.Handle == 0 || texture.Width <= 0.0f || texture.Height <= 0.0f)
        return;

    const float boxWidth = max.x - min.x;
    const float boxHeight = max.y - min.y;
    if (boxWidth <= 0.0f || boxHeight <= 0.0f)
        return;

    const float scale = std::min(boxWidth / texture.Width, boxHeight / texture.Height);
    const ImVec2 size(texture.Width * scale, texture.Height * scale);
    const ImVec2 offset((boxWidth - size.x) * 0.5f, (boxHeight - size.y) * 0.5f);
    drawList->AddImage(
        texture.Handle,
        ImVec2(min.x + offset.x, min.y + offset.y),
        ImVec2(min.x + offset.x + size.x, min.y + offset.y + size.y),
        ImVec2(0.0f, 0.0f),
        ImVec2(1.0f, 1.0f),
        tintColor);
}

const char* GetAssetIconPath(const AssetMeta& meta)
{
    const String extension = Rebel::Core::ToLower(fs::path(meta.Path.c_str()).extension().string().c_str());
    if (extension == ".png")
        return "hazel/ContentBrowser/PNG.png";
    if (extension == ".jpg" || extension == ".jpeg")
        return "hazel/ContentBrowser/JPG.png";
    if (extension == ".fbx")
        return "hazel/ContentBrowser/FBX.png";
    if (extension == ".obj")
        return "hazel/ContentBrowser/OBJ.png";
    if (extension == ".glb")
        return "hazel/ContentBrowser/GLB.png";
    if (extension == ".gltf")
        return "hazel/ContentBrowser/GLTF.png";
    if (extension == ".wav")
        return "hazel/ContentBrowser/WAV.png";
    if (extension == ".mp3")
        return "hazel/ContentBrowser/MP3.png";
    if (extension == ".ogg")
        return "hazel/ContentBrowser/OGG.png";
    if (extension == ".cs")
        return "hazel/ContentBrowser/CS.png";

    if (!meta.Type)
        return "hazel/ContentBrowser/File.png";

    const String typeNameLower = Rebel::Core::ToLower(meta.Type->Name);
    if (ContainsInsensitive(typeNameLower, "animgraph") || ContainsInsensitive(typeNameLower, "animation graph"))
        return "hazel/ContentBrowser/AnimationGraph.png";
    if (ContainsInsensitive(typeNameLower, "animation"))
        return "hazel/ContentBrowser/Animation.png";
    if (ContainsInsensitive(typeNameLower, "skeleton"))
        return "hazel/ContentBrowser/Skeleton.png";
    if (ContainsInsensitive(typeNameLower, "physics material"))
        return "hazel/ContentBrowser/PhysicsMaterial.png";
    if (ContainsInsensitive(typeNameLower, "material"))
        return "hazel/ContentBrowser/Material.png";
    if (ContainsInsensitive(typeNameLower, "static mesh"))
        return "hazel/ContentBrowser/StaticMesh.png";
    if (ContainsInsensitive(typeNameLower, "mesh collider"))
        return "hazel/ContentBrowser/MeshCollider.png";
    if (ContainsInsensitive(typeNameLower, "mesh"))
        return "hazel/ContentBrowser/Mesh.png";
    if (ContainsInsensitive(typeNameLower, "prefab"))
        return "hazel/ContentBrowser/Prefab.png";
    if (ContainsInsensitive(typeNameLower, "font"))
        return "hazel/ContentBrowser/Font.png";
    if (ContainsInsensitive(typeNameLower, "sound graph"))
        return "hazel/ContentBrowser/SoundGraph.png";
    if (ContainsInsensitive(typeNameLower, "sound config"))
        return "hazel/ContentBrowser/SoundConfig.png";

    return "hazel/ContentBrowser/File.png";
}

ImU32 GetAssetIconTint(const AssetMeta& meta)
{
    if (!meta.Type)
        return IM_COL32_WHITE;

    static std::unordered_map<const Rebel::Core::Reflection::TypeInfo*, AssetDisplayColor> colorCache;
    const auto found = colorCache.find(meta.Type);
    if (found != colorCache.end())
        return IM_COL32(found->second.R, found->second.G, found->second.B, found->second.A);

    AssetDisplayColor color = Asset::GetStaticDisplayColor();
    if (meta.Type->CreateInstance && meta.Type->IsA(Asset::StaticType()))
    {
        std::unique_ptr<Asset> asset(static_cast<Asset*>(meta.Type->CreateInstance()));
        if (asset)
            color = asset->GetDisplayColor();
    }

    colorCache.emplace(meta.Type, color);

    return IM_COL32(color.R, color.G, color.B, color.A);
}

String TruncateLabel(const String& label, size_t maxChars)
{
    if (label.length() <= maxChars)
        return label;

    if (maxChars <= 3)
        return "...";

    return label.Substr(0, maxChars - 3) + "...";
}

bool DrawBreadcrumbButton(const String& label, bool active)
{
    ImGui::PushStyleColor(ImGuiCol_Button, active ? ImVec4(0.25f, 0.27f, 0.30f, 1.0f) : ImVec4(0.18f, 0.19f, 0.205f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.27f, 0.30f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.24f, 0.27f, 1.0f));
    const bool pressed = ImGui::Button(label.c_str(), ImVec2(0.0f, 24.0f));
    ImGui::PopStyleColor(3);
    return pressed;
}

bool DrawBrowserTile(
    const char* id,
    const IconTexture& previewTexture,
    const String& title,
    const String& subtitle,
    bool selected,
    float tileWidth,
    float tileHeight,
    float previewSize,
    ImU32 previewTint = IM_COL32_WHITE)
{
    ImGui::PushID(id);
    ImGui::PushStyleColor(ImGuiCol_Header, selected ? ImVec4(178.0f / 255.0f, 128.0f / 255.0f, 51.0f / 255.0f, 0.72f) : ImVec4(48.0f / 255.0f, 48.0f / 255.0f, 48.0f / 255.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, selected ? ImVec4(200.0f / 255.0f, 143.0f / 255.0f, 61.0f / 255.0f, 0.84f) : ImVec4(62.0f / 255.0f, 62.0f / 255.0f, 62.0f / 255.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(153.0f / 255.0f, 112.0f / 255.0f, 46.0f / 255.0f, 0.88f));
    ImGui::Selectable("##Tile", selected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(tileWidth, tileHeight));
    ImGui::PopStyleColor(3);

    const bool hovered = ImGui::IsItemHovered();
    const bool activated = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    const bool doubleClicked = hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImU32 borderColor = selected ? IM_COL32(178, 128, 51, 255) : hovered ? IM_COL32(101, 105, 116, 255) : IM_COL32(54, 57, 64, 255);
    const ImU32 fillColor = selected ? IM_COL32(61, 51, 34, 228) : hovered ? IM_COL32(31, 34, 40, 236) : IM_COL32(24, 26, 31, 228);
    Rebel::Editor::UI::DrawSoftShadow(drawList, min, max, 6.0f, 4, 2.5f, hovered || selected ? 1.0f : 0.78f);
    drawList->AddRectFilled(min, max, fillColor, 6.0f);
    drawList->AddRect(min, max, borderColor, 6.0f);

    ImFont* bodyFont = Rebel::Editor::UI::GetFont("Default");
    ImFont* smallFont = Rebel::Editor::UI::GetFont("Small");

    const float previewX = min.x + (tileWidth - previewSize) * 0.5f;
    const float previewY = min.y + 10.0f;
    const ImVec2 previewMin(previewX, previewY);
    const ImVec2 previewMax(previewX + previewSize, previewY + previewSize);
    DrawFittedImage(drawList, previewTexture, previewMin, previewMax, previewTint);

    const String titleLabel = TruncateLabel(title, 15);
    const String subtitleLabel = TruncateLabel(subtitle, 17);
    const float bodyFontSize = bodyFont ? bodyFont->LegacySize : ImGui::GetFontSize();
    const float smallFontSize = smallFont ? smallFont->LegacySize : ImGui::GetFontSize();
    const ImVec2 titleSize = bodyFont ? bodyFont->CalcTextSizeA(bodyFontSize, tileWidth - 12.0f, 0.0f, titleLabel.c_str()) : ImGui::CalcTextSize(titleLabel.c_str());
    const ImVec2 subtitleSize = smallFont ? smallFont->CalcTextSizeA(smallFontSize, tileWidth - 12.0f, 0.0f, subtitleLabel.c_str()) : ImGui::CalcTextSize(subtitleLabel.c_str());

    const float titleY = previewY + previewSize + 8.0f;
    drawList->AddText(bodyFont ? bodyFont : ImGui::GetFont(), bodyFontSize, ImVec2(min.x + (tileWidth - titleSize.x) * 0.5f, titleY), IM_COL32(232, 235, 241, 255), titleLabel.c_str());
    drawList->AddText(smallFont ? smallFont : ImGui::GetFont(), smallFontSize, ImVec2(min.x + (tileWidth - subtitleSize.x) * 0.5f, titleY + 16.0f), IM_COL32(138, 145, 158, 255), subtitleLabel.c_str());

    ImGui::PopID();
    return activated || doubleClicked;
}

bool BeginEntryDragSource(ContentBrowserPanel::BrowserEntryKind kind, const String& path, const String& label)
{
    if (!ImGui::BeginDragDropSource())
        return false;

    const char* payloadType = kind == ContentBrowserPanel::BrowserEntryKind::Folder ? kFolderPayloadType : kAssetPayloadType;
    ImGui::SetDragDropPayload(payloadType, path.c_str(), path.length() + 1);
    ImGui::Text("%s %s", kind == ContentBrowserPanel::BrowserEntryKind::Folder ? ICON_FA_FOLDER : ICON_FA_FILE, label.c_str());
    ImGui::EndDragDropSource();
    return true;
}

bool ApplyMove(ContentBrowserPanel::BrowserEntryKind kind, const String& sourcePath, const String& targetDirectory, String& newPath)
{
    if (sourcePath.length() == 0 || targetDirectory.length() == 0)
        return false;

    const fs::path targetDirectoryFs(targetDirectory.c_str());
    if (!fs::exists(targetDirectoryFs))
        fs::create_directories(targetDirectoryFs);

    if (kind == ContentBrowserPanel::BrowserEntryKind::Asset)
    {
        const String fileName = GetLeafName(sourcePath);
        newPath = NormalizePath(targetDirectoryFs / fileName.c_str());
        if (EqualsInsensitive(newPath, sourcePath))
            return false;

        const fs::path sourceFile((sourcePath + ".rasset").c_str());
        const fs::path targetFile((newPath + ".rasset").c_str());
        if (!fs::exists(sourceFile) || fs::exists(targetFile))
            return false;

        fs::rename(sourceFile, targetFile);
        return true;
    }

    if (kind == ContentBrowserPanel::BrowserEntryKind::Folder)
    {
        const fs::path sourceDirectory(sourcePath.c_str());
        const String folderName = GetLeafName(sourcePath);
        newPath = NormalizePath(targetDirectoryFs / folderName.c_str());
        if (EqualsInsensitive(newPath, sourcePath) || IsSameOrDescendantPath(targetDirectory, sourcePath))
            return false;

        if (!fs::exists(sourceDirectory) || fs::exists(fs::path(newPath.c_str())))
            return false;

        fs::rename(sourceDirectory, fs::path(newPath.c_str()));
        return true;
    }

    return false;
}

bool HandleDirectoryDropTarget(ContentBrowserPanel& panel, const String& targetDirectory)
{
    if (!ImGui::BeginDragDropTarget())
        return false;

    bool changed = false;
    auto acceptMove = [&](ContentBrowserPanel::BrowserEntryKind kind, const char* payloadType)
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(payloadType))
        {
            const char* droppedPath = reinterpret_cast<const char*>(payload->Data);
            if (!droppedPath || droppedPath[0] == '\0')
                return;

            panel.m_PendingMove.Kind = kind;
            panel.m_PendingMove.Path = String(droppedPath);
            panel.m_PendingMoveTargetPath = targetDirectory;
            changed = true;
        }
    };

    acceptMove(ContentBrowserPanel::BrowserEntryKind::Asset, kAssetPayloadType);
    acceptMove(ContentBrowserPanel::BrowserEntryKind::Folder, kFolderPayloadType);
    ImGui::EndDragDropTarget();
    return changed;
}

void CollectVisibleEntries(const TMap<AssetHandle, AssetMeta>& registry, const DirectoryList& directories, const String& currentDirectory, const String& filterLower, DirectoryList& folders, AssetList& files)
{
    if (currentDirectory.length() > 0)
    {
        const DirectoryList childDirectories = GetChildDirectories(currentDirectory, directories);
        for (const String& childDirectory : childDirectories)
        {
            const String folderName = GetLeafName(childDirectory);
            const bool matchesFilter = filterLower.length() == 0 || ContainsInsensitive(folderName, filterLower) || ContainsInsensitive(childDirectory, filterLower);
            if (matchesFilter)
                AddUniqueDirectory(folders, childDirectory);
        }
    }

    for (auto& assetPair : registry)
    {
        const AssetMeta& meta = assetPair.Value;
        const String& path = meta.Path;
        if (currentDirectory.length() > 0 && !StartsWithInsensitive(path, currentDirectory))
            continue;

        String relative = currentDirectory.length() > 0 ? path.Substr(currentDirectory.length(), path.length() - currentDirectory.length()) : path;
        if (relative.length() > 0 && (relative[0] == '/' || relative[0] == '\\'))
            relative = relative.Substr(1, relative.length() - 1);
        if (relative.length() == 0)
            continue;

        auto parts = Rebel::Core::Split(relative, '/');
        if (parts.Num() == 1)
        {
            const String fileName = parts[0];
            const bool matchesFilter = filterLower.length() == 0 || ContainsInsensitive(fileName, filterLower) || (meta.Type && ContainsInsensitive(meta.Type->Name, filterLower));
            if (matchesFilter)
                files.Add(&meta);
        }
        else
        {
            const String& folder = parts[0];
            const bool matchesFilter = filterLower.length() == 0 || ContainsInsensitive(relative, filterLower) || ContainsInsensitive(folder, filterLower);
            if (!matchesFilter)
                continue;

            AddUniqueDirectory(folders, currentDirectory.length() > 0 ? currentDirectory + "/" + folder : folder);
        }
    }
}

void OpenRenamePopup(ContentBrowserPanel& panel, ContentBrowserPanel::BrowserEntryKind kind, const String& path)
{
    panel.m_PendingRename.Kind = kind;
    panel.m_PendingRename.Path = path;
    std::snprintf(panel.m_RenameBuffer, IM_ARRAYSIZE(panel.m_RenameBuffer), "%s", GetLeafName(path).c_str());
    panel.m_OpenRenamePopupPending = true;
}

void OpenDeletePopup(ContentBrowserPanel& panel, ContentBrowserPanel::BrowserEntryKind kind, const String& path)
{
    panel.m_PendingDelete.Kind = kind;
    panel.m_PendingDelete.Path = path;
    panel.m_DeleteReferencers.Clear();
    panel.m_DeleteAnalysisReady = false;
    panel.m_DeletePending = true;
}

void OpenCreateFolderPopup(ContentBrowserPanel& panel, const String& parentPath)
{
}

String BuildIndexedFolderName(int index)
{
    if (index <= 0)
        return "New Folder";

    return String("New Folder ") + String(std::to_string(index).c_str());
}

bool CreateFolderImmediate(ContentBrowserPanel& panel, AssetManagerModule& assetModule, const String& requestedParentPath)
{
    const String parentPath = requestedParentPath.length() > 0 ? requestedParentPath : panel.m_AssetRootDirectory;
    const fs::path parentDirectoryPath = ResolveContentPath(parentPath);
    RB_LOG(
        ContentBrowserLog,
        info,
        "CreateFolder requested | BrowserParent='{}' | ResolvedParent='{}' | AssetRoot='{}'",
        parentPath.c_str(),
        parentDirectoryPath.string().c_str(),
        panel.m_AssetRootDirectory.c_str());

    if (parentPath.length() == 0 || parentDirectoryPath.empty())
    {
        RB_LOG(ContentBrowserLog, error, "CreateFolder failed: no valid parent path");
        return false;
    }

    if (!fs::exists(parentDirectoryPath))
    {
        RB_LOG(
            ContentBrowserLog,
            error,
            "CreateFolder failed: parent directory does not exist | ResolvedParent='{}'",
            parentDirectoryPath.string().c_str());
        return false;
    }

    if (!fs::is_directory(parentDirectoryPath))
    {
        RB_LOG(
            ContentBrowserLog,
            error,
            "CreateFolder failed: parent path is not a directory | ResolvedParent='{}'",
            parentDirectoryPath.string().c_str());
        return false;
    }

    String createdBrowserPath;
    fs::path createdDirectoryPath;
    for (int index = 0; index < 1024; ++index)
    {
        const String folderName = BuildIndexedFolderName(index);
        const String browserPath = parentPath + "/" + folderName;
        const fs::path directoryPath = parentDirectoryPath / folderName.c_str();
        if (fs::exists(directoryPath))
            continue;

        if (!fs::create_directory(directoryPath))
        {
            RB_LOG(
                ContentBrowserLog,
                error,
                "CreateFolder failed: create_directory returned false | ResolvedPath='{}'",
                directoryPath.string().c_str());
            return false;
        }

        createdBrowserPath = browserPath;
        createdDirectoryPath = directoryPath;
        break;
    }

    if (createdBrowserPath.length() == 0)
    {
        RB_LOG(
            ContentBrowserLog,
            error,
            "CreateFolder failed: exhausted unique folder name attempts | Parent='{}'",
            parentPath.c_str());
        return false;
    }

    RB_LOG(
        ContentBrowserLog,
        info,
        "CreateFolder succeeded | BrowserPath='{}' | ResolvedPath='{}'",
        createdBrowserPath.c_str(),
        createdDirectoryPath.string().c_str());

    assetModule.RescanAssets();
    panel.m_CurrentDirectory = parentPath;
    panel.m_SelectedEntryPath = createdBrowserPath;
    AddUniqueDirectory(panel.m_CreatedDirectories, createdBrowserPath);
    return true;
}

void QueueCreateFolder(ContentBrowserPanel& panel, const String& requestedParentPath)
{
    panel.m_PendingCreateFolderParentPath = requestedParentPath.length() > 0 ? requestedParentPath : panel.m_AssetRootDirectory;
    panel.m_CreateFolderPending = true;
}

void CreateAnimGraphAsset(ContentBrowserPanel& panel, const String& parentPath)
{
    AssetManagerModule* assetModule = GEngine ? GEngine->GetModuleManager().GetModule<AssetManagerModule>() : nullptr;
    if (!assetModule)
        return;

    AnimGraphAsset graph;
    graph.EnsureDefaultGraph();

    const String assetPath = BuildUniqueAssetPath(parentPath, "AnimGraph");
    if (assetModule->SaveAssetToFile(assetPath + ".rasset", graph))
    {
        assetModule->RescanAssets();
        panel.m_CurrentDirectory = parentPath;
        panel.m_SelectedEntryPath = assetPath;
    }
}

void DrawFolderContextMenu(ContentBrowserPanel& panel, const String& folderPath)
{
    if (!ImGui::BeginPopupContextItem())
        return;

    if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Open"))
    {
        panel.m_CurrentDirectory = folderPath;
        panel.m_SelectedEntryPath = folderPath;
    }

    if (ImGui::MenuItem(ICON_FA_FOLDER_PLUS " New Folder"))
        QueueCreateFolder(panel, folderPath);

    if (ImGui::MenuItem(ICON_FA_I_CURSOR " Rename"))
        OpenRenamePopup(panel, ContentBrowserPanel::BrowserEntryKind::Folder, folderPath);

    if (ImGui::MenuItem(ICON_FA_TRASH " Delete"))
        OpenDeletePopup(panel, ContentBrowserPanel::BrowserEntryKind::Folder, folderPath);

    ImGui::EndPopup();
}

void DrawFolderTreeBackgroundContextMenu(ContentBrowserPanel& panel, const String& parentPath)
{
    if (!ImGui::BeginPopupContextWindow("ContentBrowserFolderTreeContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        return;

    if (ImGui::MenuItem(ICON_FA_FOLDER_PLUS " New Folder"))
        QueueCreateFolder(panel, parentPath);

    if (ImGui::MenuItem(ICON_FA_DIAGRAM_PROJECT " New Anim Graph"))
        CreateAnimGraphAsset(panel, parentPath);

    ImGui::EndPopup();
}

void DrawGridBackgroundContextMenu(ContentBrowserPanel& panel, const String& parentPath)
{
    if (!ImGui::BeginPopupContextWindow("ContentBrowserGridContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        return;

    if (ImGui::MenuItem(ICON_FA_FOLDER_PLUS " New Folder"))
        QueueCreateFolder(panel, parentPath);

    if (ImGui::MenuItem(ICON_FA_DIAGRAM_PROJECT " New Anim Graph"))
        CreateAnimGraphAsset(panel, parentPath);

    ImGui::EndPopup();
}

void DrawAssetContextMenu(ContentBrowserPanel& panel, const AssetMeta& meta)
{
    if (!ImGui::BeginPopupContextItem())
        return;

    if (ImGui::MenuItem(ICON_FA_ARROW_UP_RIGHT_FROM_SQUARE " Open"))
        panel.m_PendingOpenAsset = meta.ID;

    if (ImGui::MenuItem(ICON_FA_I_CURSOR " Rename"))
        OpenRenamePopup(panel, ContentBrowserPanel::BrowserEntryKind::Asset, meta.Path);

    if (ImGui::MenuItem(ICON_FA_TRASH " Delete"))
        OpenDeletePopup(panel, ContentBrowserPanel::BrowserEntryKind::Asset, meta.Path);

    ImGui::EndPopup();
}

void DrawDirectoryTreeNode(ContentBrowserPanel& panel, AssetManagerModule& assetModule, const String& directoryPath, const DirectoryList& directories, const String& filterLower)
{
    const DirectoryList children = GetChildDirectories(directoryPath, directories);
    const bool selected = EqualsInsensitive(panel.m_CurrentDirectory, directoryPath) || EqualsInsensitive(panel.m_SelectedEntryPath, directoryPath);
    const bool matchesFilter = filterLower.length() == 0 || ContainsInsensitive(directoryPath, filterLower);
    const bool hasChildren = children.Num() > 0;
    bool descendantMatches = false;

    if (!matchesFilter && filterLower.length() > 0)
    {
        for (const String& child : children)
        {
            if (ContainsInsensitive(child, filterLower))
            {
                descendantMatches = true;
                break;
            }
        }
    }

    if (filterLower.length() > 0 && !matchesFilter && !descendantMatches)
        return;

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (selected)
        flags |= ImGuiTreeNodeFlags_Selected;
    if (!hasChildren)
        flags |= ImGuiTreeNodeFlags_Leaf;
    if (descendantMatches)
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);

    const String label = String(ICON_FA_FOLDER) + " " + GetLeafName(directoryPath);
    const bool open = ImGui::TreeNodeEx(directoryPath.c_str(), flags, "%s", label.c_str());

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
    {
        panel.m_CurrentDirectory = directoryPath;
        panel.m_SelectedEntryPath = directoryPath;
    }

    BeginEntryDragSource(ContentBrowserPanel::BrowserEntryKind::Folder, directoryPath, GetLeafName(directoryPath));
    HandleDirectoryDropTarget(panel, directoryPath);
    DrawFolderContextMenu(panel, directoryPath);

    if (!open)
        return;

    for (const String& child : children)
        DrawDirectoryTreeNode(panel, assetModule, child, directories, filterLower);

    ImGui::TreePop();
}

bool ApplyRename(ContentBrowserPanel& panel, AssetManagerModule& assetModule)
{
    if (panel.m_PendingRename.Kind == ContentBrowserPanel::BrowserEntryKind::None)
        return false;

    String newName = Rebel::Core::Trim(String(panel.m_RenameBuffer));

    if (newName.length() == 0 || ContainsChar(newName, '/') || ContainsChar(newName, '\\'))
        return false;

    const String oldPath = panel.m_PendingRename.Path;
    const String parentPath = GetDirectoryPath(oldPath);
    const String newPath = parentPath.length() > 0 ? parentPath + "/" + newName : newName;
    if (EqualsInsensitive(oldPath, newPath))
        return false;

    if (panel.m_PendingRename.Kind == ContentBrowserPanel::BrowserEntryKind::Asset)
    {
        const fs::path sourceFile((oldPath + ".rasset").c_str());
        const fs::path targetFile((newPath + ".rasset").c_str());
        if (!fs::exists(sourceFile) || fs::exists(targetFile))
            return false;

        fs::rename(sourceFile, targetFile);
    }
    else if (panel.m_PendingRename.Kind == ContentBrowserPanel::BrowserEntryKind::Folder)
    {
        const fs::path sourceDirectory(oldPath.c_str());
        const fs::path targetDirectory(newPath.c_str());
        if (!fs::exists(sourceDirectory) || fs::exists(targetDirectory))
            return false;

        fs::rename(sourceDirectory, targetDirectory);

        if (IsSameOrDescendantPath(panel.m_CurrentDirectory, oldPath))
            panel.m_CurrentDirectory = newPath + panel.m_CurrentDirectory.Substr(oldPath.length(), panel.m_CurrentDirectory.length() - oldPath.length());
        if (IsSameOrDescendantPath(panel.m_SelectedEntryPath, oldPath))
            panel.m_SelectedEntryPath = newPath + panel.m_SelectedEntryPath.Substr(oldPath.length(), panel.m_SelectedEntryPath.length() - oldPath.length());
    }

    assetModule.RescanAssets();
    panel.m_SelectedEntryPath = newPath;
    if (panel.m_PendingRename.Kind == ContentBrowserPanel::BrowserEntryKind::Folder)
    {
        for (String& createdDirectory : panel.m_CreatedDirectories)
        {
            if (IsSameOrDescendantPath(createdDirectory, oldPath))
                createdDirectory = newPath + createdDirectory.Substr(oldPath.length(), createdDirectory.length() - oldPath.length());
        }
    }
    panel.m_PendingRename = {};
    panel.m_RenameBuffer[0] = '\0';
    return true;
}

bool ApplyDelete(ContentBrowserPanel& panel, AssetManagerModule& assetModule)
{
    if (panel.m_PendingDelete.Kind == ContentBrowserPanel::BrowserEntryKind::None)
        return false;

    const String oldPath = panel.m_PendingDelete.Path;
    if (panel.m_PendingDelete.Kind == ContentBrowserPanel::BrowserEntryKind::Asset)
    {
        const fs::path assetFile((oldPath + ".rasset").c_str());
        if (!fs::exists(assetFile))
            return false;

        if (!fs::remove(assetFile))
            return false;
    }
    else if (panel.m_PendingDelete.Kind == ContentBrowserPanel::BrowserEntryKind::Folder)
    {
        const fs::path directoryPath = ResolveContentPath(oldPath);
        if (!fs::exists(directoryPath) || !fs::is_directory(directoryPath))
            return false;

        if (fs::remove_all(directoryPath) == 0)
            return false;

        for (int i = static_cast<int>(panel.m_CreatedDirectories.Num()) - 1; i >= 0; --i)
        {
            if (IsSameOrDescendantPath(panel.m_CreatedDirectories[i], oldPath))
                panel.m_CreatedDirectories.RemoveAt(i);
        }

        if (IsSameOrDescendantPath(panel.m_CurrentDirectory, oldPath))
            panel.m_CurrentDirectory = GetDirectoryPath(oldPath);
    }

    if (EqualsInsensitive(panel.m_SelectedEntryPath, oldPath))
        panel.m_SelectedEntryPath = String();

    assetModule.RescanAssets();
    panel.m_PendingDelete = {};
    panel.m_DeletePending = false;
    panel.m_DeleteReferencers.Clear();
    panel.m_DeleteAnalysisReady = false;
    return true;
}

bool ApplyQueuedMove(ContentBrowserPanel& panel, AssetManagerModule& assetModule)
{
    if (panel.m_PendingMove.Kind == ContentBrowserPanel::BrowserEntryKind::None || panel.m_PendingMoveTargetPath.length() == 0)
        return false;

    const ContentBrowserPanel::BrowserEntryKind moveKind = panel.m_PendingMove.Kind;
    const String sourcePath = panel.m_PendingMove.Path;
    const String targetPath = panel.m_PendingMoveTargetPath;
    String newPath;
    const bool moved = ApplyMove(moveKind, sourcePath, targetPath, newPath);
    panel.m_PendingMove = {};
    panel.m_PendingMoveTargetPath = String();
    if (!moved)
        return false;

    assetModule.RescanAssets();
    panel.m_SelectedEntryPath = newPath;
    if (moveKind == ContentBrowserPanel::BrowserEntryKind::Folder && IsSameOrDescendantPath(panel.m_CurrentDirectory, sourcePath))
        panel.m_CurrentDirectory = newPath + panel.m_CurrentDirectory.Substr(sourcePath.length(), panel.m_CurrentDirectory.length() - sourcePath.length());
    if (moveKind == ContentBrowserPanel::BrowserEntryKind::Folder)
    {
        for (String& createdDirectory : panel.m_CreatedDirectories)
        {
            if (IsSameOrDescendantPath(createdDirectory, sourcePath))
                createdDirectory = newPath + createdDirectory.Substr(sourcePath.length(), createdDirectory.length() - sourcePath.length());
        }
    }
    return true;
}

bool ApplyQueuedCreateFolder(ContentBrowserPanel& panel, AssetManagerModule& assetModule)
{
    if (!panel.m_CreateFolderPending)
        return false;

    const String parentPath = panel.m_PendingCreateFolderParentPath;
    panel.m_CreateFolderPending = false;
    panel.m_PendingCreateFolderParentPath = String();
    return CreateFolderImmediate(panel, assetModule, parentPath);
}

void DrawRenameModal(ContentBrowserPanel& panel, AssetManagerModule& assetModule)
{
    if (panel.m_OpenRenamePopupPending)
    {
        ImGui::OpenPopup("Rename Entry");
        panel.m_OpenRenamePopupPending = false;
    }

    if (!ImGui::BeginPopupModal("Rename Entry", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    ImGui::TextUnformatted(panel.m_PendingRename.Kind == ContentBrowserPanel::BrowserEntryKind::Folder ? "Rename folder" : "Rename asset");
    ImGui::SetNextItemWidth(320.0f);
    ImGui::InputText("##RenameEntry", panel.m_RenameBuffer, IM_ARRAYSIZE(panel.m_RenameBuffer), ImGuiInputTextFlags_AutoSelectAll);

    if (ImGui::Button("Apply", ImVec2(120.0f, 0.0f)))
    {
        if (ApplyRename(panel, assetModule))
            ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
    {
        panel.m_PendingRename = {};
        panel.m_RenameBuffer[0] = '\0';
        panel.m_OpenRenamePopupPending = false;
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void DrawDeleteModal(ContentBrowserPanel& panel, AssetManagerModule& assetModule)
{
    if (panel.m_DeletePending)
    {
        ImGui::OpenPopup("Delete Entry");
        panel.m_DeletePending = false;
    }

    if (!ImGui::BeginPopupModal("Delete Entry", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    if (!panel.m_DeleteAnalysisReady)
        RefreshDeleteReferencerAnalysis(panel, assetModule);

    if (panel.m_PendingDelete.Kind == ContentBrowserPanel::BrowserEntryKind::Folder)
    {
        ImGui::Text("Delete folder '%s'?", GetLeafName(panel.m_PendingDelete.Path).c_str());
        ImGui::TextDisabled("This removes the folder and all files inside it.");
    }
    else
    {
        ImGui::Text("Delete asset '%s'?", GetLeafName(panel.m_PendingDelete.Path).c_str());
        ImGui::TextDisabled("This removes the .rasset file from disk.");
    }

    if (panel.m_DeleteReferencers.Num() > 0)
    {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.94f, 0.72f, 0.33f, 1.0f), "%s Referencers found", ICON_FA_TRIANGLE_EXCLAMATION);
        ImGui::TextWrapped("These assets or levels still reference what you are deleting.");
        ImGui::BeginChild("DeleteReferencers", ImVec2(520.0f, 140.0f), true);
        for (const String& referencer : panel.m_DeleteReferencers)
            ImGui::BulletText("%s", referencer.c_str());
        ImGui::EndChild();
    }

    if (ImGui::Button(panel.m_DeleteReferencers.Num() > 0 ? "Force Delete" : "Delete", ImVec2(120.0f, 0.0f)))
    {
        if (ApplyDelete(panel, assetModule))
            ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
    {
        panel.m_PendingDelete = {};
        panel.m_DeletePending = false;
        panel.m_DeleteReferencers.Clear();
        panel.m_DeleteAnalysisReady = false;
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

}

void ContentBrowserPanel::QueueRename(BrowserEntryKind kind, const String& path, const String& currentName)
{
    m_PendingRename.Kind = kind;
    m_PendingRename.Path = path;
    std::snprintf(m_RenameBuffer, IM_ARRAYSIZE(m_RenameBuffer), "%s", currentName.c_str());
    m_OpenRenamePopupPending = true;
}

void ContentBrowserPanel::QueueDelete(BrowserEntryKind kind, const String& path)
{
    m_PendingDelete.Kind = kind;
    m_PendingDelete.Path = path;
    m_DeleteReferencers.Clear();
    m_DeleteAnalysisReady = false;
}

void ContentBrowserPanel::Draw()
{
    using namespace Rebel::Editor::UI;

    AssetManagerModule* assetModule = GEngine->GetModuleManager().GetModule<AssetManagerModule>();
    auto& registry = assetModule->GetRegistry().GetAll();

    AssetList allAssets;
    for (auto& assetPair : registry)
        allAssets.Add(&assetPair.Value);

    const String assetRoot = FindCommonDirectoryRoot(allAssets);
    m_AssetRootDirectory = assetRoot;
    if ((assetRoot.length() == 0 && m_CurrentDirectory.length() != 0) ||
        (assetRoot.length() > 0 && (m_CurrentDirectory.length() == 0 || !StartsWithInsensitive(m_CurrentDirectory, assetRoot))))
        m_CurrentDirectory = assetRoot;

    const String filterLower = Rebel::Core::ToLower(String(m_FilterBuffer));
    DirectoryList allDirectories = BuildDirectoryList(allAssets, assetRoot);
    CollectCreatedDirectories(m_CreatedDirectories, assetRoot, allDirectories);
    std::sort(allDirectories.begin(), allDirectories.end(), [](const String& lhs, const String& rhs)
    {
        return std::strcmp(Rebel::Core::ToLower(lhs).c_str(), Rebel::Core::ToLower(rhs).c_str()) < 0;
    });

    DirectoryList folders;
    AssetList files;
    CollectVisibleEntries(registry, allDirectories, m_CurrentDirectory, filterLower, folders, files);

    std::sort(folders.begin(), folders.end(), [](const String& lhs, const String& rhs)
    {
        return std::strcmp(Rebel::Core::ToLower(lhs).c_str(), Rebel::Core::ToLower(rhs).c_str()) < 0;
    });

    std::sort(files.begin(), files.end(), [](const AssetMeta* lhs, const AssetMeta* rhs)
    {
        return std::strcmp(Rebel::Core::ToLower(GetLeafName(lhs->Path)).c_str(), Rebel::Core::ToLower(GetLeafName(rhs->Path)).c_str()) < 0;
    });

    {
        ScopedFont titleFont(GetFont("Bold"));
        ImGui::TextUnformatted("Content Browser");
    }

    ImGui::SameLine();
    ImGui::TextDisabled("%s  %zu items", ICON_FA_BOX_ARCHIVE, static_cast<size_t>(folders.Num() + files.Num()));

    {
        ScopedFont subFont(GetFont("Small"));
        ImGui::TextDisabled("Browse folders on the left, drag assets between directories, and manage files in place.");
    }

    ImGui::SetNextItemWidth(120.0f);
    ImGui::SliderFloat("Thumbnail", &m_ThumbnailSize, 48.0f, 96.0f, "%.0f px");
    ImGui::SameLine();
    DrawSearchField("##ContentBrowserSearch", m_FilterBuffer, IM_ARRAYSIZE(m_FilterBuffer), "Filter folders or assets...");

    TArray<String> breadcrumbParts;
    TArray<String> rootParts;
    if (m_CurrentDirectory.length() > 0)
        breadcrumbParts = Rebel::Core::Split(m_CurrentDirectory, '/');
    if (assetRoot.length() > 0)
        rootParts = Rebel::Core::Split(assetRoot, '/');

    String breadcrumbPath;
    for (size_t i = 0; i < breadcrumbParts.Num(); ++i)
    {
        if (i > 0)
        {
            ImGui::SameLine(0.0f, 4.0f);
            ImGui::TextDisabled("/");
            ImGui::SameLine(0.0f, 4.0f);
        }

        breadcrumbPath = breadcrumbPath.length() == 0 ? breadcrumbParts[i] : breadcrumbPath + "/" + breadcrumbParts[i];
        const bool isCurrent = i == breadcrumbParts.Num() - 1;
        if (DrawBreadcrumbButton(breadcrumbParts[i], isCurrent))
        {
            m_CurrentDirectory = breadcrumbPath;
            m_SelectedEntryPath = breadcrumbPath;
        }

        if (!isCurrent)
            ImGui::SameLine();
    }

    const bool canGoUp = breadcrumbParts.Num() > rootParts.Num();
    if (canGoUp)
    {
        ImGui::SameLine(0.0f, 10.0f);
        if (ImGui::SmallButton((String(ICON_FA_ARROW_LEFT) + " Up").c_str()))
        {
            breadcrumbParts.PopBack();
            m_CurrentDirectory = Rebel::Core::Join(breadcrumbParts, "/");
            m_SelectedEntryPath = m_CurrentDirectory;
        }
    }

    ImGui::Separator();

    const float leftPaneWidth = 260.0f;
    ImGui::BeginChild("ContentBrowserFolders", ImVec2(leftPaneWidth, 0.0f), true);
    {
        ScopedFont treeTitle(GetFont("Small"));
        ImGui::TextDisabled("%s Folders", ICON_FA_FOLDER_TREE);
    }
    ImGui::Separator();

    if (assetRoot.length() == 0)
    {
        DrawEmptyState(ICON_FA_FOLDER_OPEN, "No folders", "Import or create assets to populate the directory tree.");
    }
    else
    {
        DrawDirectoryTreeNode(*this, *assetModule, assetRoot, allDirectories, filterLower);
    }
    DrawFolderTreeBackgroundContextMenu(*this, m_CurrentDirectory.length() > 0 ? m_CurrentDirectory : assetRoot);
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("ContentBrowserGridPane", ImVec2(0.0f, 0.0f), false);
    const float previewSize = m_ThumbnailSize;
    const float tileWidth = previewSize + 36.0f;
    const float tileHeight = previewSize + 60.0f;
    const int columnCount = glm::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / tileWidth));

    if (folders.Num() == 0 && files.Num() == 0)
    {
        if (allAssets.Num() == 0)
            DrawEmptyState(ICON_FA_FOLDER_OPEN, "No assets indexed", "Import or create assets so the browser has content to display.");
        else if (filterLower.length() > 0)
            DrawEmptyState(ICON_FA_FILTER, "No matching assets", "The current filter did not match any folders, asset names, or asset types in this directory.");
        else
            DrawEmptyState(ICON_FA_INBOX, "Folder is empty", "This directory has no direct child folders or assets yet.");
    }
    else
    {
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8.0f, 8.0f));
        if (ImGui::BeginTable("ContentBrowserGrid", columnCount, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_PadOuterX))
        {
            for (const String& folderPath : folders)
            {
                ImGui::TableNextColumn();
                const bool selected = EqualsInsensitive(m_SelectedEntryPath, folderPath);
                if (DrawBrowserTile(folderPath.c_str(), GetBrowserPreviewTexture("hazel/ContentBrowser/Folder.png"), GetLeafName(folderPath), "Folder", selected, tileWidth - 8.0f, tileHeight, previewSize))
                {
                    m_SelectedEntryPath = folderPath;
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        m_CurrentDirectory = folderPath;
                }

                BeginEntryDragSource(BrowserEntryKind::Folder, folderPath, GetLeafName(folderPath));
                HandleDirectoryDropTarget(*this, folderPath);
                DrawFolderContextMenu(*this, folderPath);
            }

            for (const AssetMeta* meta : files)
            {
                ImGui::TableNextColumn();
                const String fileName = GetLeafName(meta->Path);
                const bool selected = EqualsInsensitive(m_SelectedEntryPath, meta->Path);
                const String subtitle = meta->Type ? meta->Type->Name : String("Asset");
                if (DrawBrowserTile(meta->Path.c_str(), GetBrowserPreviewTexture(GetAssetIconPath(*meta)), fileName, subtitle, selected, tileWidth - 8.0f, tileHeight, previewSize, GetAssetIconTint(*meta)))
                {
                    m_SelectedEntryPath = meta->Path;
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        m_PendingOpenAsset = meta->ID;
                }

                BeginEntryDragSource(BrowserEntryKind::Asset, meta->Path, fileName);
                DrawAssetContextMenu(*this, *meta);

                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s\n%s", meta->Path.c_str(), subtitle.c_str());
            }

            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
    }

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::SeparatorText((String(ICON_FA_ARROW_UP_FROM_BRACKET) + " Drop Here To Move Into Current Folder").c_str());
    HandleDirectoryDropTarget(*this, m_CurrentDirectory);
    DrawGridBackgroundContextMenu(*this, m_CurrentDirectory);

    DrawRenameModal(*this, *assetModule);
    DrawDeleteModal(*this, *assetModule);
    ApplyQueuedCreateFolder(*this, *assetModule);
    ApplyQueuedMove(*this, *assetModule);
    ImGui::EndChild();
}

AssetHandle ContentBrowserPanel::ConsumeOpenAssetRequest()
{
    const AssetHandle handle = m_PendingOpenAsset;
    m_PendingOpenAsset = 0;
    return handle;
}
