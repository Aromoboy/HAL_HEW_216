
#include <windows.h>
#include <string>
#include <memory>
#include "DirectX.h"
#include "SpriteDrawer.h"
#include "Game.h"
#include "Keyboard.h"
#include "VertexBuffer.h"

// mm マルチメディア
// lib cppファイルをまとめたようなもの
#pragma comment(lib, "winmm.lib")


LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);


bool bFlag = false;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) 
{

	//終了時のメモリリークチェック
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);


	WNDCLASSEX wcex;
	ZeroMemory(&wcex, sizeof(wcex));//ウィンドウの情報をすべてゼロで埋める
	wcex.hInstance = hInstance;//アプリケーションの識別番号
	wcex.lpszClassName = "ClassName";
	wcex.lpfnWndProc = WndProc;//ウィンドウプロシージャの設定
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wcex.hIconSm = wcex.hIcon;
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);

	
	
	if (!RegisterClassEx(&wcex))
	{
		MessageBox(NULL, "ウィンドウクラスの登録に失敗", "エラー", MB_OK);
	}

	HWND hwnd;

	
	
	hwnd = CreateWindowEx(
		WS_EX_OVERLAPPEDWINDOW,
		wcex.lpszClassName, "縦横無尽に跳ね回るウィンドウ",
		WS_CAPTION | WS_SYSMENU,
		CW_USEDEFAULT, CW_USEDEFAULT,
		WINX, WINY,
		HWND_DESKTOP, NULL, hInstance, NULL);
	if (hwnd == nullptr)
	{
		MessageBox(NULL, "ウィンドウの作成に失敗", "エラー", MB_OK);
	}


	if (hwnd == NULL)//作成されなかったら終了する
	{
		return 0;
	}
	else
	{

			ShowWindow(hwnd, nCmdShow);
			UpdateWindow(hwnd);
		
	}
	

	DWORD  nowTime = 0;
	DWORD oldTime = 0;
	DWORD fpsCount = 0;
	DWORD fpsOldTime = 0;
	std::string txtFps;

	timeBeginPeriod(1);


	//メッセージキューにメッセージがあるか
	MSG message;

	if (FAILED(InitDirectX(hwnd, WINX, WINY, false)))
	{
		MessageBox(hwnd, "DirectXの初期化に失敗", "エラー", MB_OK);
		return 0;
	}

	

	InitSpriteDrawer(GetDevice(), GetContext(), WINX, WINY);

	InitVertex();

	std::unique_ptr<Game> pMainGame = std::make_unique<Game>();



	int nMoveCnt = 0;
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
			// FPSの処理
			nowTime = timeGetTime();
			
			if (nowTime - oldTime >= 1000 / 60)
			{
				

				KeyboardUpdate();
				pMainGame->UpdateGame();
				BeginDrawDirectX();
				pMainGame->DrawGame();
				EndDrawDirectX();
				oldTime = nowTime;

				
				
				fpsCount++;
			}

			if (nowTime - fpsOldTime >= 1000)
			{
#ifdef _DEBUG
				std::string txtFPS =
					std::to_string(fpsCount);
				txtFps = "FPS:" + txtFps;
				SetWindowText(hwnd, txtFPS.c_str());
#endif
				fpsCount = 0;
				fpsOldTime = nowTime;
			}
		}
	}


	timeEndPeriod(1);
	UninitVertex();
	UninitSpriteDrawer();
	UninitDirectX();
	return 0;

}


LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{

	int id;
	static HRGN hRgn1, hRgn2, hRgn3, hRgn, hRgnX;
	WINDOWPLACEMENT wndpl;
	RECT rc;
	static int def; //メニュー、タイトルバーの高さ
	POINT pta[3], ptb[3];



	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	

	}

	return DefWindowProc(hwnd, message, wParam, lParam);

}




