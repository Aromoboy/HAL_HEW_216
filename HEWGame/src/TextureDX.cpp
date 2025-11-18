// ============================================================================
// Tv: TextureDX ̋֘AWbNAsrc ɑ@\̉ĂB
// ړI: ߂ẴW[ɐGJ҂łAӖƈˑ֌Wfcł悤ɍx̐t^ĂB
// Ag: src z̑t@CVXewƘA邽߁At@CԂ̊֌WǐՂۂ̖ڈƂėpłB
// ============================================================================

#include "TextureDX.h"
#include "Debug.h"
#include <wincodec.h>
#include <vector>
#include <iostream>
#pragma comment(lib, "windowscodecs.lib")

//============================================================
// WICを使用してテクスチャファイルをロード
//============================================================
bool Texture2D::LoadFromFile(ID3D11Device* device, const std::wstring& path)
{
    // WICファクトリーを作成
    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory))))
    {
        DebugMessage("[Texture2D] WICファクトリーの作成に失敗");
        return false;
    }

    // ファイルからデコーダーを作成
    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
        WICDecodeMetadataCacheOnLoad, &decoder)))
    {
        DebugMessage("[Texture2D] ファイルを開けません: ");
        return false;
    }

    // 最初のフレームを取得
    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    decoder->GetFrame(0, &frame);

    // RGBA32に変換
    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    factory->CreateFormatConverter(&converter);
    converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone, nullptr, 0.0f,
        WICBitmapPaletteTypeCustom);

    // サイズを取得
    UINT width, height;
    converter->GetSize(&width, &height);

    // ピクセルデータを読み込み
    std::vector<BYTE> pixels(width * height * 4);
    converter->CopyPixels(nullptr, width * 4, pixels.size(), pixels.data());

    // テクスチャを作成
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
    initData.pSysMem = pixels.data();
    initData.SysMemPitch = width * 4;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = device->CreateTexture2D(&desc, &initData, &texture);
    if (FAILED(hr))
    {
        DebugMessage("[Texture2D] テクスチャリソースの作成に失敗");
        return false;
    }

    // シェーダーリソースビューを作成
    hr = device->CreateShaderResourceView(texture.Get(), nullptr, textureView.GetAddressOf());
    if (FAILED(hr))
    {
        DebugMessage("[Texture2D] SRVの作成に失敗");
        return false;
    }

    return true;
}

//============================================================
// 使用例
//============================================================
/*
// テクスチャを作成
Texture2D texture;

// テクスチャファイルをロード
if (texture.LoadFromFile(device, L"textures/player.png"))
{
    // 描画時にテクスチャをバインド
    context->PSSetShaderResources(0, 1, texture.textureView.GetAddressOf());

    // 描画処理...
    context->Draw(...);
}

// 注意：通常はTextureCacheを使用してテクスチャを管理します
// Texture2Dを直接使うのは特別な場合のみです
*/