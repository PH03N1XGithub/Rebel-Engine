#include "Core/CorePch.h"
#include "Core/Serialization/YamlSerializer.h"

#include "Core/String.h"

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

void YamlSerializer::Write(const String& key, int value)
{
    NodeStack.Back()[key] = value;
}

    void YamlSerializer::Write(const String& key, bool value)
{
    NodeStack.Back()[key] = value;
}

void YamlSerializer::Write(const String& key, float value)
{
    NodeStack.Back()[key] = value;
}

void YamlSerializer::Write(const String& key, const String& value)
{
    NodeStack.Back()[key] = value;
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
        NodeStack.Add(YAML::Node()); // push empty node
}

void YamlSerializer::EndObjectRead()
{
    if (!NodeStack.IsEmpty())
        NodeStack.Back();
}

bool YamlSerializer::Read(const String& key, int& outValue) const
{
    const YAML::Node& node = NodeStack.Back()[key];
    if (node.IsDefined() && node.IsScalar())
    {
        outValue = node.as<int>();
        return true;
    }
    return false;
}
    bool YamlSerializer::Read(const String& key, bool& outValue) const
{
    const YAML::Node& node = NodeStack.Back()[key];
    if (node.IsDefined() && node.IsScalar())
    {
        outValue = node.as<bool>();
        return true;
    }
    return false;
}

bool YamlSerializer::Read(const String& key, float& outValue) const
{
    const YAML::Node& node = NodeStack.Back()[key];
    if (node.IsDefined() && node.IsScalar())
    {
        outValue = node.as<float>();
        return true;
    }
    return false;
}

bool YamlSerializer::Read(const String& key, String& outValue) const
{
    const YAML::Node& node = NodeStack.Back()[key];
    if (node.IsDefined() && node.IsScalar())
    {
        outValue = node.as<String>();
        return true;
    }
    return false;
}

} // namespace Serialization
