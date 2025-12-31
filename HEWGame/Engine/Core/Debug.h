// エンジンのデバッグ表示を支える機能をまとめている
// 開発中の確認作業から呼ばれる

#ifndef ___DEBUG_H___
#define ___DEBUG_H___

#include <windows.h>
#include <string>

namespace DebugLog
{
    void Init(HWND hwnd);

    void DebugMessage(const std::string& message);
}

[[deprecated("Use DebugLog::Init instead")]] inline void InitDebug(HWND hwnd)
{
    DebugLog::Init(hwnd);
}

[[deprecated("Use DebugLog::DebugMessage instead")]] inline void DebugMessage(const std::string& message)
{
    DebugLog::DebugMessage(message);
}



#endif // !___DEBUG_H___
