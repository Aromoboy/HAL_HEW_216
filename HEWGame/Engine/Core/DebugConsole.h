// デバッグ用のコンソール表示を扱う
// 開発中の入力確認や表示から使われる

#pragma once
#include "windows.h"

class DebugConsole
{
public:
	static void InitDebugConsole(HWND a_hWindow);

	static void SetDrawPos(int x, int y);

	static void ToggleConsole();

private:
	static HWND m_hConsole;
	static HWND m_hMainWindow;
};

