#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <cstdio>

#if defined(_WIN32)
#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#endif

namespace Diagnostics
{
	inline bool EnableConsoleOutput = false;

	inline void LogLine(const std::string& level, const std::string& message)
	{
		std::ostringstream oss;
		oss << '[' << level << "] " << message << '\n';
		const std::string formatted = oss.str();

#if defined(_WIN32)
		::OutputDebugStringA(formatted.c_str());
#endif

		bool canWriteToConsole = true;

#if defined(_WIN32)
		HANDLE stderrHandle = ::GetStdHandle(STD_ERROR_HANDLE);
		canWriteToConsole = stderrHandle != INVALID_HANDLE_VALUE && stderrHandle != nullptr && _isatty(_fileno(stderr)) != 0;

	#if defined(_DEBUG)
		if (!canWriteToConsole && EnableConsoleOutput)
		{
			FILE* newStderr = nullptr;
			if (::AllocConsole() && freopen_s(&newStderr, "CONOUT$", "w", stderr) == 0)
			{
				canWriteToConsole = true;
			}
		}
	#endif
#endif

		if (canWriteToConsole)
		{
			std::clog << formatted;
			std::clog.flush();
		}
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
