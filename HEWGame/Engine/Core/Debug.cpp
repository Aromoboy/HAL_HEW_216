#include "Debug.h"

#include <iostream>
#include <mutex>
#include <string>
#include <cstdio>

#if defined(_WIN32)
#include <io.h>
#endif

namespace
{
        std::mutex g_logMutex;
        HWND g_hwnd = nullptr;

        std::string EnsureNewline(const std::string& message)
        {
                if (!message.empty() && message.back() == '\n')
                {
                        return message;
                }
                return message + '\n';
        }

#if defined(_WIN32)
        std::wstring Utf8ToWide(const std::string& message)
        {
                if (message.empty())
                {
                        return std::wstring();
                }

                const int requiredSize = ::MultiByteToWideChar(
                        CP_UTF8,
                        0,
                        message.data(),
                        static_cast<int>(message.size()),
                        nullptr,
                        0);

                if (requiredSize <= 0)
                {
                        return std::wstring(message.begin(), message.end());
                }

                std::wstring wide(static_cast<size_t>(requiredSize), L'\0');
                ::MultiByteToWideChar(
                        CP_UTF8,
                        0,
                        message.data(),
                        static_cast<int>(message.size()),
                        wide.data(),
                        requiredSize);
                return wide;
        }

        bool IsConsoleHandle(HANDLE handle)
        {
                if (handle == nullptr || handle == INVALID_HANDLE_VALUE)
                {
                        return false;
                }

                DWORD mode = 0;
                return ::GetConsoleMode(handle, &mode) != 0;
        }
#endif
}

void DebugLog::Init(HWND hwnd)
{
        g_hwnd = hwnd;
        (void)g_hwnd;
}

void DebugLog::DebugMessage(const std::string& message)
{
        const std::string formatted = EnsureNewline(message);
        std::lock_guard<std::mutex> lock(g_logMutex);

#if defined(_WIN32)
        const std::wstring wide = Utf8ToWide(formatted);
        ::OutputDebugStringW(wide.c_str());
#endif

        bool canWriteToConsole = true;

#if defined(_WIN32)
        HANDLE stdoutHandle = ::GetStdHandle(STD_OUTPUT_HANDLE);
        canWriteToConsole = stdoutHandle != INVALID_HANDLE_VALUE && stdoutHandle != nullptr && _isatty(_fileno(stdout)) != 0;

        if (!canWriteToConsole)
        {
                FILE* newStdout = nullptr;
                if (::AllocConsole() && freopen_s(&newStdout, "CONOUT$", "w", stdout) == 0)
                {
                        stdoutHandle = ::GetStdHandle(STD_OUTPUT_HANDLE);
                        canWriteToConsole = true;
                }
        }
#endif

        if (canWriteToConsole)
        {
#if defined(_WIN32)
                if (IsConsoleHandle(stdoutHandle))
                {
                        DWORD written = 0;
                        if (::WriteConsoleW(stdoutHandle,
                                wide.c_str(),
                                static_cast<DWORD>(wide.size()),
                                &written,
                                nullptr))
                        {
                                return;
                        }
                }
#endif

                std::cout << formatted;
                std::cout.flush();
        }
}
