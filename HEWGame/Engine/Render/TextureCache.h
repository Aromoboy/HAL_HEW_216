// テクスチャ資源を共有する保管庫をまとめている
// 描画初期化や描画処理から参照される

#ifndef __TEXTURECACHE_H__
#define __TEXTURECACHE_H__

#pragma once
#include <d3d11.h>
#include <map>
#include <memory>
#include <string>
#include "TextureDX.h"

class TextureCache {
public:
    // シングルトン取得
    static TextureCache& Instance()
    {
        static TextureCache instance;
        return instance;
    }

    // コピーとムーブを禁止
    TextureCache(const TextureCache&) = delete;
    TextureCache& operator=(const TextureCache&) = delete;
    TextureCache(TextureCache&&) = delete;
    TextureCache& operator=(TextureCache&&) = delete;

    std::shared_ptr<Texture2D> Load(const std::string& name, ID3D11Device* device, const std::wstring& path);
    std::shared_ptr<Texture2D> LoadFromMemory(const std::string& name, ID3D11Device* device, const uint8_t* data, size_t size, unsigned int width = 0, unsigned int height = 0);
    std::shared_ptr<Texture2D> Get(const std::string& name);

private:
    // コンストラクターは private
    TextureCache() = default;
    ~TextureCache() = default;

    std::map<std::string, std::shared_ptr<Texture2D>> textures;
};
#endif
