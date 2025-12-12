// DirectXTK (DirectX Tool Kit) gpoȂ̂ŁA
// W Windows Imaging Component (WIC) 𒼐ڎgp
// eNX`[hĂ܂B
// ̂߁Awincodec.h ̃CN[h
// windowscodecs.lib ̃J[ݒ肪KvłB
#include "TextureCache.h"
#include "Debug.h"
#include "Diagnostics/Logger.h"
#include <iostream>
#include <filesystem>

//============================================================
// eNX`[hăLbVɓo^
//============================================================
std::shared_ptr<Texture2D> TextureCache::Load(const std::string& name, ID3D11Device* device, const std::wstring& path)
{
    const std::string cacheKey = name;

    // LbVɂłɑ݂邩mF
    auto it = textures.find(name);
    if (it != textures.end())
    {
        Diagnostics::LogInfo("[TextureCache] key='" + cacheKey + "' hit");
        return it->second;
    }

    std::error_code ec;
    const bool exists = std::filesystem::exists(std::filesystem::path(path), ec);
    Diagnostics::LogInfo("[TextureCache] key='" + cacheKey + "' miss exists=" + (exists ? "yes" : "no") +
        " path='" + std::string(path.begin(), path.end()) + "'");

    // VeNX`쐬
    auto texture = std::make_shared<Texture2D>();
    if (!texture->LoadFromFile(device, path)) return nullptr;

    // LbVɓo^
    textures[name] = std::move(texture);
    Diagnostics::LogInfo("[TextureCache] key='" + cacheKey + "' stored");
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
// LbVeNX`擾
//============================================================
std::shared_ptr<Texture2D> TextureCache::Get(const std::string& name)
{
    auto it = textures.find(name);

    if (it == textures.end()) return nullptr;

    return it->second;
}
//============================================================
// gp
//============================================================
/*
// VOgȂ̂łǂłANZX\
// FeNX`[hăLbVɓo^
auto& playerTex = TextureCache::Instance().Load("player", device, L"textures/player.png");
auto& enemyTex = TextureCache::Instance().Load("enemy", device, L"textures/enemy.png");
// `掞FLbVeNX`擾Ďgp
auto& tex = TextureCache::Instance().Get("player");  // iLbV擾j
context->PSSetShaderResources(0, 1, tex.textureView.GetAddressOf());
// ܂͒Z
TextureCache::Instance().Get("enemy");
// ӁFOŉxLoad()ĂłAŏ1񂾂[h܂
// 2ڈȍ~̓LbVꂽeNX`Ԃ܂
*/
