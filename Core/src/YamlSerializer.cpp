#include "Core/CorePch.h"
#include "Core/Serialization/YamlSerializer.h"

#include "Core/AssetPtrBase.h"
#include "Core/String.h"
#include "Core/Math/CoreMath.h"

namespace Rebel::Core::Serialization {

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
        std::ofstream out(filename.c_str());
        if (!out.is_open())
            return false;
        out << Root;
        return true;
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
        // If you want the same behavior as saving, you can skip Transient here too.
        // if (HasFlag(prop.Flags, Reflection::EPropertyFlags::Transient)) continue;

        char* ptr = reinterpret_cast<char*>(obj) + prop.Offset;

        switch (prop.Type)
        {
        case Reflection::EPropertyType::Int8:   { auto n = node[prop.Name.c_str()]; if(n) *reinterpret_cast<int8*>(ptr)   = n.as<int>(); } break;
        case Reflection::EPropertyType::UInt8:  { auto n = node[prop.Name.c_str()]; if(n) *reinterpret_cast<uint8*>(ptr)  = n.as<unsigned>(); } break;
        case Reflection::EPropertyType::Int16:  { auto n = node[prop.Name.c_str()]; if(n) *reinterpret_cast<int16*>(ptr)  = n.as<int>(); } break;
        case Reflection::EPropertyType::UInt16: { auto n = node[prop.Name.c_str()]; if(n) *reinterpret_cast<uint16*>(ptr) = n.as<unsigned>(); } break;
        case Reflection::EPropertyType::Int32:  { auto n = node[prop.Name.c_str()]; if(n) *reinterpret_cast<int32*>(ptr)  = n.as<int32>(); } break;
        case Reflection::EPropertyType::UInt32: { auto n = node[prop.Name.c_str()]; if(n) *reinterpret_cast<uint32*>(ptr) = n.as<uint32>(); } break;
        case Reflection::EPropertyType::Int64:  { auto n = node[prop.Name.c_str()]; if(n) *reinterpret_cast<int64*>(ptr)  = n.as<int64>(); } break;
        case Reflection::EPropertyType::UInt64: { auto n = node[prop.Name.c_str()]; if(n) *reinterpret_cast<uint64*>(ptr) = n.as<uint64>(); } break;
        case Reflection::EPropertyType::Float:  { auto n = node[prop.Name.c_str()]; if(n) *reinterpret_cast<Float*>(ptr)  = n.as<float>(); } break;
        case Reflection::EPropertyType::Double: { auto n = node[prop.Name.c_str()]; if(n) *reinterpret_cast<Double*>(ptr) = n.as<double>(); } break;
        case Reflection::EPropertyType::Bool:   { auto n = node[prop.Name.c_str()]; if(n) *reinterpret_cast<Bool*>(ptr)   = n.as<bool>(); } break;
        case Reflection::EPropertyType::String: { auto n = node[prop.Name.c_str()]; if(n) *reinterpret_cast<String*>(ptr) = n.as<String>(); } break;

        case Reflection::EPropertyType::Vector3:
        {
            auto n = node[prop.Name.c_str()];
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
            auto n = node[key.c_str()];
            if (n)
            {
                uint64 h = n.as<uint64>();
                auto* ref = reinterpret_cast<AssetPtrBase*>(ptr);
                ref->SetHandle((AssetHandle)h);
            }
            break;
        }

        case Reflection::EPropertyType::MaterialHandle:
        {
            // Your serializer currently writes "MaterialHandle" instead of prop.Name,
            // so read both to be safe.
            /*auto n = node[prop.Name.c_str()];
            if (!n) n = node[prop.Name.c_str()];

            if (n)
            {
                /*auto& mh = *reinterpret_cast<MaterialHandle*>(ptr);
                mh.Id = n.as<uint32>();#1#
            }*/
            break;
        }

        case Reflection::EPropertyType::Unknown:
        default:
            break;
        case Reflection::EPropertyType::Enum:
            {
                auto n = node[prop.Name.c_str()];
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
            /*if (HasFlag(prop.Flags, Reflection::EPropertyFlags::Transient))
                continue;*/
    
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
            case Reflection::EPropertyType::MaterialHandle:
                Write("MaterialHandle",String("0"));
                break;

            case Reflection::EPropertyType::Unknown:
                
                Write("Unknown Type",String("Unserialized"));
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
