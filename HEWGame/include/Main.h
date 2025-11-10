#ifndef __MAIN_H__
#define __MAIN_H__

#include <Windows.h>

HRESULT Init(HWND hWnd, UINT width, UINT height);
void Uninit();
void Update();
void Draw();

#endif // __MAIN_H__