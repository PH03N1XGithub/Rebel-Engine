#include "Core/CorePch.h"
#include "Core/Serialization/YamlSerializer.h"

#include "Core/AssetPtrBase.h"
#include "Core/String.h"
#include "Core/Math/CoreMath.h"

#include <sstream>
#include <filesystem>

namespace Rebel::Core::Serialization {

    namespace
    {
        const Reflection::TypeInfo* ReadClassPropertyValue(const void* ptr)
        {
            const Reflection::TypeInfo* type = nullptr;
            if (ptr)
                memcpy(&type, ptr, sizeof(type));
            return type;
        }

        void WriteClassPropertyValue(void* ptr, const Reflection::TypeInfo* type)
        {
            if (!ptr)
                return;

            memcpy(ptr, &type, sizeof(type));
        }

        YAML::Node FindMapValue(const YAML::Node& node, const char* key)
        {
            if (!node || !node.IsMap() || !key)
                return YAML::Node();

            for (const auto& entry : node)
            {
                const YAML::Node& entryKey = entry.first;
                if (!entryKey || !entryKey.IsScalar())
                    continue;

                const std::string currentKey = entryKey.Scalar();
                if (currentKey == key)
                    return entry.second;
            }

            return YAML::Node();
        }

        template<typename TValue>
        bool TryReadScalar(const YAML::Node& node, TValue& outValue)
        {
            if (!node || !node.IsDefined() || !node.IsScalar())
                return false;

            try
            {
                outValue = node.as<TValue>();
                return true;
            }
            catch (const YAML::Exception&)
            {
                return false;
            }
        }
    }

    // -------------------- Saving --------------------
    void YamlSerializer::BeginObject(const String& name)
    {
        YAML::Node node;
        if (NodeStack.IsEmpty())
        {
            Root[name] = node;
            NodeStack.Add(Root[name]);
        }
        else
        {
            NodeStack.Back()[name] = node;
            NodeStack.Add(NodeStack.Back()[name]);
        }
    }
        
    void YamlSerializer::EndObject()
    {
        if (!NodeStack.IsEmpty())
            NodeStack.PopBack();
    }
        
    bool YamlSerializer::SaveToFile(const String& filename) const
    {
        std::error_code ec;
        const std::filesystem::path outPath(filename.c_str());
        if (std::filesystem::exists(outPath, ec))
        {
            std::filesystem::permissions(
                outPath,
                std::filesystem::perms::owner_write |
                    std::filesystem::perms::group_write |
                    std::filesystem::perms::others_write,
                std::filesystem::perm_options::add,
                ec);
        }

        std::ofstream out(filename.c_str());
        if (!out.is_open())
            return false;
        out << Root;
        return true;
    }

    String YamlSerializer::ToString() const
    {
        std::stringstream ss;
        ss << Root;
        return String(ss.str().c_str());
    }

    void YamlSerializer::Reset()
    {
        Root = YAML::Node();
        NodeStack.Clear();
    }
    
    // -------------------- Loading --------------------
    bool YamlSerializer::LoadFromFile(const String& filename)
    {
        try
        {
            Root = YAML::LoadFile(filename.c_str());
            NodeStack.Clear();
            NodeStack.Add(Root); // start at root
            return true;
        }
        catch (const std::exception& e)
        {
            std::cout << "Failed to load YAML: " << e.what() << "\n";
            return false;
        }
    }

    bool YamlSerializer::LoadFromString(const String& yamlText)
    {
        try
        {
            Root = YAML::Load(yamlText.c_str());
            NodeStack.Clear();
            NodeStack.Add(Root);
            return true;
        }
        catch (const std::exception& e)
        {
            std::cout << "Failed to load YAML string: " << e.what() << "\n";
            return false;
        }
    }
    
    void YamlSerializer::BeginObjectRead(const String& name)
    {
        if (NodeStack.IsEmpty())
        {
            // Start at root
            NodeStack.Add(Root);
        }
    
        if (NodeStack.Back()[name].IsDefined())
            NodeStack.Add(NodeStack.Back()[name]);
        else
            NodeStack.Add(YAML::Node()); // push an empty node
    }
    
    void YamlSerializer::EndObjectRead() 
    {
        if (!NodeStack.IsEmpty())
            NodeStack.PopBack();
    }

    void YamlSerializer::DeserializeType(const Reflection::TypeInfo* typeInfo, void* obj)
{
    if (!typeInfo || !obj) return;

    BeginObjectRead(typeInfo->Name);
    DeserializeTypeRecursive(typeInfo, obj);
    EndObjectRead();
}

    inline bool EnumStringToValue(
    const Reflection::EnumInfo& info,
    const char* name,
    int64& outValue)
    {
        for (uint32 i = 0; i < info.Count; ++i)
        {
            if (strcmp(info.MemberNames[i], name) == 0)
            {
                outValue = (int64)i; // assumes sequential enum
                return true;
            }
        }
        return false;
    }


void YamlSerializer::DeserializeTypeRecursive(const Reflection::TypeInfo* typeInfo, void* obj)
{
    if (!typeInfo || !obj) return;

    // base first (same order as SerializeTypeRecursive)
    if (const Reflection::TypeInfo* base = typeInfo->Super)
        DeserializeTypeRecursive(base, obj);

    const YAML::Node& node = NodeStack.Back();
    if (!node || !node.IsMap())
        return;

    for (const auto& prop : typeInfo->Properties)
    {
        if (HasFlag(prop.Flags, Reflection::EPropertyFlags::Transient))
            continue;

        char* ptr = reinterpret_cast<char*>(obj) + prop.Offset;

        switch (prop.Type)
        {
        case Reflection::EPropertyType::Int8:   { auto n = FindMapValue(node, prop.Name.c_str()); int value = 0; if (TryReadScalar(n, value)) *reinterpret_cast<int8*>(ptr) = static_cast<int8>(value); } break;
        case Reflection::EPropertyType::UInt8:  { auto n = FindMapValue(node, prop.Name.c_str()); unsigned value = 0; if (TryReadScalar(n, value)) *reinterpret_cast<uint8*>(ptr) = static_cast<uint8>(value); } break;
        case Reflection::EPropertyType::Int16:  { auto n = FindMapValue(node, prop.Name.c_str()); int value = 0; if (TryReadScalar(n, value)) *reinterpret_cast<int16*>(ptr) = static_cast<int16>(value); } break;
        case Reflection::EPropertyType::UInt16: { auto n = FindMapValue(node, prop.Name.c_str()); unsigned value = 0; if (TryReadScalar(n, value)) *reinterpret_cast<uint16*>(ptr) = static_cast<uint16>(value); } break;
        case Reflection::EPropertyType::Int32:  { auto n = FindMapValue(node, prop.Name.c_str()); int32 value = 0; if (TryReadScalar(n, value)) *reinterpret_cast<int32*>(ptr) = value; } break;
        case Reflection::EPropertyType::UInt32: { auto n = FindMapValue(node, prop.Name.c_str()); uint32 value = 0; if (TryReadScalar(n, value)) *reinterpret_cast<uint32*>(ptr) = value; } break;
        case Reflection::EPropertyType::Int64:  { auto n = FindMapValue(node, prop.Name.c_str()); int64 value = 0; if (TryReadScalar(n, value)) *reinterpret_cast<int64*>(ptr) = value; } break;
        case Reflection::EPropertyType::UInt64: { auto n = FindMapValue(node, prop.Name.c_str()); uint64 value = 0; if (TryReadScalar(n, value)) *reinterpret_cast<uint64*>(ptr) = value; } break;
        case Reflection::EPropertyType::Float:  { auto n = FindMapValue(node, prop.Name.c_str()); Float value = 0.0f; if (TryReadScalar(n, value)) *reinterpret_cast<Float*>(ptr) = value; } break;
        case Reflection::EPropertyType::Double: { auto n = FindMapValue(node, prop.Name.c_str()); Double value = 0.0; if (TryReadScalar(n, value)) *reinterpret_cast<Double*>(ptr) = value; } break;
        case Reflection::EPropertyType::Bool:   { auto n = FindMapValue(node, prop.Name.c_str()); Bool value = false; if (TryReadScalar(n, value)) *reinterpret_cast<Bool*>(ptr) = value; } break;
        case Reflection::EPropertyType::String: { auto n = FindMapValue(node, prop.Name.c_str()); String value; if (TryReadScalar(n, value)) *reinterpret_cast<String*>(ptr) = value; } break;

        case Reflection::EPropertyType::Vector3:
        {
            auto n = FindMapValue(node, prop.Name.c_str());
            if (n && n.IsSequence() && n.size() == 3)
            {
                Vector3& v = *reinterpret_cast<Vector3*>(ptr);
                v.x = n[0].as<float>();
                v.y = n[1].as<float>();
                v.z = n[2].as<float>();
            }
            break;
        }

        case Reflection::EPropertyType::Asset:
        {
            // matches your current serialization key: "<PropName>Asset handle"
            String key = prop.Name;
            auto n = FindMapValue(node, key.c_str());
            uint64 h = 0;
            if (TryReadScalar(n, h))
            {
                auto* ref = reinterpret_cast<AssetPtrBase*>(ptr);
                ref->SetHandle((AssetHandle)h);
            }
            break;
        }

        case Reflection::EPropertyType::Class:
        {
            auto n = FindMapValue(node, prop.Name.c_str());
            String className;
            if (!TryReadScalar(n, className))
                break;

            const Reflection::TypeInfo* selectedType = nullptr;
            if (className.length() > 0)
                selectedType = Reflection::TypeRegistry::Get().GetType(className);

            if (selectedType && prop.SubclassBaseType && !selectedType->IsA(prop.SubclassBaseType))
                break;

            WriteClassPropertyValue(ptr, selectedType);
            break;
        }

        case Reflection::EPropertyType::MaterialHandle:
        {
            auto n = FindMapValue(node, prop.Name.c_str());
            if (!n)
                n = FindMapValue(node, "MaterialHandle");

            uint32 value = 0;
            if (TryReadScalar(n, value))
            {
                *reinterpret_cast<uint32*>(ptr) = value;
            }
            break;
        }

        case Reflection::EPropertyType::Unknown:
        default:
            break;
        case Reflection::EPropertyType::Enum:
            {
                auto n = FindMapValue(node, prop.Name.c_str());
                if (!n || !n.IsScalar() || !prop.Enum)
                    break;

                String enumName = n.as<String>();

                int64 value = 0;
                if (EnumStringToValue(*prop.Enum, enumName.c_str(), value))
                {
                    // Write value back respecting enum storage size
                    memcpy(ptr, &value, prop.Size);
                }
                else
                {
                    // Optional: log warning
                    // RB_LOG_WARN("Unknown enum value '%s' for %s::%s",
                    //     enumName.c_str(), prop.Enum->EnumName, prop.Name.c_str());
                }
                break;
            }
            break;
        }
    }
}

    
    void YamlSerializer::SerializeType(
        const Reflection::TypeInfo* typeInfo,
        const void* obj)
    {
        BeginObject(typeInfo->Name);
    
        SerializeTypeRecursive(typeInfo, obj);
    
        EndObject();
    }

    inline const char* EnumValueToString(
    const Reflection::EnumInfo& info,
    int64 value)
    {
        // assuming enum values are 0..N-1
        if (value >= 0 && value < (int64)info.Count)
            return info.MemberNames[value];

        return "<invalid>";
    }


    void YamlSerializer::SerializeTypeRecursive(
    const Reflection::TypeInfo* typeInfo,
    const void* obj)
    {
        // 1️⃣ Serialize base type FIRST
        if (const Reflection::TypeInfo* base = typeInfo->Super)
        {
            SerializeTypeRecursive(base, obj);
        }
    
        // 2️⃣ Serialize properties declared on THIS type
        for (const auto& prop : typeInfo->Properties)
        {
            if (HasFlag(prop.Flags, Reflection::EPropertyFlags::Transient))
                continue;
    
            const char* ptr = static_cast<const char*>(obj) + prop.Offset;
    
            switch (prop.Type)
            {
            case Reflection::EPropertyType::Int8:
                Write(prop.Name, *reinterpret_cast<const int8*>(ptr)); break;
            case Reflection::EPropertyType::UInt8:
                Write(prop.Name, *reinterpret_cast<const uint8*>(ptr)); break;
            case Reflection::EPropertyType::Int16:
                Write(prop.Name, *reinterpret_cast<const int16*>(ptr)); break;
            case Reflection::EPropertyType::UInt16:
                Write(prop.Name, *reinterpret_cast<const uint16*>(ptr)); break;
            case Reflection::EPropertyType::Int32:
                Write(prop.Name, *reinterpret_cast<const int32*>(ptr)); break;
            case Reflection::EPropertyType::UInt32:
                Write(prop.Name, *reinterpret_cast<const uint32*>(ptr)); break;
            case Reflection::EPropertyType::Int64:
                Write(prop.Name, *reinterpret_cast<const int64*>(ptr)); break;
            case Reflection::EPropertyType::UInt64:
                Write(prop.Name, *reinterpret_cast<const uint64*>(ptr)); break;
            case Reflection::EPropertyType::Float:
                Write(prop.Name, *reinterpret_cast<const Float*>(ptr)); break;
            case Reflection::EPropertyType::Double:
                Write(prop.Name, *reinterpret_cast<const Double*>(ptr)); break;
            case Reflection::EPropertyType::Bool:
                Write(prop.Name, *reinterpret_cast<const Bool*>(ptr)); break;
            case Reflection::EPropertyType::String:
                Write(prop.Name, *reinterpret_cast<const String*>(ptr)); break;
    
            case Reflection::EPropertyType::Vector3:
            {
                const Vector3& v = *reinterpret_cast<const Vector3*>(ptr);
                YAML::Node seq(YAML::NodeType::Sequence);
                seq.push_back(v.x);
                seq.push_back(v.y);
                seq.push_back(v.z);
                seq.SetStyle(YAML::EmitterStyle::Flow);
                NodeStack.Back()[prop.Name] = seq;
                break;
            }
    
            case Reflection::EPropertyType::Asset:
                {
                    const AssetPtrBase* ref = reinterpret_cast<const AssetPtrBase*>(ptr);
                    
                    // TEMP: serialize later or skip
                    Write(prop.Name,static_cast<uint64>(ref->GetHandle()));
                break;
    
                }
            case Reflection::EPropertyType::Class:
            {
                const Reflection::TypeInfo* type = ReadClassPropertyValue(ptr);
                Write(prop.Name, type ? type->Name : String());
                break;
            }
            case Reflection::EPropertyType::MaterialHandle:
            {
                Write(prop.Name, *reinterpret_cast<const uint32*>(ptr));
                break;
            }

            case Reflection::EPropertyType::Unknown:
                break;
            case Reflection::EPropertyType::Enum:
                {
                    // Read enum value from object memory
                    int64 value = 0;
                    memcpy(&value, ptr, prop.Size);

                    // Convert enum -> string
                    const char* name = "<invalid>";
                    if (prop.Enum)
                        name = EnumValueToString(*prop.Enum, value);

                    Write(prop.Name, String(name));
                    break;
                }
            }
        }
    }
} // namespace Serialization
