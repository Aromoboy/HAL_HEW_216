#include "TextureDX.h"
#include "Debug.h"
#include <wincodec.h>
#include <combaseapi.h>
#include <shlwapi.h>
#include <vector>
#include <iostream>
#include <cstring>
#include <limits>
#include <new>
#include <memory>
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "shlwapi.lib")

namespace
{
    bool CreateTexture(ID3D11Device* device, UINT width, UINT height, const void* data, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& outTextureView)
    {
        // ミカ：無効な入力と行サイズのオーバーフローをGPUへ渡さない
        if (!device || !data || width == 0 || height == 0
            || width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION || height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION)
        {
            return false;
        }
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = data;
        initData.SysMemPitch = width * 4;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        HRESULT hr = device->CreateTexture2D(&desc, &initData, &texture);
        if (FAILED(hr))
        {
            DebugLog::DebugMessage("[Texture2D] eNX`\\[X̍쐬Ɏs");
            return false;
        }

        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
        hr = device->CreateShaderResourceView(texture.Get(), nullptr, view.GetAddressOf());
        if (FAILED(hr))
        {
            DebugLog::DebugMessage("[Texture2D] SRV̍쐬Ɏs");
            return false;
        }

        outTextureView = std::move(view);
        return true;
    }

    bool DecodeWICToTexture(ID3D11Device* device, IWICImagingFactory* factory, IStream* stream, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& outTextureView)
    {
        if (!device || !factory || !stream)
        {
            return false;
        }
        Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
        if (FAILED(factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder)))
        {
            return false;
        }

        Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
        // ミカ：各デコード段階の失敗を確認し、未初期化の画像サイズを使わない
        if (FAILED(decoder->GetFrame(0, &frame)))
        {
            return false;
        }

        Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
        if (FAILED(factory->CreateFormatConverter(&converter))
            || FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone, nullptr, 0.0f,
            WICBitmapPaletteTypeCustom)))
        {
            return false;
        }

        UINT width = 0;
        UINT height = 0;
        if (FAILED(converter->GetSize(&width, &height)) || width == 0 || height == 0
            || width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION || height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION)
        {
            return false;
        }

        const size_t stride = static_cast<size_t>(width) * 4;
        const size_t byteCount = stride * height;
        if (byteCount > (std::numeric_limits<UINT>::max)())
        {
            return false;
        }
        std::unique_ptr<BYTE[]> pixels(new (std::nothrow) BYTE[byteCount]);
        if (!pixels || FAILED(converter->CopyPixels(nullptr, static_cast<UINT>(stride),
            static_cast<UINT>(byteCount), pixels.get())))
        {
            return false;
        }

        return CreateTexture(device, width, height, pixels.get(), outTextureView);
    }
}

//============================================================
// WICgpăeNX`t@C[h
//============================================================
bool Texture2D::LoadFromFile(ID3D11Device* device, const std::wstring& path)
{
    // WICt@Ng[쐬
    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory))))
    {
        DebugLog::DebugMessage("[Texture2D] WICt@Ng[̍쐬Ɏs");
        return false;
    }

    Microsoft::WRL::ComPtr<IStream> stream;
    if (FAILED(SHCreateStreamOnFileEx(path.c_str(), STGM_READ | STGM_SHARE_DENY_WRITE, FILE_ATTRIBUTE_NORMAL, FALSE, nullptr, &stream)))
    {
        return false;
    }

    return DecodeWICToTexture(device, factory.Get(), stream.Get(), textureView);
}

bool Texture2D::LoadFromMemory(ID3D11Device* device, const uint8_t* data, size_t size, unsigned int width, unsigned int height)
{
    if (!device || !data || size == 0)
    {
        return false;
    }

    // ミカ：高さゼロはAssimpの圧縮画像として扱い、生のRGBAだけ寸法と長さを確認する
    if (height != 0)
    {
        if (width == 0 || height == 0 || width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION
            || height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION || size < static_cast<size_t>(width) * height * 4)
        {
            return false;
        }
        return CreateTexture(device, width, height, data, textureView);
    }

    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory))))
    {
        DebugLog::DebugMessage("[Texture2D] WICt@Ng[̍쐬Ɏs");
        return false;
    }

    // WIC デコード用に圧縮データを IStream にコピー
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
    if (hMem == nullptr)
    {
        return false;
    }

    void* pMem = GlobalLock(hMem);
    if (pMem == nullptr)
    {
        GlobalFree(hMem);
        return false;
    }
    std::memcpy(pMem, data, size);
    GlobalUnlock(hMem);

    Microsoft::WRL::ComPtr<IStream> stream;
    HRESULT hr = CreateStreamOnHGlobal(hMem, TRUE, &stream);
    if (FAILED(hr))
    {
        GlobalFree(hMem);
        return false;
    }

    return DecodeWICToTexture(device, factory.Get(), stream.Get(), textureView);
}

//============================================================
// gp
//============================================================
/*
// eNX`쐬
Texture2D texture;

// eNX`t@C[h
if (texture.LoadFromFile(device, L"textures/player.png"))
{
    // `掞ɃeNX`oCh
    context->PSSetShaderResources(0, 1, texture.textureView.GetAddressOf());

    // `揈...
    context->Draw(...);
}

// ӁFʏTextureCachegpăeNX`Ǘ܂
// Texture2D𒼐ڎĝ͓ʂȏꍇ݂̂ł
*/
