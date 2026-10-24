#include <windows.h>
#include "Defines.h"
#include "Main.h"
#include <crtdbg.h>
#include <string>
#include <chrono>
#include "DebugConsole.h"
#include "Diagnostics/Logger.h"
#include "DirectX.h"
#include "WindowManager.h"
#include "FullscreenManager.h"

// タイマー関連の使用
#pragma comment(lib, "winmm.lib")

//--- プロトタイプ宣言
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

namespace
{
        bool g_canRender = true;

        void SetCanRender(bool canRender)
        {
                if (canRender && !g_canRender)
                {
                        ResetElapsedTime();
                }
                g_canRender = canRender;
        }
}


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
        wcex.lpszClassName = L"Class Name";
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
                MessageBox(NULL, L"Failed to RegisterClassEx", L"Error", MB_OK);
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

        WindowManager::Initialize(hWnd);
        FullscreenManager::Initialize(hWnd, &g_canRender);


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


	//--- フレームレート制御
    timeBeginPeriod(1);
    using FrameClock = std::chrono::steady_clock;
    const auto frameInterval = std::chrono::duration_cast<FrameClock::duration>(
        std::chrono::duration<double>(1.0 / FPS));
    auto nextFrame = FrameClock::now() + frameInterval;

    while (true)
    {
        if (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT) break;
            TranslateMessage(&message);
            DispatchMessage(&message);
            continue;
        }

        // ミカ：非表示中は入力を待ち、復帰時に古い描画予定を持ち越さない
        if (!g_canRender)
        {
            MsgWaitForMultipleObjectsEx(0, nullptr, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            nextFrame = FrameClock::now() + frameInterval;
            continue;
        }

        const auto now = FrameClock::now();
        if (now < nextFrame)
        {
            // ミカ：次のフレームか入力まで待機し、空回りでCPUを占有しない
            const auto wait = std::chrono::ceil<std::chrono::milliseconds>(nextFrame - now);
            MsgWaitForMultipleObjectsEx(0, nullptr, static_cast<DWORD>(wait.count()),
                QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            continue;
        }

        Update();
        Draw();
        if (IsEnd()) PostMessage(hWnd, WM_CLOSE, 0, 0);

        // ミカ：予定時刻を基準に60Hzを保ち、長い停止後の連続描画は省く
        nextFrame += frameInterval;
        const auto finished = FrameClock::now();
        if (nextFrame < finished) nextFrame = finished;
    }

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
        case WM_ACTIVATEAPP:
                SetCanRender((wParam != FALSE) && !IsIconic(hWnd));
                break;
        case WM_ACTIVATE:
        {
                const WORD state = LOWORD(wParam);
                SetCanRender((state != WA_INACTIVE) && !IsIconic(hWnd));
                break;
        }
        case WM_SIZE:
        {
                const UINT width = LOWORD(lParam);
                const UINT height = HIWORD(lParam);

                if (wParam == SIZE_MINIMIZED || width == 0 || height == 0)
                {
                        SetCanRender(false);
                        break;
                }

                SetCanRender(GetActiveWindow() == hWnd);
                if (!IsDirectXReady())
                {
                        break;
                }
                ResizeDirectX(width, height);
                break;
        }
        case WM_SYSKEYDOWN:
        {
                if (wParam == VK_RETURN && (lParam & (1 << 29)))
                {
                        FullscreenManager::Toggle();
                        return 0;
                }
                break;
        }
        }
        return DefWindowProc(hWnd, message, wParam, lParam);
}
