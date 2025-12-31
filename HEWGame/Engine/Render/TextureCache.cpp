// DirectXTK を使うため
// WIC を経由して読み込む
// テクスチャ読み込み関数を使用
// そのため wincodec.h をインクルード
// windowscodecs.lib をリンカ設定に追加する必要がある
#include "TextureCache.h"
#include "Debug.h"
#include "Diagnostics/Logger.h"
#include <iostream>
#include <filesystem>

//============================================================
// テクスチャをキャッシュへ登録
//============================================================
std::shared_ptr<Texture2D> TextureCache::Load(const std::string& name, ID3D11Device* device, const std::wstring& path)
{
    const std::string cacheKey = name;

    // キャッシュに存在するか確認
    auto it = textures.find(name);
    if (it != textures.end())
    {
        Diagnostics::LogInfo("[TextureCache] key='" + cacheKey + "' ヒット\n");
        return it->second;
    }

    std::error_code ec;
    const bool exists = std::filesystem::exists(std::filesystem::path(path), ec);
    Diagnostics::LogInfo(
        "[TextureCache] key='" + cacheKey + "' ミス 存在=" + (exists ? "有" : "無") +
        " path='" + std::string(path.begin(), path.end()) + "'\n");

    // テクスチャ生成
    auto texture = std::make_shared<Texture2D>();
    if (!texture->LoadFromFile(device, path)) return nullptr;

    // キャッシュへ保存
    textures[name] = std::move(texture);
    Diagnostics::LogInfo("[TextureCache] key='" + cacheKey + "' 格納\n");
    return textures[name];
}

std::shared_ptr<Texture2D> TextureCache::LoadFromMemory(const std::string& name, ID3D11Device* device, const uint8_t* data, size_t size, unsigned int width, unsigned int height)
{
    auto it = textures.find(name);
    if (it != textures.end()) return it->second;

    auto texture = std::make_shared<Texture2D>();
    if (!texture->LoadFromMemory(device, data, size, width, height)) return nullptr;

    textures[name] = std::move(texture);
    return textures[name];
}
//============================================================
// キャッシュ済みテクスチャ取得
//============================================================
std::shared_ptr<Texture2D> TextureCache::Get(const std::string& name)
{
    auto it = textures.find(name);

    if (it == textures.end()) return nullptr;

    return it->second;
}
//============================================================
// 使い方
//============================================================
/*
// シングルトン経由でロードしてキャッシュへ保存
auto& playerTex = TextureCache::Instance().Load("player", device, L"textures/player.png");
auto& enemyTex = TextureCache::Instance().Load("enemy", device, L"textures/enemy.png");
// キャッシュ済みテクスチャを取得して使用
auto& tex = TextureCache::Instance().Get("player");
context->PSSetShaderResources(0, 1, tex.textureView.GetAddressOf());
// もちろん後から Load() を呼んでも最初だけロードされる
TextureCache::Instance().Get("enemy");
// 2回目以降はキャッシュ済みテクスチャを返す
*/
