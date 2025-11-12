#ifndef __TEXTURECACHE_H__
#define __TEXTURECACHE_H__

#include <d3d11.h>
#include <map>
#include <memory>
#include <string>

//============================================================
// Texture2D - テクスチャリソースをラップ
//============================================================
class Texture2D {
public:
    ID3D11Texture2D* texture = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;

    ~Texture2D() {
        if (srv) srv->Release();
        if (texture) texture->Release();
    }

    bool LoadFromFile(ID3D11Device* device, const std::string& path);
    void BindPS(ID3D11DeviceContext* context, UINT slot);
};

//============================================================
// TextureCache - テクスチャ管理
//============================================================
class TextureCache {
public:
    std::map<std::string, std::unique_ptr<Texture2D>> textures;

    Texture2D& Load(const std::string& path, ID3D11Device* device);
    Texture2D& Get(const std::string& path);
};

#endif