// 画面の全画面切り替えを扱う
// ウィンドウ制御や設定画面から呼ばれる

#include "FullscreenManager.h"

#include "WindowManager.h"

namespace
{
        struct FullscreenState
        {
                HWND hWnd = nullptr;
                bool* canRender = nullptr;
        };

        FullscreenState g_fullscreenState;
}

namespace FullscreenManager
{
        void Initialize(HWND hWnd, bool* canRenderFlag)
        {
                g_fullscreenState.hWnd = hWnd;
                g_fullscreenState.canRender = canRenderFlag;
        }

        bool CanToggle()
        {
                if (!g_fullscreenState.hWnd)
                {
                        return false;
                }

                if (g_fullscreenState.canRender && !(*g_fullscreenState.canRender))
                {
                        return false;
                }

                return !IsIconic(g_fullscreenState.hWnd);
        }

        void Toggle()
        {
                if (!CanToggle())
                {
                        return;
                }

                // WindowManagerが保持するキャッシュされたウィンドウ状態を使用して遷移を実行
                WindowManager::ToggleFullscreen();
        }
}

