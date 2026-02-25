#pragma once
#include <filesystem>

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

	void Init() override
	{
		m_Manager.Clear();
		ScanDirectory();
	}

	void Tick(float) override {}
	void Shutdown() override
	{
		m_Manager.Clear();
		m_Registry.Clear();
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

