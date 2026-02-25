#include "Core/CorePch.h"
#include "Core/Core.h"

namespace Rebel::Core
{

	LogCategory::LogCategory(const String& name)       
	{
		auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
		auto engineSink  = std::make_shared<EngineSink_mt>();

		std::vector<spdlog::sink_ptr> sinks { consoleSink, engineSink };
		
		m_Logger = std::make_shared<spdlog::logger>(name.c_str(), sinks.begin(), sinks.end());
		/**
		 * @param %^ color start
		 * @param %T - timestamp
		 * @param %n - logger name(category)
		 * @param %l - log level
		 * @param %v - msg
		 */
		m_Logger->set_pattern("%^[%T] [%n] [%l%$] %v%$");
		// Allow all log levels to appear
		m_Logger->set_level(spdlog::level::trace);  // <-- important TODO: implement level of importance for log

	}
}
