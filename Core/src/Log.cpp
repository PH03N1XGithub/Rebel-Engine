#include "Core/CorePch.h"
#include "Core/Core.h"

namespace Rebel::Core
{

	LogCategory::LogCategory(const String& name)       
	{
		//auto rawLogger = spdlog::stdout_color_mt(name).get(); // raw spdlog::logger*
		m_Logger = std::make_shared<spdlog::logger>(*spdlog::stdout_color_mt(name.c_str()));
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
