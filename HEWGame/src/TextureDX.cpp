#include "TextureDX.h"
#include "Debug.h"
#include <wincodec.h>
#include <combaseapi.h>
#include <shlwapi.h>
#include <vector>
#include <iostream>
#include <cstring>
#include <sstream>
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "shlwapi.lib")

namespace
{
    std::string FormatHR(const char* message, HRESULT hr)
    {
        std::ostringstream oss;
        oss << message << " hr=0x" << std::hex << static_cast<unsigned long>(hr);
        return oss.str();
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
            DebugMessage(FormatHR("[Texture2D] eNX`\\[X̍쐬Ɏs", hr));
            return false;
        }

        hr = device->CreateShaderResourceView(texture.Get(), nullptr, outTextureView.GetAddressOf());
        if (FAILED(hr))
        {
            DebugMessage(FormatHR("[Texture2D] SRV̍쐬Ɏs", hr));
            return false;
        }

        return true;
    }

    bool DecodeWICToTexture(ID3D11Device* device, IWICImagingFactory* factory, IStream* stream, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& outTextureView)
    {
        Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
        HRESULT hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
        if (FAILED(hr))
        {
            DebugMessage(FormatHR("[Texture2D] Failed to create decoder from stream", hr));
            return false;
        }

        Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
        hr = decoder->GetFrame(0, &frame);
        if (FAILED(hr))
        {
            DebugMessage(FormatHR("[Texture2D] Failed to get frame", hr));
            return false;
        }

        Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
        hr = factory->CreateFormatConverter(&converter);
        if (FAILED(hr))
        {
            DebugMessage(FormatHR("[Texture2D] Failed to create format converter", hr));
            return false;
        }
        hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone, nullptr, 0.0f,
            WICBitmapPaletteTypeCustom);
        if (FAILED(hr))
        {
            DebugMessage(FormatHR("[Texture2D] Failed to initialize format converter", hr));
            return false;
        }

        UINT width, height;
        hr = converter->GetSize(&width, &height);
        if (FAILED(hr))
        {
            DebugMessage(FormatHR("[Texture2D] Failed to get image size", hr));
            return false;
        }

        std::vector<BYTE> pixels(width * height * 4);
        hr = converter->CopyPixels(nullptr, width * 4, pixels.size(), pixels.data());
        if (FAILED(hr))
        {
            DebugMessage(FormatHR("[Texture2D] Failed to copy pixels", hr));
            return false;
        }

        return CreateTexture(device, width, height, pixels.data(), outTextureView);
    }
}

bool Texture2D::LoadFromFile(ID3D11Device* device, const std::wstring& path)
{
    std::string pathStr(path.begin(), path.end());

    // Create WIC factory
    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory)
    );

    if (FAILED(hr))
    {
        DebugMessage(FormatHR("[Texture2D] Failed to create WIC factory", hr));
        DebugMessage("[TextureDX] Failed to decode texture: " + pathStr);
        return false;
    }

    // Create file stream
    Microsoft::WRL::ComPtr<IStream> stream;
    hr = SHCreateStreamOnFileEx(
        path.c_str(),
        STGM_READ | STGM_SHARE_DENY_WRITE,
        FILE_ATTRIBUTE_NORMAL,
        FALSE,
        nullptr,
        &stream
    );

    if (FAILED(hr))
    {
        DebugMessage(FormatHR("[Texture2D] Failed to create file stream", hr));
        DebugMessage("[TextureDX] Failed to decode texture: " + pathStr);
        return false;
    }

    bool result = DecodeWICToTexture(device, factory.Get(), stream.Get(), textureView);
    if (!result)
    {
        DebugMessage("[TextureDX] Failed to decode texture: " + pathStr);
    }

    return result;
}


bool Texture2D::LoadFromMemory(
    ID3D11Device* device,
    const uint8_t* data,
    size_t size,
    unsigned int width,
    unsigned int height,
    const char* formatHint)
{
    if (!data || size == 0)
    {
        DebugMessage("[TextureDX] Failed to decode texture: size=" + std::to_string(size));
        return false;
    }

    // Raw RGBA data embedded inside FBX
    if (height != 0 && width != 0)
    {
        const auto expectedSize = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
        if (size < expectedSize)
        {
            DebugMessage(
                "[TextureDX] Raw texture data too small: expected "
                + std::to_string(expectedSize)
                + " bytes but got "
                + std::to_string(size)
            );
            return false;
        }
        return CreateTexture(device, width, height, data, textureView);
    }

    const std::string hint = formatHint ? formatHint : "";

    // Create WIC factory
    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory)
    );

    if (FAILED(hr))
    {
        DebugMessage(FormatHR("[Texture2D] Failed to create WIC factory", hr));
        DebugMessage("[TextureDX] Failed to decode texture: size=" + std::to_string(size));
        return false;
    }

    // Allocate memory
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!hMem)
    {
        DebugMessage("[TextureDX] Failed to allocate memory for texture decode");
        return false;
    }

    struct HGlobalReleaser
    {
        HGLOBAL handle;
        bool release;
        ~HGlobalReleaser() { if (release && handle) GlobalFree(handle); }
    } hMemReleaser{ hMem, true };

    void* pMem = GlobalLock(hMem);
    if (!pMem)
    {
        DebugMessage("[TextureDX] Failed to lock HGLOBAL");
        return false;
    }

    memcpy(pMem, data, size);
    GlobalUnlock(hMem);

    // Convert HGLOBAL to IStream
    Microsoft::WRL::ComPtr<IStream> stream;
    hr = CreateStreamOnHGlobal(hMem, TRUE, &stream);

    if (FAILED(hr))
    {
        DebugMessage(FormatHR("[Texture2D] Failed to create stream from HGLOBAL", hr));
        return false;
    }

    // ownership handed over to IStream
    hMemReleaser.release = false;

    bool result = DecodeWICToTexture(device, factory.Get(), stream.Get(), textureView);
    if (!result)
    {
        DebugMessage(
            "[TextureDX] Failed to decode texture: size="
            + std::to_string(size)
            + (hint.empty() ? "" : ", hint=" + hint)
        );
    }

    return result;
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
