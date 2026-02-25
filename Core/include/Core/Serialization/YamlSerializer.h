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
		bool SaveToFile(const String& filename) const override;

		template<typename T>
		void Write(const String& key, const T& value);

		template<typename T>
		bool Read(const String& key, T& out);

		void Serialize(void* object, const Reflection::TypeInfo* type) override{};


		// Loading
		bool LoadFromFile(const String& filename) override;
		void BeginObjectRead(const String& name);
		void EndObjectRead();

		// --- read helpers ---
		void PushNode(const YAML::Node& node) { NodeStack.Add(node); }
		void PopNode() { if (!NodeStack.IsEmpty()) NodeStack.PopBack(); }
		
		template<typename T>
		void Serialize(const T& obj)
		{
			SerializeType( T::StaticType(), &obj);
		}

		const YAML::Node& Current() const { return NodeStack.Back(); }
		YAML::Node CurrentMutable() { return NodeStack.Back(); } // if you need non-const

		// Reflection load
		void DeserializeType(const Reflection::TypeInfo* typeInfo, void* obj);
		void DeserializeTypeRecursive(const Reflection::TypeInfo* typeInfo, void* obj);


		void SerializeType(
			const Reflection::TypeInfo* typeInfo,
			const void* obj);
		void SerializeTypeRecursive(const Reflection::TypeInfo* typeInfo, const void* obj);

		void BeginArray(const String& name)
		{
			YAML::Node arr(YAML::NodeType::Sequence);
			NodeStack.Back()[name] = arr;
			NodeStack.Add(NodeStack.Back()[name]);
		}

		size_t GetArraySize() const
		{
			const YAML::Node& n = NodeStack.Back();
			return n && n.IsSequence() ? n.size() : 0;
		}

		void BeginArrayElementRead(size_t index)
		{
			const YAML::Node& n = NodeStack.Back();
			NodeStack.Add((n && n.IsSequence() && index < n.size()) ? n[index] : YAML::Node());
		}

		void EndArrayElementRead() { PopNode(); }

		// Arrays
		void BeginArrayRead(const String& name)
		{
			const YAML::Node& n = NodeStack.Back()[name];
			NodeStack.Add(n.IsDefined() ? n : YAML::Node());
		}

		void EndArrayRead() { PopNode(); }

		void BeginArrayElement()
		{
			YAML::Node elem;                         // new empty element
			NodeStack.Back().push_back(elem);        // append to sequence

			// Get reference to the LAST element we just pushed
			const YAML::Node& last = NodeStack.Back()[NodeStack.Back().size() - 1];
			NodeStack.Add(last);
		}

		void EndArrayElement()
		{
			NodeStack.PopBack();
		}


		

		template<typename T>
		void Deserialize(YamlSerializer& loader, T& obj)
		{
			const auto* typeInfo = T::StaticType();
		    if (!typeInfo) return;
		
		    loader.BeginObjectRead(typeInfo->Name);
		
		    for (const Reflection::PropertyInfo& prop : typeInfo->Properties)
		    {
		        if (!HasFlag(prop.Flags, Rebel::Core::Reflection::EPropertyFlags::SaveGame))
		            continue;
		
		        char* ptr = reinterpret_cast<char*>(&obj) + prop.Offset;
		
		    	switch (prop.Type)
		    	{
		    	case Reflection::EPropertyType::Int8:
		    		Read(prop.Name, *(int8*)ptr); break;
		    	case Reflection::EPropertyType::UInt8:
		    		Read(prop.Name, *(uint8*)ptr); break;
		    	case Reflection::EPropertyType::Int16:
		    		Read(prop.Name, *(int16*)ptr); break;
		    	case Reflection::EPropertyType::UInt16:
		    		Read(prop.Name, *(uint16*)ptr); break;
		    	case Reflection::EPropertyType::Int32:
		    		Read(prop.Name, *(int32*)ptr); break;
		    	case Reflection::EPropertyType::UInt32:
		    		Read(prop.Name, *(uint32*)ptr); break;
		    	case Reflection::EPropertyType::Int64:
		    		Read(prop.Name, *(int64*)ptr); break;
		    	case Reflection::EPropertyType::UInt64:
		    		Read(prop.Name, *(uint64*)ptr); break;
		    	case Reflection::EPropertyType::Float:
		    		Read(prop.Name, *(Float*)ptr); break;
		    	case Reflection::EPropertyType::Double:
		    		Read(prop.Name, *(Double*)ptr); break;
		    	case Reflection::EPropertyType::Bool:
		    		Read(prop.Name, *(Bool*)ptr); break;
		    	case Reflection::EPropertyType::String:
		    		Read(prop.Name, *(String*)ptr); break;
		    	default:
		    		break;
		    	}
		    }
		
		    loader.EndObjectRead();
		}
		
	private:
		YAML::Node Root;
		Memory::TArray<YAML::Node> NodeStack;
	};

	template <typename T>
	void YamlSerializer::Write(const String& key, const T& value)
	{
		NodeStack.Back()[key] = value;
	}

	template <typename T>
	bool YamlSerializer::Read(const String& key, T& out)
	{
		const YAML::Node& node = NodeStack.Back()[key];
		if (node.IsDefined() && node.IsScalar())
		{
			out = node.as<T>();
			return true;
		}
		return false;
	}


} // namespace Serialization
