#pragma once
#include "windows.h"
#include "iostream"

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

