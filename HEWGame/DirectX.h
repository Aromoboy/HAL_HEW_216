
#ifndef ___DIRECT_X___
#define ___DIRECT_X___


#include<d3d11.h>// DirectXの機能の利用
#pragma comment(lib, "d3d11.lib")// DirectXの機能の本体

HRESULT InitDirectX(HWND hwnd, UINT width, UINT height, BOOL fullscreen);
void	UninitDirectX();
void	BeginDrawDirectX();
void	EndDrawDirectX();

ID3D11Device* GetDevice();// Deviceの取得
ID3D11DeviceContext* GetContext();// Contextの取得





#endif // !___DIRECT_X___
