#include "Debug.h"

#include <iostream>
#include <mutex>
#include <string>

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
        ::OutputDebugStringA(formatted.c_str());
#endif
        std::cout << formatted;
        std::cout.flush();
}
