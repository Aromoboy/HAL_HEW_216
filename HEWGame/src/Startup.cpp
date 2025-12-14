#include <windows.h>
#include "Defines.h"
#include "Main.h"
#include <crtdbg.h>
#include <string>
#include "DebugConsole.h"
#include "Diagnostics/Logger.h"
#include "DirectX.h"

// timeGetTime周りの使用
#pragma comment(lib, "winmm.lib")

//--- プロトタイプ宣言
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);


// エントリポイント
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	//--- 変数宣言
	WNDCLASSEX wcex;
	HWND hWnd;
	MSG message;

	// ウィンドクラス情報の設定
	ZeroMemory(&wcex, sizeof(wcex));
	wcex.hInstance = hInstance;
	wcex.lpszClassName = "Class Name";
	wcex.lpfnWndProc = WndProc;
	wcex.style = CS_CLASSDC | CS_DBLCLKS;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wcex.hIconSm = wcex.hIcon;
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);

	// ウィンドウクラス情報の登録
	if (!RegisterClassEx(&wcex))
	{
		MessageBox(NULL, "Failed to RegisterClassEx", "Error", MB_OK);
		return 0;
	}

	// ウィンドウの作成
	RECT rect = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
	DWORD style = WS_CAPTION | WS_SYSMENU;
	DWORD exStyle = WS_EX_OVERLAPPEDWINDOW;
	AdjustWindowRectEx(&rect, style, false, exStyle);
	hWnd = CreateWindowEx(
		exStyle, wcex.lpszClassName,
		APP_TITLE, style,
		CW_USEDEFAULT,CW_USEDEFAULT,
		rect.right - rect.left, rect.bottom - rect.top,
		HWND_DESKTOP,
		NULL, hInstance, NULL
	);


	// デバッグコンソールの表示
#ifdef _DEBUG

	DebugConsole::InitDebugConsole(hWnd);

#endif


	// ウィンドウの表示
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	// 初期化処理
	if (FAILED(Init(hWnd, SCREEN_WIDTH, SCREEN_HEIGHT)))
	{
		Uninit();
		UnregisterClass(wcex.lpszClassName, hInstance);
		return 0;
	}


	//--- FPS制御
	timeBeginPeriod(1);
	DWORD countStartTime = timeGetTime();
	DWORD preExecTime = countStartTime;
	DWORD fpsOldTime = countStartTime;
	float processingTime = 0.0f;
	int fpsCount = 0;

	//--- ウィンドウの管理
	while (1)
	{
		if (PeekMessage(&message, NULL, 0, 0, PM_NOREMOVE))
		{
			if (!GetMessage(&message, NULL, 0, 0))
			{
				break;
			}
			else
			{
				TranslateMessage(&message);
				DispatchMessage(&message);
			}
		}
		else
		{
			DWORD nowTime = timeGetTime();
			float diff = static_cast<float>(nowTime - preExecTime);
			if (diff >= 1000.0f / fFPS)
			{
				

				Update();
				Draw();

				if (nowTime - fpsOldTime >= 1000)
				{
					processingTime = static_cast<float>(timeGetTime() - nowTime);
				}

				
				preExecTime = nowTime;
				fpsCount++;
			}
			if (nowTime - fpsOldTime >= 1000)
			{
				fpsCount = 0;
				fpsOldTime = nowTime;
			}
		}
	}


	// 終了時
	timeEndPeriod(1);
	Uninit();
	UnregisterClass(wcex.lpszClassName, hInstance);

	return 0;
}

// ウィンドウプロシージャ
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
        switch (message)
        {
        case WM_DESTROY:
                PostQuitMessage(0);
                break;
        case WM_SIZE:
        {
                const UINT width = LOWORD(lParam);
                const UINT height = HIWORD(lParam);

                if (wParam == SIZE_MINIMIZED || width == 0 || height == 0)
                {
                        break;
                }

                ResizeDirectX(width, height);
                break;
        }
        }
        return DefWindowProc(hWnd, message, wParam, lParam);
}