#pragma once
#include "Core/CoreTypes.h"
#include "Core/String.h"
#include "Core/Containers/TArray.h"

class BinaryStream
{
public:
	virtual ~BinaryStream() = default;
	virtual void Write(const void* data, size_t size) = 0;
	virtual void Read(void* data, size_t size) = 0;
	virtual uint64 Tell() const = 0;
	virtual  void Seek(uint64 pos) = 0;

};

DEFINE_LOG_CATEGORY(FileIO)
class FileStream : public BinaryStream
{
	std::FILE* f;
public:
	FileStream(const char* path, const char* mode)
	{
		f = std::fopen(path, mode);
		if (!f)
		{
			RB_LOG(FileIO, error, "Failed to open file: {}", path);
		}
	}
	~FileStream()
	{
		if (f) std::fclose(f);
	}

	void Write(const void* data, size_t size) override
	{
		std::fwrite(data, 1, size, f);
	}
	void Read(void* data, size_t size) override
	{
		std::fread(data, 1, size, f);
	}
	uint64 Tell() const override
	{
		return (uint64)_ftelli64(f);
	}

	void Seek(uint64 pos) override
	{
		_fseeki64(f, (long long)pos, SEEK_SET);
	}

};


class BinaryWriter
{
	BinaryStream& stream;
public:
	BinaryWriter(BinaryStream& s) : stream(s) {}

	template<typename T>
	void Write(const T& v)
	{
		stream.Write(&v, sizeof(T));
	}

	void WriteBytes(const void* data, size_t size)
	{
		stream.Write(data, size);
	}

	uint64 Tell() const {
		return stream.Tell();
	}

	void Seek(uint64 pos) {
		stream.Seek(pos);
	}
};

class BinaryReader
{
	BinaryStream& stream;
public:
	BinaryReader(BinaryStream& s) : stream(s) {}

	template<typename T>
	void Read(T& v)
	{
		stream.Read(&v, sizeof(T));
	}

	void ReadBytes(void* data, size_t size)
	{
		stream.Read(data, size);
	}

	uint64 Tell() const
	{
		return stream.Tell();
	}

	void Seek(uint64 pos)
	{
		stream.Seek(pos);
	}
};

template<typename T>
BinaryWriter& operator<<(BinaryWriter& ar, const T& v)
{
	ar.Write(v);
	return ar;
}

template<typename T>
BinaryReader& operator>>(BinaryReader& ar, T& v)
{
	ar.Read(v);
	return ar;
}

template<typename T>
BinaryWriter& operator<<(BinaryWriter& ar, const TArray<T>& arr)
{
	uint32 count = arr.Num();
	ar << count;
	if (count > 0)
		ar.WriteBytes(arr.Data(), count * sizeof(T));
	return ar;
}

template<typename T>
BinaryReader& operator>>(BinaryReader& ar, TArray<T>& arr)
{
	uint32 count;
	ar >> count;
	arr.Resize(count);
	if (count > 0)
		ar.ReadBytes(arr.Data(), count * sizeof(T));
	return ar;
}

inline BinaryWriter& operator<<(BinaryWriter& ar, const String& s)
{
	uint32 len = (uint32)s.length();
	ar << len;
	ar.WriteBytes(s.c_str(), len);
	return ar;
}

inline BinaryReader& operator>>(BinaryReader& ar, String& s)
{
	uint32 len;
	ar >> len;
	char* buf = new char[len + 1];
	ar.ReadBytes(buf, len);
	buf[len] = 0;
	s = String(buf);
	delete[] buf;
	return ar;
}



