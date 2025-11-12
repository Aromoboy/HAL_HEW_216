//ShaderCache.cpp
#include "ShaderCache.h"
#include <fstream>
#include <vector>
#include <d3dcompiler.h>
#include <iostream>
#pragma comment(lib, "d3dcompiler.lib")

using namespace Core;

//============================================================
// ユーティリティ関数：ファイルをバイナリデータとして読み込む
//============================================================
static bool ReadFileBytes(const std::string& path, std::vector<char>& data)
{
    // バイナリモードでファイルを開く
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "[Shader] ファイルを開けません: " << path << std::endl;
        return false;
    }

    // ファイルサイズを取得
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    // データバッファを確保して読み込み
    data.resize(size);
    file.read(data.data(), size);

    std::cout << "[Shader] ファイル読み込み成功: " << path << " (" << size << " bytes)" << std::endl;
    return true;
}

//============================================================
// シェーダープログラムをファイルからロード
//============================================================
bool ShaderProgram::LoadFromFiles(ID3D11Device* device, const std::string& vsPath, const std::string& psPath)
{
    std::vector<char> vsData, psData;

    // 頂点シェーダーとピクセルシェーダーの両方を読み込み
    if (!ReadFileBytes(vsPath, vsData) || !ReadFileBytes(psPath, psData))
    {
        std::cerr << "[ShaderProgram] シェーダーファイルの読み込みに失敗しました" << std::endl;
        return false;
    }

    // 頂点シェーダーオブジェクトを作成
    HRESULT hr = device->CreateVertexShader(vsData.data(), vsData.size(), nullptr, vertexShader.GetAddressOf());
    if (FAILED(hr))
    {
        std::cerr << "[ShaderProgram] 頂点シェーダーの作成に失敗: " << vsPath << std::endl;
        return false;
    }
    std::cout << "[ShaderProgram] 頂点シェーダー作成成功: " << vsPath << std::endl;

    // ピクセルシェーダーオブジェクトを作成
    hr = device->CreatePixelShader(psData.data(), psData.size(), nullptr, pixelShader.GetAddressOf());
    if (FAILED(hr))
    {
        std::cerr << "[ShaderProgram] ピクセルシェーダーの作成に失敗: " << psPath << std::endl;
        return false;
    }
    std::cout << "[ShaderProgram] ピクセルシェーダー作成成功: " << psPath << std::endl;

    return true;
}

//============================================================
// シェーダーをレンダリングパイプラインに設定
//============================================================
void ShaderProgram::Bind(ID3D11DeviceContext* context)
{
    // 頂点シェーダーとピクセルシェーダーをコンテキストに設定
    context->VSSetShader(vertexShader.Get(), nullptr, 0);
    context->PSSetShader(pixelShader.Get(), nullptr, 0);
}

//============================================================
// シェーダーをロードしてキャッシュに登録
//============================================================
ShaderProgram& ShaderCache::Load(const std::string& name, ID3D11Device* device,
    const std::string& vsPath, const std::string& psPath)
{
    // キャッシュにすでに存在するか確認
    auto it = programs.find(name);
    if (it != programs.end())
    {
        std::cout << "[ShaderCache] キャッシュされたシェーダーを使用: " << name << std::endl;
        return *it->second;
    }

    // 新しいシェーダープログラムを作成
    auto shader = std::make_unique<ShaderProgram>();
    if (!shader->LoadFromFiles(device, vsPath, psPath))
    {
        std::cerr << "[ShaderCache] シェーダーのロードに失敗しました: " << name << std::endl;
        throw std::runtime_error("シェーダーのロードに失敗: " + name);
    }

    // キャッシュに登録
    ShaderProgram& ref = *shader;
    programs[name] = std::move(shader);
    std::cout << "[ShaderCache] シェーダーをキャッシュに登録: " << name << std::endl;
    return ref;
}

//============================================================
// キャッシュからシェーダーを取得
//============================================================
ShaderProgram& ShaderCache::Get(const std::string& name)
{
    auto it = programs.find(name);
    if (it == programs.end())
    {
        std::cerr << "[ShaderCache] シェーダーが見つかりません: " << name << std::endl;
        throw std::runtime_error("シェーダーが見つかりません: " + name);
    }

    return *it->second;
}

//============================================================
// ShaderCache 使用例
//============================================================
/*
// 前提: ID3D11Device* device, ID3D11DeviceContext* context が作成済み

// 1. シェーダーキャッシュの作成（シングルトン推奨）
Core::ShaderCache& shaderCache = Core::ShaderCache::Instance(); // もしシングルトンなら

// 2. シェーダーのロード
try {
    // 初回：ファイルからロードしてキャッシュに登録
    Core::ShaderProgram& basicShader = shaderCache.Load(
        "BasicShader",                    // キャッシュ内での名前（一意）
        device,
        "shaders/BasicVS.cso",           // 頂点シェーダーバイナリ
        "shaders/BasicPS.cso"            // ピクセルシェーダーバイナリ
    );

    // 3. シェーダーの使用（描画前にバインド）
    basicShader.Bind(context);

    // 4. キャッシュからの再利用
    // 同じ名前でロードするとキャッシュが返される
    Core::ShaderProgram& sameShader = shaderCache.Load(
        "BasicShader", device, "", ""   // vs/psパスは実際には使われない
    );

    // 5. 別のシェーダーのロード
    Core::ShaderProgram& uiShader = shaderCache.Load(
        "UIShader",
        device,
        "shaders/UIVS.cso",
        "shaders/UIPS.cso"
    );

} catch (const std::runtime_error& e) {
    std::cerr << "シェーダーロードエラー: " << e.what() << std::endl;
}

// 6. 取得のみ（存在しない場合は例外）
try {
    Core::ShaderProgram& shader = shaderCache.Get("BasicShader");
    shader.Bind(context);
} catch (const std::runtime_error& e) {
    std::cerr << "シェーダー取得エラー: " << e.what() << std::endl;
}

// 7. 描画時の使用
void RenderScene(ID3D11DeviceContext* context) {
    // 背景用シェーダー
    shaderCache.Get("SkyShader").Bind(context);
    DrawSky();

    // モデル用シェーダー
    shaderCache.Get("BasicShader").Bind(context);
    DrawModels();

    // UI用シェーダー
    shaderCache.Get("UIShader").Bind(context);
    DrawUI();
}
*/