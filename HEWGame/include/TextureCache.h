#ifndef __TEXTURECACHE_H__
#define __TEXTURECACHE_H__

#pragma once
#include <d3d11.h>
#include <map>
#include <memory>
#include <set>
#include <string>
#include "TextureDX.h"

class TextureCache {
public:
    // VOgCX^X擾
    static TextureCache& Instance()
    {
        static TextureCache instance;
        return instance;
    }

    // Rs[E[u֎~
    TextureCache(const TextureCache&) = delete;
    TextureCache& operator=(const TextureCache&) = delete;
    TextureCache(TextureCache&&) = delete;
    TextureCache& operator=(TextureCache&&) = delete;

    static std::string NormalizeKey(std::string key);
    static std::string MakeFileKey(const std::string& fullPath);
    static std::string MakeFbxEmbeddedKey(const std::string& fbxPath, int index);

    std::shared_ptr<Texture2D> Load(const std::string& name, ID3D11Device* device, const std::wstring& path);
    std::shared_ptr<Texture2D> LoadFromMemory(const std::string& name, ID3D11Device* device, const uint8_t* data, size_t size, unsigned int width = 0, unsigned int height = 0, const char* formatHint = nullptr);
    std::shared_ptr<Texture2D> GetTexture(const std::string& name);
    ID3D11ShaderResourceView* Get(const std::string& name);

private:
    // RXgN^private
    TextureCache() = default;
    ~TextureCache() = default;

    std::map<std::string, std::shared_ptr<Texture2D>> textures;
#ifdef _DEBUG
    std::set<std::string> missingLogged;
#endif
};
#endif
