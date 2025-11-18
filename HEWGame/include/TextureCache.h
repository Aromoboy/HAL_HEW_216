// ============================================================================
// Tv: TextureCache ɊւC^[tF[Xf[^\錾Ainclude ėpł悤ɂB
// ړI: ߂ẴW[ɐGJ҂łAӖƈˑ֌Wfcł悤ɍx̐t^ĂB
// Ag: include z̑t@CVXewƘA邽߁At@CԂ̊֌WǐՂۂ̖ڈƂėpłB
// ============================================================================

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

    std::shared_ptr<Texture2D> Load(const std::string& name, ID3D11Device* device, const std::wstring& path);
    std::shared_ptr<Texture2D> Get(const std::string& name);

private:
    // コンストラクタをprivateに
    TextureCache() = default;
    ~TextureCache() = default;

    std::map<std::string, std::shared_ptr<Texture2D>> textures;
};
#endif