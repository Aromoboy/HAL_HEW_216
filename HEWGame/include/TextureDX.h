#ifndef __TEXTUREDX_H__
#define __TEXTUREDX_H__

#include <string>
#include <d3d11.h>
#include <wrl/client.h>

class Texture2D
{
public:
    bool LoadFromFile(ID3D11Device* device, const std::wstring& path);
    bool LoadFromMemory(ID3D11Device* device, const uint8_t* data, size_t size, unsigned int width = 0, unsigned int height = 0, const char* formatHint = nullptr);
    ID3D11ShaderResourceView* Get() const { return textureView.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> textureView;
};
#endif