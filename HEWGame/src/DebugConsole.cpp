#include "DebugConsole.h"

#include"conioex.h"

HWND DebugConsole::m_hConsole = nullptr;
HWND DebugConsole::m_hMainWindow = nullptr;

void DebugConsole::InitDebugConsole(HWND a_hWindow)
{
    AllocConsole(); // コンソールを新規作成
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout); // 標準出力をコンソールに接続
    freopen_s(&fp, "CONOUT$", "w", stderr); // 標準エラーも同様に

    std::cout << "Debug console initialized!" << std::endl;

    DebugConsole::m_hConsole = GetConsoleWindow();
    DebugConsole::m_hMainWindow = a_hWindow;

    SetWindowPos(m_hConsole, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    ShowWindow(m_hConsole, SW_HIDE);
}


void DebugConsole::SetDrawPos(int x, int y)
{
    gotoxy(x, y);
}

void DebugConsole::ToggleConsole()
{
    static bool visible = true;
    if (visible)
    {
        ShowWindow(m_hConsole, SW_SHOW);
    }
    else
    {
        ShowWindow(m_hConsole, SW_HIDE);
    }

    SetForegroundWindow(m_hMainWindow);

    visible ^= true; // フラグ反転
}
