// DirectXTK (DirectX Tool Kit) を使用出来なかったので、
// 標準の Windows Imaging Component (WIC) を直接使用して
// テクスチャをロードしています。
// そのため、wincodec.h のインクルードと
// windowscodecs.lib のリンカー設定が必要です。
#include "TextureCache.h"
#include "Debug.h"
#include <iostream>

//============================================================
// テクスチャをロードしてキャッシュに登録
//============================================================
std::shared_ptr<Texture2D> TextureCache::Load(const std::string& name, ID3D11Device* device, const std::wstring& path)
{
    // キャッシュにすでに存在するか確認
    auto it = textures.find(name);
    if (it != textures.end()) return it->second;

    // 新しいテクスチャを作成
    auto texture = std::make_shared<Texture2D>();
    if (!texture->LoadFromFile(device, path)) return nullptr;
    
    // キャッシュに登録
    textures[name] = std::move(texture);
    return textures[name];
}
//============================================================
// キャッシュからテクスチャを取得
//============================================================
std::shared_ptr<Texture2D> TextureCache::Get(const std::string& name)
{
    auto it = textures.find(name);

    if (it == textures.end()) return nullptr;    

    return it->second;
}
//============================================================
// 使用例
//============================================================
/*
// シングルトンなのでどこからでもアクセス可能
// 初期化時：テクスチャをロードしてキャッシュに登録
auto& playerTex = TextureCache::Instance().Load("player", device, L"textures/player.png");
auto& enemyTex = TextureCache::Instance().Load("enemy", device, L"textures/enemy.png");
// 描画時：キャッシュからテクスチャを取得して使用
auto& tex = TextureCache::Instance().Get("player");  // 高速（キャッシュから取得）
context->PSSetShaderResources(0, 1, tex.textureView.GetAddressOf());
// または短く書く
TextureCache::Instance().Get("enemy");
// 注意：同じ名前で何度Load()を呼んでも、最初の1回だけロードされます
// 2回目以降はキャッシュされたテクスチャが返されます
*/