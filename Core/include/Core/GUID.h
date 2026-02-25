#pragma once
#include "CoreTypes.h"

namespace Rebel::Core
{
	struct GUID
	{
		GUID();
		GUID(uint64 InGUID);
		operator uint64() const { return m_GUID; }
		bool operator==(const GUID& Other) const { return m_GUID == Other.m_GUID; }
	private:
		uint64 m_GUID;
	};

	inline GUID::GUID() : m_GUID(0)
	{
		static thread_local std::mt19937_64 rng{ std::random_device{}() };
		m_GUID = rng();
	}

	inline GUID::GUID(uint64 InGUID) : m_GUID(InGUID)
	{
	}
}
namespace std
{
	template<>
	struct hash<Rebel::Core::GUID>
	{
		size_t operator()(const Rebel::Core::GUID& id) const noexcept
		{
			return std::hash<uint64_t>{}(static_cast<uint64_t>(id));
		}
	};
}