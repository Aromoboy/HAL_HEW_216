// DirectXTK (DirectX Tool Kit) gpoȂ̂ŁA
// W Windows Imaging Component (WIC) 𒼐ڎgp
// eNX`[hĂ܂B
// ̂߁Awincodec.h ̃CN[h
// windowscodecs.lib ̃J[ݒ肪KvłB
#include "TextureCache.h"
#include "Debug.h"
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace
{
    void Trim(std::string& str)
    {
        const auto isNotSpace = [](unsigned char ch)
        {
            return !std::isspace(ch);
        };

        str.erase(str.begin(), std::find_if(str.begin(), str.end(), isNotSpace));
        str.erase(std::find_if(str.rbegin(), str.rend(), isNotSpace).base(), str.end());
    }

}

std::string TextureCache::NormalizeKey(std::string key)
{
    Trim(key);
    std::replace(key.begin(), key.end(), '\\', '/');
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (key.empty())
    {
        DebugMessage("[TextureCache] NormalizeKey produced an empty key.\n");
    }

    return key;
}

std::string TextureCache::MakeFileKey(const std::string& fullPath)
{
    try
    {
        const auto normalizedPath = std::filesystem::path(fullPath).lexically_normal();
        const auto u8 = normalizedPath.u8string();
        const std::string path8(u8.begin(), u8.end());
        const auto normalized = NormalizeKey(path8);
        return NormalizeKey("file:" + normalized);
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        DebugMessage("[TextureCache] Failed to normalize texture path '" + fullPath + "': " + e.what() + "\n");
    }
    catch (const std::exception& e)
    {
        DebugMessage("[TextureCache] Unexpected error while building texture key for path '" + fullPath + "': " + e.what() + "\n");
    }

    return {};
}

std::string TextureCache::MakeFbxEmbeddedKey(const std::string& fbxPath, int index)
{
    try
    {
        std::filesystem::path normalizedPath(std::filesystem::path(fbxPath).lexically_normal());
        normalizedPath.replace_extension();
        const auto u8 = normalizedPath.u8string();
        const std::string path8(u8.begin(), u8.end());
        const auto baseKey = NormalizeKey(path8);
        return NormalizeKey(baseKey + "::embedded::" + std::to_string(index));
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        DebugMessage("[TextureCache] Failed to normalize FBX embedded texture path '" + fbxPath + "': " + e.what() + "\n");
    }
    catch (const std::exception& e)
    {
        DebugMessage("[TextureCache] Unexpected error while building FBX embedded texture key for path '" + fbxPath + "': " + e.what() + "\n");
    }

    return {};
}

//============================================================
// eNX`[hăLbVɓo^
//============================================================
std::shared_ptr<Texture2D> TextureCache::Load(const std::string& name, ID3D11Device* device, const std::wstring& path)
{
    const auto key = NormalizeKey(name);

    if (key.empty())
    {
        DebugMessage("[TextureCache] Empty texture key provided; aborting texture load.\n");
        return nullptr;
    }
    // LbVɂłɑ݂邩mF
    auto it = textures.find(key);
    if (it != textures.end()) return it->second;

    // VeNX`쐬
    auto texture = std::make_shared<Texture2D>();
    if (!texture->LoadFromFile(device, path))
    {
        DebugMessage("[TextureCache] Failed to load texture '" + key + "' from file. Skipping cache entry.\n");
        return nullptr;
    }

    if (!texture->Get())
    {
        DebugMessage("[TextureCache] Texture '" + key + "' loaded from file but SRV is null.\n");
        return nullptr;
    }

    // LbVɓo^
    textures[key] = std::move(texture);
    return textures[key];
}

std::shared_ptr<Texture2D> TextureCache::LoadFromMemory(const std::string& name, ID3D11Device* device, const uint8_t* data, size_t size, unsigned int width, unsigned int height, const char* formatHint)
{
    const auto key = NormalizeKey(name);

    if (key.empty())
    {
        DebugMessage("[TextureCache] Empty texture key provided; aborting in-memory texture load.\n");
        return nullptr;
    }
    auto it = textures.find(key);
    if (it != textures.end()) return it->second;

    auto texture = std::make_shared<Texture2D>();
    if (!texture->LoadFromMemory(device, data, size, width, height, formatHint))
    {
        DebugMessage("[TextureCache] Failed to load in-memory texture '" + key + "'. Skipping cache entry.\n");
        return nullptr;
    }

    if (!texture->Get())
    {
        DebugMessage("[TextureCache] Texture '" + key + "' loaded from memory but SRV is null.\n");
        return nullptr;
    }

    textures[key] = std::move(texture);
    return textures[key];
}
//============================================================
// LbVeNX`擾
//============================================================
std::shared_ptr<Texture2D> TextureCache::GetTexture(const std::string& name)
{
    const auto key = NormalizeKey(name);

    if (key.empty())
    {
        return nullptr;
    }
    auto it = textures.find(key);

    if (it == textures.end())
    {
#ifdef _DEBUG
        if (missingLogged.emplace(key).second)
        {
            DebugMessage("[TextureCache] Texture not found in cache: '" + key + "'\n");
        }
#endif
        return nullptr;
    }

    return it->second;
}

ID3D11ShaderResourceView* TextureCache::Get(const std::string& name)
{
    const auto texture = GetTexture(name);
    return texture ? texture->Get() : nullptr;
}
//============================================================
// gp
//============================================================
/*
// VOgȂ̂łǂłANZX\
// FeNX`[hăLbVɓo^
auto& playerTex = TextureCache::Instance().Load("player", device, L"textures/player.png");
auto& enemyTex = TextureCache::Instance().Load("enemy", device, L"textures/enemy.png");
// `掞FLbVeNX`SRV 擾Ďgp
auto* srv = TextureCache::Instance().Get("player");
context->PSSetShaderResources(0, 1, &srv);
// ܂͒Z
TextureCache::Instance().Get("enemy");
// ӁFOŉxLoad()ĂłAŏ1񂾂[h܂
// 2ڈȍ~̓LbVꂽeNX`Ԃ܂
*/
