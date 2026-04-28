#pragma once
#include <filesystem>

#include "AssetFileHeader.h"
#include "AssetManager.h"

class AssetManagerModule : public IModule
{
	REFLECTABLE_CLASS(AssetManagerModule, IModule)
public:
	AssetManagerModule() : m_Manager(m_Registry)
	{
		
	};

	AssetRegistry& GetRegistry() { return m_Registry; }
	AssetManager&  GetManager()  { return m_Manager; }
	void RescanAssets()
	{
		m_Manager.Clear();
		ScanDirectory();
	}

	void Init() override
	{
		RescanAssets();
	}

	void Tick(float) override {}
	void Shutdown() override
	{
		m_Manager.Clear();
		m_Registry.Clear();
	}

	template<typename TAsset>
	bool SaveAssetToFile(const String& filePath, TAsset& asset)
	{
		static_assert(std::is_base_of_v<Asset, TAsset>);

		std::filesystem::path outputPath(filePath.c_str());
		if (outputPath.extension() != ".rasset")
			outputPath.replace_extension(".rasset");

		std::filesystem::create_directories(outputPath.parent_path());

		if (!IsValidAssetHandle(asset.ID))
			asset.ID = AssetHandle(Rebel::Core::GUID());

		std::filesystem::path noExtPath = outputPath;
		noExtPath.replace_extension();
		asset.Path = String(noExtPath.generic_string().c_str());

		FileStream fs(outputPath.string().c_str(), "wb");
		if (!fs.IsOpen())
			return false;

		BinaryWriter ar(fs);

		AssetFileHeader header{};
		header.AssetID = (uint64)asset.ID;
		header.TypeHash = Rebel::Core::Reflection::TypeHash(TAsset::StaticType()->Name.c_str());
		header.Version = asset.SerializedVersion;

		ar.Write(header);
        header.PayloadOffset = ar.Tell();

        asset.Serialize(ar);
        header.Version = asset.SerializedVersion;

        ar.Seek(0);
        ar.Write(header);

		ScanDirectory();
		return true;
	}

	bool SaveAssetHeader(const Asset& asset)
	{
		YAML::Node root;

		// --- Asset identity ---
		root["Asset"]["ID"]    = (uint64)asset.ID;
		root["Asset"]["Class"] = asset.GetType()->Name;

		// File path comes from the asset itself
		String path = asset.Path + "TestAsset"+".rasset";

		// Make sure folder exists
		std::filesystem::create_directories(std::filesystem::path(path.c_str()).parent_path());

		String filename = path.c_str();

		std::ofstream out(filename.c_str());
		if (!out.is_open())
			return false;
		out << root;
		return true;
	}


private:
	void ScanDirectory(); // fills registry only

private:

	AssetRegistry m_Registry;
	AssetManager  m_Manager;
};
REFLECT_CLASS(AssetManagerModule, IModule)
END_REFLECT_CLASS(AssetManagerModule)

