#include "Keyboard.h"
#include "Windows.h"

BYTE g_keyTable[256];
BYTE g_oldkeyTable[256];

void KeyboardUpdate()
{
	memcpy(g_oldkeyTable, g_keyTable, sizeof(g_oldkeyTable));

	GetKeyboardState(g_keyTable);
}

bool IsKeyPress(int nVirtKey)
{
	return g_keyTable[nVirtKey] & 0x80;
}

bool IsKeyTrigger(int nVirtKey)
{
	return (g_keyTable[nVirtKey] ^ g_oldkeyTable[nVirtKey]) & g_keyTable[nVirtKey] & 0x80;
}

bool IsKeyRelease(int nVirtKey)
{
	return (g_keyTable[nVirtKey] ^ g_oldkeyTable[nVirtKey]) & g_oldkeyTable[nVirtKey] & 0x80;
}
