#include "WindowManager.h"

#include "DirectX.h"

namespace
{
        struct WindowState
        {
                HWND hWnd = nullptr;
                WindowManager::WindowMode mode = WindowManager::WindowMode::Windowed;
                DWORD savedStyle = 0;
                DWORD savedExStyle = 0;
                RECT windowRect = {};
                HMONITOR monitor = nullptr;
        };

        WindowState g_state;
}

namespace WindowManager
{
        void Initialize(HWND hWnd)
        {
                g_state.hWnd = hWnd;
                g_state.mode = WindowMode::Windowed;
                g_state.savedStyle = static_cast<DWORD>(GetWindowLong(hWnd, GWL_STYLE));
                g_state.savedExStyle = static_cast<DWORD>(GetWindowLong(hWnd, GWL_EXSTYLE));
                GetWindowRect(hWnd, &g_state.windowRect);
                g_state.monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
        }

        void EnterFullscreen()
        {
                if (!g_state.hWnd || g_state.mode == WindowMode::Fullscreen)
                {
                        return;
                }

                g_state.savedStyle = static_cast<DWORD>(GetWindowLong(g_state.hWnd, GWL_STYLE));
                g_state.savedExStyle = static_cast<DWORD>(GetWindowLong(g_state.hWnd, GWL_EXSTYLE));
                GetWindowRect(g_state.hWnd, &g_state.windowRect);

                g_state.monitor = MonitorFromWindow(g_state.hWnd, MONITOR_DEFAULTTONEAREST);
                MONITORINFO monitorInfo = {};
                monitorInfo.cbSize = sizeof(MONITORINFO);

                if (GetMonitorInfo(g_state.monitor, &monitorInfo))
                {
                        SetWindowLong(g_state.hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
                        SetWindowLong(g_state.hWnd, GWL_EXSTYLE, g_state.savedExStyle);
                        SetWindowPos(
                                g_state.hWnd,
                                HWND_TOPMOST,
                                monitorInfo.rcMonitor.left,
                                monitorInfo.rcMonitor.top,
                                monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                                monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                                SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

                        RECT clientRect = {};
                        if (GetClientRect(g_state.hWnd, &clientRect))
                        {
                                const UINT width = static_cast<UINT>(clientRect.right - clientRect.left);
                                const UINT height = static_cast<UINT>(clientRect.bottom - clientRect.top);
                                ResizeDirectX(width, height);
                        }

                        g_state.mode = WindowMode::Fullscreen;
                }
        }

        void ExitFullscreen()
        {
                if (!g_state.hWnd || g_state.mode == WindowMode::Windowed)
                {
                        return;
                }

                SetWindowLong(g_state.hWnd, GWL_STYLE, g_state.savedStyle);
                SetWindowLong(g_state.hWnd, GWL_EXSTYLE, g_state.savedExStyle);

                const bool wasTopMost = (g_state.savedExStyle & WS_EX_TOPMOST) != 0;
                const HWND zOrder = wasTopMost ? HWND_TOPMOST : HWND_NOTOPMOST;
                SetWindowPos(
                        g_state.hWnd,
                        zOrder,
                        g_state.windowRect.left,
                        g_state.windowRect.top,
                        g_state.windowRect.right - g_state.windowRect.left,
                        g_state.windowRect.bottom - g_state.windowRect.top,
                        SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

                ShowWindow(g_state.hWnd, SW_SHOWNORMAL);

                RECT clientRect = {};
                if (GetClientRect(g_state.hWnd, &clientRect))
                {
                        const UINT width = static_cast<UINT>(clientRect.right - clientRect.left);
                        const UINT height = static_cast<UINT>(clientRect.bottom - clientRect.top);
                        ResizeDirectX(width, height);
                }

                g_state.mode = WindowMode::Windowed;
        }

        void ToggleFullscreen()
        {
                if (g_state.mode == WindowMode::Windowed)
                {
                        EnterFullscreen();
                }
                else
                {
                        ExitFullscreen();
                }
        }

        WindowMode GetMode()
        {
                return g_state.mode;
        }
}
