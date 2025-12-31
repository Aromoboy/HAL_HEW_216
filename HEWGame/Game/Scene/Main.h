// ゲームの起動から終了までの流れをまとめている
// アプリの初期化とシーン管理がここから動く

#ifndef __MAIN_H__
#define __MAIN_H__

#include <Windows.h>

HRESULT Init(HWND hWnd, UINT width, UINT height);
void Uninit();
void Update();
void Draw();
HRESULT GetComInitResult();

#endif // __MAIN_H__
