#pragma once
#include "spdlog/spdlog.h"
#include "CoreTypes.h"
#include "String.h"

namespace Rebel::Core
{
	enum class LogLevel : uint8  
	{
		trace,
		debug,
		info,
		warn,
		error,
		critical
	};
	
	struct LogCategory
	{
		explicit LogCategory(const String& name);

		inline std::shared_ptr<spdlog::logger>& GetLogger() { return m_Logger; }

	private:
		std::shared_ptr<spdlog::logger> m_Logger;
	};
	
}

#define DEFINE_LOG_CATEGORY(CategoryName) LogCategory CategoryName(#CategoryName);  

#define RB_LOG(Category, Level, ...) \
{ \
Rebel::Core::LogLevel logLevel = Rebel::Core::LogLevel::Level; \
Category.GetLogger()->Level(__VA_ARGS__); \
}






