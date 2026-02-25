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

#define DEFINE_LOG_CATEGORY(CategoryName) inline Rebel::Core::LogCategory CategoryName(#CategoryName);  

#define RB_LOG(Category, Level, ...) \
{ \
Rebel::Core::LogLevel logLevel = Rebel::Core::LogLevel::Level; \
Category.GetLogger()->Level(__VA_ARGS__); \
}

#include <unordered_map>

struct ConsoleState
{
	bool AutoScroll = true;
	bool ShowTime = true;
	int MinLogLevel = 0;
	char SearchBuffer[256] = "";

	std::unordered_map<std::string, bool> CategoryFilter;
};


inline ConsoleState GConsoleState;

struct EngineLogEntry
{
	std::string Category;
	spdlog::level::level_enum Level;
	std::string Message;
};

class EngineLogBuffer
{
public:
	void Add(const EngineLogEntry& entry)
	{
		std::lock_guard<std::mutex> lock(m_Mutex);

		if (m_Entries.size() > 2000)
			m_Entries.erase(m_Entries.begin());

		m_Entries.push_back(entry);

		// Auto-register category if missing
		if (GConsoleState.CategoryFilter.find(entry.Category) == GConsoleState.CategoryFilter.end())
			GConsoleState.CategoryFilter[entry.Category] = true;
	}


	const std::vector<EngineLogEntry>& Get() const { return m_Entries; }

	void Clear()
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		m_Entries.clear();
	}
	
	

private:
	std::vector<EngineLogEntry> m_Entries;
	std::mutex m_Mutex;
};

inline EngineLogBuffer GEngineLogBuffer;


#include "spdlog/sinks/base_sink.h"

template<typename Mutex>
class EngineSink : public spdlog::sinks::base_sink<Mutex>
{
protected:
	void sink_it_(const spdlog::details::log_msg& msg) override
	{
		spdlog::memory_buf_t formatted;
		spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);

		EngineLogEntry entry;
		entry.Category = std::string(msg.logger_name.data(), msg.logger_name.size());
		entry.Level = msg.level;
		entry.Message = std::string(formatted.data(), formatted.size());

		GEngineLogBuffer.Add(entry);
	}

	void flush_() override {}
};

using EngineSink_mt = EngineSink<std::mutex>;












