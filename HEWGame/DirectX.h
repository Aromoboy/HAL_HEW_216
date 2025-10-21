#ifndef ___DIRECT_X___
#define ___DIRECT_X___

#include <Windows.h>
#include <d3d11.h>

#pragma comment(lib, "d3d11.lib")

HRESULT InitDirectX(HWND hwnd, UINT width, UINT height, BOOL fullscreen);
void    UninitDirectX();
void    BeginDrawDirectX();
void    EndDrawDirectX();

ID3D11Device* GetDevice();
ID3D11DeviceContext* GetContext();
HWND GetGameWindowHandle();

#endif // !___DIRECT_X___
