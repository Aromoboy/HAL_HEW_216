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
    // シングルトンインスタンスを取得
    static TextureCache& Instance()
    {
        static TextureCache instance;
        return instance;
    }

    // コピー・ムーブを禁止
    TextureCache(const TextureCache&) = delete;
    TextureCache& operator=(const TextureCache&) = delete;
    TextureCache(TextureCache&&) = delete;
    TextureCache& operator=(TextureCache&&) = delete;

    Texture2D& Load(const std::string& name, ID3D11Device* device, const std::wstring& path);
    Texture2D& Get(const std::string& name);

private:
    // コンストラクタをprivateに
    TextureCache() = default;
    ~TextureCache() = default;

    std::map<std::string, std::unique_ptr<Texture2D>> textures;
};
#endif