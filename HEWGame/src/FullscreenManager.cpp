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

                // Use the cached window state held by WindowManager to perform the transition.
                WindowManager::ToggleFullscreen();
        }
}

