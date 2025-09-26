#pragma once
#include "Core/String.h"

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

	// Basic key-value serialization
	virtual void Write(const String& key, int value) = 0;
	virtual void Write(const String& key, bool value) = 0;
	virtual void Write(const String& key, float value) = 0;
	virtual void Write(const String& key, const String& value) = 0;

	virtual bool Read(const String& key, int& outValue) const = 0;
	virtual bool Read(const String& key, bool& outValue) const = 0;
	virtual bool Read(const String& key, float& outValue) const = 0;
	virtual bool Read(const String& key, String& outValue) const = 0;

	// Optional: nested scopes (objects, arrays)
	virtual void BeginObject(const String& key) = 0;
	virtual void EndObject() = 0;
};
}
