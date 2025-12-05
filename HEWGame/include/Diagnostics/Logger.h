#pragma once

#include <iostream>
#include <sstream>
#include <string>

#if defined(_WIN32)
#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Diagnostics
{
	inline void LogLine(const std::string& level, const std::string& message)
	{
		std::ostringstream oss;
		oss << '[' << level << "] " << message << '\n';
		const std::string formatted = oss.str();

#if defined(_WIN32)
		::OutputDebugStringA(formatted.c_str());
#endif
		std::clog << formatted;
	}

	inline void LogInfo(const std::string& message)
	{
		LogLine("Info", message);
	}

	inline void LogDebug(const std::string& message)
	{
#if defined(_DEBUG)
		LogLine("Debug", message);
#else
		(void)message;
#endif
	}
}
