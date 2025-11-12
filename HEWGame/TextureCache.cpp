// TextureCache.cpp
// DirectXTK (DirectX Tool Kit) を使用出来なかったので、
// 標準の Windows Imaging Component (WIC) を直接使用して
// テクスチャをロードしています。
// そのため、wincodec.h のインクルードと
// windowscodecs.lib のリンカー設定が必要です。

#include "TextureCache.h"
#include <iostream>
#include <vector>
#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")

//============================================================
// WICでテクスチャをロード
//============================================================
bool Texture2D::LoadFromFile(ID3D11Device* device, const std::string& path)
{
    CoInitialize(nullptr);

    // WICファクトリーを作成
    IWICImagingFactory* factory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_IWICImagingFactory, (void**)&factory);
    if (FAILED(hr)) {
        std::cerr << "[Texture] WICファクトリーの作成に失敗" << std::endl;
        return false;
    }

    // ファイルからデコーダーを作成
    std::wstring wpath(path.begin(), path.end());
    IWICBitmapDecoder* decoder = nullptr;
    hr = factory->CreateDecoderFromFilename(wpath.c_str(), nullptr, GENERIC_READ,
        WICDecodeMetadataCacheOnDemand, &decoder);
    if (FAILED(hr)) {
        factory->Release();
        std::cerr << "[Texture] ファイルを開けません: " << path << std::endl;
        return false;
    }

    // 最初のフレームを取得
    IWICBitmapFrameDecode* frame = nullptr;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) {
        decoder->Release();
        factory->Release();
        std::cerr << "[Texture] フレームの取得に失敗" << std::endl;
        return false;
    }

    // RGBA32に変換
    IWICFormatConverter* converter = nullptr;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) {
        frame->Release();
        decoder->Release();
        factory->Release();
        std::cerr << "[Texture] コンバーターの作成に失敗" << std::endl;
        return false;
    }

    hr = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        converter->Release();
        frame->Release();
        decoder->Release();
        factory->Release();
        std::cerr << "[Texture] フォーマット変換に失敗" << std::endl;
        return false;
    }

    // サイズを取得
    UINT width, height;
    converter->GetSize(&width, &height);

    // ピクセルデータを読み込み
    UINT stride = width * 4;
    UINT imageSize = stride * height;
    std::vector<BYTE> pixels(imageSize);
    hr = converter->CopyPixels(nullptr, stride, imageSize, pixels.data());

    // WICリソースを解放
    converter->Release();
    frame->Release();
    decoder->Release();
    factory->Release();

    if (FAILED(hr)) {
        std::cerr << "[Texture] ピクセルデータの読み込みに失敗" << std::endl;
        return false;
    }

    // テクスチャを作成
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = pixels.data();
    initData.SysMemPitch = stride;

    hr = device->CreateTexture2D(&texDesc, &initData, &texture);
    if (FAILED(hr)) {
        std::cerr << "[Texture] テクスチャの作成に失敗" << std::endl;
        return false;
    }

    // シェーダーリソースビューを作成
    hr = device->CreateShaderResourceView(texture, nullptr, &srv);
    if (FAILED(hr)) {
        std::cerr << "[Texture] SRVの作成に失敗" << std::endl;
        return false;
    }

    std::cout << "[Texture] ロード成功: " << path << " (" << width << "x" << height << ")" << std::endl;
    return true;
}

//============================================================
// テクスチャをピクセルシェーダーにバインド
//============================================================
void Texture2D::BindPS(ID3D11DeviceContext* context, UINT slot)
{
    context->PSSetShaderResources(slot, 1, &srv);
}

//============================================================
// テクスチャをロードしてキャッシュに登録
//============================================================
Texture2D& TextureCache::Load(const std::string& path, ID3D11Device* device)
{
    // キャッシュにすでに存在するか確認
    auto it = textures.find(path);
    if (it != textures.end()) {
        std::cout << "[TextureCache] キャッシュされたテクスチャを使用: " << path << std::endl;
        return *it->second;
    }

    // 新しいテクスチャを作成
    auto texture = std::make_unique<Texture2D>();
    if (!texture->LoadFromFile(device, path)) {
        std::cerr << "[TextureCache] テクスチャのロードに失敗しました: " << path << std::endl;
        throw std::runtime_error("テクスチャのロードに失敗: " + path);
    }

    // キャッシュに登録
    Texture2D& ref = *texture;
    textures[path] = std::move(texture);
    std::cout << "[TextureCache] テクスチャをキャッシュに登録: " << path << std::endl;
    return ref;
}

//============================================================
// キャッシュからテクスチャを取得
//============================================================
Texture2D& TextureCache::Get(const std::string& path)
{
    auto it = textures.find(path);
    if (it == textures.end()) {
        std::cerr << "[TextureCache] テクスチャが見つかりません: " << path << std::endl;
        throw std::runtime_error("テクスチャが見つかりません: " + path);
    }

    return *it->second;
}

//============================================================
// TextureCache 使用例
//============================================================
/*
// 前提: ID3D11Device* device, ID3D11DeviceContext* context が作成済み

// 1. テクスチャキャッシュの作成（シングルトン推奨）
TextureCache& textureCache = TextureCache::Instance(); // もしシングルトンなら

// 2. テクスチャのロード
try {
    // 初回：ファイルからロードしてキャッシュに登録
    Texture2D& playerTex = textureCache.Load("textures/player.png", device);

    // 3. シェーダーにバインド
    playerTex.BindPS(context, 0); // ピクセルシェーダーのスロット0に設定

    // 4. キャッシュからの再利用
    // 同じパスで取得するとキャッシュが返される（再ロードしない）
    Texture2D& sameTex = textureCache.Get("textures/player.png"); // キャッシュヒット

    // 5. 別のテクスチャのロード
    Texture2D& groundTex = textureCache.Load("textures/ground.jpg", device);
    groundTex.BindPS(context, 1); // スロット1に設定

} catch (const std::runtime_error& e) {
    std::cerr << "テクスチャロードエラー: " << e.what() << std::endl;
}

// 6. 描画時の使用
void RenderScene(ID3D11DeviceContext* context) {
    // プレイヤー描画
    textureCache.Get("textures/player.png").BindPS(context, 0);
    DrawPlayer();

    // 地面描画
    textureCache.Get("textures/ground.jpg").BindPS(context, 0);
    DrawGround();
}
*/