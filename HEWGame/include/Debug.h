#ifndef ___DEBUG_H___
#define ___DEBUG_H___

#include <windows.h>
#include <string>

using namespace std;

void InitDebug(HWND a_hwnd);

void DebugMessage(std::string a_szMessage);



#endif // !___DEBUG_H___
