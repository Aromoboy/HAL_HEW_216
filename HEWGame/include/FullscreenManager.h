#pragma once

#include <Windows.h>

namespace FullscreenManager
{
    // Initialize with the window handle and a pointer to the render availability flag.
    // The flag must remain valid for the application's lifetime.
    void Initialize(HWND hWnd, bool* canRenderFlag);

    // Returns whether toggling is safe (window exists, not minimized, renderable).
    bool CanToggle();

    // Toggle fullscreen while respecting cached window state and render availability.
    void Toggle();
}

