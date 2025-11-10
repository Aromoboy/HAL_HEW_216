#include "Debug.h"

HWND g_hwnd;

void InitDebug(HWND a_hwnd)
{
	g_hwnd = a_hwnd;
}

void DebugMessage(std::string a_szMessage)
{
	MessageBox(g_hwnd, a_szMessage.c_str(), "Error!!", MB_OK | MB_ICONERROR);
	
}
