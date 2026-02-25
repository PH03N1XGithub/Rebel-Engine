#pragma once
#include "Core/String.h"

namespace Rebel::Core::Reflection
{
	struct TypeInfo;
}

namespace Rebel::Core::Serialization
{
	
enum class ESerializerFormat
{
	YAML,
	Binary
};

class ISerializer
{
public:
	virtual ~ISerializer() = default;

	// Save/load to file
	virtual bool SaveToFile(const String& filePath) const = 0;
	virtual bool LoadFromFile(const String& filePath) = 0;

	// Optional: nested scopes (objects, arrays)
	virtual void BeginObject(const String& key) = 0;
	virtual void EndObject() = 0;

	// New binary serialization
	virtual void Serialize(void* object, const Reflection::TypeInfo* type) = 0;
};
}
