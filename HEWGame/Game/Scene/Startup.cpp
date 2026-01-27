#include <windows.h>

#include "ConfigSectionReader.h"
#include "Defines.h"
#include "Main.h"
#include "DebugConsole.h"
#include "Diagnostics/Logger.h"
#include "DirectX.h"
#include "WindowManager.h"
#include "FullscreenManager.h"

#include <crtdbg.h>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

// タイマー関連の使用
#pragma comment(lib, "winmm.lib")

//--- プロトタイプ宣言
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

namespace
{
        bool g_canRender = true;
        constexpr const char* kWindowConfigPath = "Assets/Config/technical.cfg";
        constexpr const char* kWindowConfigSection = "window.settings";

        bool TryParseUnsigned(const std::string& value, UINT& out)
        {
                const char* start = value.c_str();
                if (*start == '\0')
                {
                        return false;
                }

                char* end = nullptr;
                unsigned long parsed = std::strtoul(start, &end, 10);
                if (end == start)
                {
                        return false;
                }

                while (*end != '\0' && std::isspace(static_cast<unsigned char>(*end)))
                {
                        ++end;
                }

                if (*end != '\0')
                {
                        return false;
                }

                if (parsed == 0 || parsed > std::numeric_limits<UINT>::max())
                {
                        return false;
                }

                out = static_cast<UINT>(parsed);
                return true;
        }

        bool TryGetConfigWindowSize(UINT& width, UINT& height)
        {
                std::vector<Config::SectionLine> lines;
                if (!Config::ReadSectionLines(kWindowConfigPath, kWindowConfigSection, lines))
                {
                        return false;
                }

                bool widthSet = false;
                bool heightSet = false;

                for (const auto& entry : lines)
                {
                        std::string key = Config::ToLowerCopy(Config::Trim(entry.key));
                        std::string value = Config::Trim(entry.value);

                        if (key == "width")
                        {
                                widthSet = TryParseUnsigned(value, width);
                        }
                        else if (key == "height")
                        {
                                heightSet = TryParseUnsigned(value, height);
                        }
                }

                return widthSet && heightSet;
        }

        void ResolveInitialWindowSize(UINT& width, UINT& height)
        {
                if (TryGetConfigWindowSize(width, height))
                {
                        return;
                }

                const int workAreaWidth = GetSystemMetrics(SM_CXWORKAREA);
                const int workAreaHeight = GetSystemMetrics(SM_CYWORKAREA);

                if (workAreaWidth > 0 && workAreaHeight > 0)
                {
                        width = static_cast<UINT>(workAreaWidth);
                        height = static_cast<UINT>(workAreaHeight);
                        return;
                }

                width = SCREEN_WIDTH;
                height = SCREEN_HEIGHT;
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

        UINT initialWidth = 0;
        UINT initialHeight = 0;
        ResolveInitialWindowSize(initialWidth, initialHeight);

	// ウィンドウの作成
	RECT rect = {0, 0, static_cast<LONG>(initialWidth), static_cast<LONG>(initialHeight)};
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
	if (FAILED(Init(hWnd, initialWidth, initialHeight)))
	{
		Uninit();
		UnregisterClass(wcex.lpszClassName, hInstance);
		return 0;
	}


	//--- フレームレート制御
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
                                if (g_canRender)
                                {
                                        Update();
                                        Draw();

                                        if (nowTime - fpsOldTime >= 1000)
                                        {
                                                processingTime = static_cast<float>(timeGetTime() - nowTime);
                                        }

                                        fpsCount++;
                                }

                                preExecTime = nowTime;
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
        case WM_ACTIVATEAPP:
                g_canRender = (wParam != FALSE) && !IsIconic(hWnd);
                break;
        case WM_ACTIVATE:
        {
                const WORD state = LOWORD(wParam);
                g_canRender = (state != WA_INACTIVE) && !IsIconic(hWnd);
                break;
        }
        case WM_SIZE:
        {
                const UINT width = LOWORD(lParam);
                const UINT height = HIWORD(lParam);

                if (wParam == SIZE_MINIMIZED || width == 0 || height == 0)
                {
                        g_canRender = false;
                        break;
                }

                g_canRender = (GetActiveWindow() == hWnd);
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
