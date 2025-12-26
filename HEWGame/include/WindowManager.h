#ifndef __WINDOWMANAGER_H__
#define __WINDOWMANAGER_H__

#include <windows.h>

namespace WindowManager
{
        enum class WindowMode
        {
                Windowed,
                Fullscreen,
        };

        void Initialize(HWND hWnd);
        void ToggleFullscreen();
        void EnterFullscreen();
        void ExitFullscreen();
        WindowMode GetMode();
}

#endif
