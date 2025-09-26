#pragma once
#include <yaml-cpp/yaml.h>
#include <string>
#include "ISerializer.h"
#include "Core/CoreTypes.h"
#include "Core/Reflection.h"

namespace Rebel::Core::Serialization {

	class YamlSerializer : public ISerializer
	{
	public:
		YamlSerializer() = default;

		// Saving
		void BeginObject(const String& name) override;
		void EndObject() override;
		void Write(const String& key, int value) override;
		void Write(const String& key, bool value) override;
		void Write(const String& key, float value) override;
		void Write(const String& key, const String& value) override;
		bool SaveToFile(const String& filename) const override;

		// Loading
		bool LoadFromFile(const String& filename) override;
		bool Read(const String& key, int& outValue) const override;
		bool Read(const String& key, bool& outValue) const override;
		bool Read(const String& key, float& outValue) const override;
		bool Read(const String& key, String& outValue) const override;
		void BeginObjectRead(const String& name);
		void EndObjectRead();
		
		template<typename T>
		void Serialize(YamlSerializer& serializer, const T& obj)
		{
			const auto* typeInfo = T::StaticType();
		    if (!typeInfo) return; // <- This fixes the nullptr problem
		
		    serializer.BeginObject(typeInfo->Name);
		
		    for (const auto& prop : typeInfo->Properties)
		    {
		        if (!HasFlag(prop.Flags, Rebel::Core::Reflection::EPropertyFlags::SaveGame))
		            continue;
		
		        const char* ptr = reinterpret_cast<const char*>(&obj) + prop.Offset;

		    	//serializer.Write(prop.Name, *(ptr));
		
		        // OLD style type inference based on size
		    	switch (prop.Type)
		    	{
		    	case Rebel::Core::Reflection::EPropertyType::Int:
		    		serializer.Write(prop.Name, *reinterpret_cast<const int*>(ptr));
		    		break;

		    	case Rebel::Core::Reflection::EPropertyType::Float:
		    		serializer.Write(prop.Name, *reinterpret_cast<const float*>(ptr));
		    		break;

		    	case Rebel::Core::Reflection::EPropertyType::Bool:
		    		serializer.Write(prop.Name, *reinterpret_cast<const bool*>(ptr));
		    		break;

		    	case Rebel::Core::Reflection::EPropertyType::String:
		    		serializer.Write(prop.Name, *reinterpret_cast<const String*>(ptr));
		    		break;

		    	default:
		    		// Unknown type, optionally log warning
		    		break;
		    	}
		    }
		
		    serializer.EndObject();
		}

		template<typename T>
		void Deserialize(YamlSerializer& loader, T& obj)
		{
			const auto* typeInfo = T::StaticType();
		    if (!typeInfo) return;
		
		    loader.BeginObjectRead(typeInfo->Name);
		
		    for (const auto& prop : typeInfo->Properties)
		    {
		        if (!HasFlag(prop.Flags, Rebel::Core::Reflection::EPropertyFlags::SaveGame))
		            continue;
		
		        char* ptr = reinterpret_cast<char*>(&obj) + prop.Offset;
		
		    	switch (prop.Type)
		    	{
		    	case Rebel::Core::Reflection::EPropertyType::Int:
		    		loader.Read(prop.Name, *reinterpret_cast<int*>(ptr));
		    		break;

		    	case Rebel::Core::Reflection::EPropertyType::Float:
		    		loader.Read(prop.Name, *reinterpret_cast<float*>(ptr));
		    		break;

		    	case Rebel::Core::Reflection::EPropertyType::Bool:
		    		loader.Read(prop.Name, *reinterpret_cast<bool*>(ptr));
		    		break;

		    	case Rebel::Core::Reflection::EPropertyType::String:
		    		loader.Read(prop.Name, *reinterpret_cast<String*>(ptr));
		    		break;

		    	default:
		    		// Unknown type, optionally log warning
		    		break;
		    	}
		    }
		
		    loader.EndObjectRead();
		}
		
	private:
		YAML::Node Root;
		Memory::TArray<YAML::Node> NodeStack;
	};



} // namespace Serialization
