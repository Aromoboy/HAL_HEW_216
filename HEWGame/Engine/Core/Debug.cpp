// エンジンのデバッグ表示を支える機能をまとめている
// 開発中の確認作業から呼ばれる

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

        bool canWriteToConsole = true;

#if defined(_WIN32)
        HANDLE stdoutHandle = ::GetStdHandle(STD_OUTPUT_HANDLE);
        canWriteToConsole = stdoutHandle != INVALID_HANDLE_VALUE && stdoutHandle != nullptr && _isatty(_fileno(stdout)) != 0;

        if (!canWriteToConsole)
        {
                FILE* newStdout = nullptr;
                if (::AllocConsole() && freopen_s(&newStdout, "CONOUT$", "w", stdout) == 0)
                {
                        canWriteToConsole = true;
                }
        }
#endif

        if (canWriteToConsole)
        {
                std::cout << formatted;
                std::cout.flush();
        }
}
