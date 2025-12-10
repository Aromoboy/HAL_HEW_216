#ifndef __TEXTURE_LOAD_H__
#define __TEXTURE_LOAD_H__

#include "DirectXTex/DirectXTex.h"

#ifdef _X86_
#ifdef _DEBUG
#pragma comment(lib, "DirectXTex/Win32/Debug/DirectXTex.lib")
#else
#pragma comment(lib, "DirectXTex/Win32/Release/DirectXTex.lib")
#endif
#else
#ifdef _DEBUG
#pragma comment(lib, "DirectXTex/x64/Debug/DirectXTex.lib")
#else
#pragma comment(lib, "DirectXTex/x64/Release/DirectXTex.lib")
#endif
#endif

inline HRESULT LoadTextureFromFile(
	ID3D11Device* pDevice,
	const char* pszFileName, ID3D11ShaderResourceView** ppTexture)
{
	HRESULT hr;
	DirectX::TexMetadata mdata;
	DirectX::ScratchImage image;
	wchar_t wPath[MAX_PATH];
	size_t wLen = 0;

	MultiByteToWideChar(0, 0, pszFileName, -1, wPath, MAX_PATH);
	if (strstr(pszFileName, ".tga"))
	{
		hr = DirectX::LoadFromTGAFile(wPath, &mdata, image);
	}
	else
	{
		hr = DirectX::LoadFromWICFile(wPath, DirectX::WIC_FLAGS::WIC_FLAGS_IGNORE_SRGB, &mdata, image);
	}
	if (FAILED(hr)) {
		return hr;
	}

	hr = CreateShaderResourceView(
		pDevice,
		image.GetImages(), image.GetImageCount(),
		mdata, ppTexture);

	return hr;
}

#endif // __TEXTURE_LOAD_H__