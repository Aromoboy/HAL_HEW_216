// ============================================================================
// Tv: ShaderCache ̋֘AWbNAsrc ɑ@\̉ĂB
// ړI: ߂ẴW[ɐGJ҂łAӖƈˑ֌Wfcł悤ɍx̐t^ĂB
// Ag: src z̑t@CVXewƘA邽߁At@CԂ̊֌WǐՂۂ̖ڈƂėpłB
// ============================================================================

// l: ShaderCache.cpp Ɋւ{ŕ⑫ĂB
#include "ShaderCache.h"
#include <iostream>
#include "Debug.h"

//============================================================
// シェーダープログラムをロードしてキャッシュに登録
//============================================================
std::shared_ptr<ShaderProgram> ShaderCache::Load(const std::string& name,
    ID3D11Device* device,
    const std::string& vsPath,
    const std::string& psPath)
{
    // キャッシュにすでに存在するか確認
    auto it = programs.find(name);
    if (it != programs.end())
        return it->second;

    // 新しいシェーダープログラムを作成
    auto shader = std::make_unique<ShaderProgram>();
    if (!shader->LoadFromFiles(device, vsPath, psPath))
    {
        DebugMessage("[ShaderCache] シェーダーのロードに失敗: ");
        throw std::runtime_error("シェーダーのロードに失敗: " + name);
    }

    // キャッシュに登録
    ShaderProgram& ref = *shader;
    programs[name] = std::move(shader);
    return programs[name];
}

//============================================================
// キャッシュからシェーダープログラムを取得
//============================================================
std::shared_ptr<ShaderProgram> ShaderCache::Get(const std::string& name)
{
    auto it = programs.find(name);
    if (it == programs.end())
        throw std::runtime_error("シェーダーが見つかりません: " + name);

    return programs[name];
}

//============================================================
// 使用例
//============================================================
/*
// シングルトンなのでどこからでもアクセス可能

// 初期化時：シェーダーをロードしてキャッシュに登録
auto& basicShader = ShaderCache::Instance().Load("basic", device, "shaders/basic_vs.cso", "shaders/basic_ps.cso");
auto& lightShader = ShaderCache::Instance().Load("light", device, "shaders/light_vs.cso", "shaders/light_ps.cso");

// 描画時：キャッシュからシェーダーを取得して使用
auto& shader = ShaderCache::Instance().Get("basic");  // 高速（キャッシュから取得）
shader.Bind(context);  // シェーダーをパイプラインに設定

// または直接バインド
ShaderCache::Instance().Get("light").Bind(context);

// 注意：同じ名前で何度Load()を呼んでも、最初の1回だけロードされます
// 2回目以降はキャッシュされたシェーダーが返されます
*/