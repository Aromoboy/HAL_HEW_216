#include "TextureDX.h"
#include "Debug.h"
#include <wincodec.h>
#include <combaseapi.h>
#include <shlwapi.h>
#include <vector>
#include <iostream>
#include <cstring>
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "shlwapi.lib")

namespace
{
    class ComInitializer
    {
    public:
        ComInitializer()
        {
            HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            if (result == RPC_E_CHANGED_MODE)
            {
                hr = S_FALSE;
                return;
            }

            hr = result;
            if (SUCCEEDED(hr))
            {
                needsUninit = true;
            }
        }

        ~ComInitializer()
        {
            if (needsUninit)
            {
                CoUninitialize();
            }
        }

        bool IsReady() const { return SUCCEEDED(hr); }

    private:
        HRESULT hr = E_FAIL;
        bool needsUninit = false;
    };

    bool EnsureCOMInitialized()
    {
        thread_local ComInitializer initializer;
        return initializer.IsReady();
    }

    bool CreateTexture(ID3D11Device* device, UINT width, UINT height, const void* data, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& outTextureView)
    {
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

        hr = device->CreateShaderResourceView(texture.Get(), nullptr, outTextureView.GetAddressOf());
        if (FAILED(hr))
        {
            DebugLog::DebugMessage("[Texture2D] SRV̍쐬Ɏs");
            return false;
        }

        return true;
    }

    bool DecodeWICToTexture(ID3D11Device* device, IWICImagingFactory* factory, IStream* stream, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& outTextureView)
    {
        Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
        if (FAILED(factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder)))
        {
            return false;
        }

        Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
        decoder->GetFrame(0, &frame);

        Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
        factory->CreateFormatConverter(&converter);
        converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone, nullptr, 0.0f,
            WICBitmapPaletteTypeCustom);

        UINT width, height;
        converter->GetSize(&width, &height);

        std::vector<BYTE> pixels(width * height * 4);
        converter->CopyPixels(nullptr, width * 4, pixels.size(), pixels.data());

        return CreateTexture(device, width, height, pixels.data(), outTextureView);
    }
}

//============================================================
// WICgpăeNX`t@C[h
//============================================================
bool Texture2D::LoadFromFile(ID3D11Device* device, const std::wstring& path)
{
    if (!EnsureCOMInitialized())
    {
        DebugLog::DebugMessage("[Texture2D] Failed to initialize COM");
        return false;
    }

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
    if (!data || size == 0)
    {
        return false;
    }

    // Raw RGBA data
    if (height != 0 && width != 0)
    {
        return CreateTexture(device, width, height, data, textureView);
    }

    if (!EnsureCOMInitialized())
    {
        DebugLog::DebugMessage("[Texture2D] Failed to initialize COM");
        return false;
    }

    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory))))
    {
        DebugLog::DebugMessage("[Texture2D] WICt@Ng[̍쐬Ɏs");
        return false;
    }

    // Copy compressed payload into an IStream for WIC decoding
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
