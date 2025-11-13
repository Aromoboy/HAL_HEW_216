// ShaderProgram.cpp
#include "ShaderProgram.h"
#include <fstream>
#include <vector>
#include <iostream>
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

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

    return true;
}

//============================================================
// シェーダープログラムをファイルからロード
//============================================================
bool ShaderProgram::LoadFromFiles(ID3D11Device* device,
    const std::string& vsPath,
    const std::string& psPath)
{
    std::vector<char> vsData, psData;

    // 頂点シェーダーとピクセルシェーダーの両方を読み込み
    if (!ReadFileBytes(vsPath, vsData) || !ReadFileBytes(psPath, psData))
        return false;

    // 頂点シェーダーオブジェクトを作成
    if (FAILED(device->CreateVertexShader(vsData.data(), vsData.size(), nullptr, vertexShader.GetAddressOf())))
        return false;

    // ピクセルシェーダーオブジェクトを作成
    if (FAILED(device->CreatePixelShader(psData.data(), psData.size(), nullptr, pixelShader.GetAddressOf())))
        return false;

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
// 使用例
//============================================================
/*
// シェーダープログラムを作成
ShaderProgram shader;

// シェーダーファイルをロード（.cso = コンパイル済みシェーダーオブジェクト）
if (shader.LoadFromFiles(device, "shaders/basic_vs.cso", "shaders/basic_ps.cso"))
{
    // 描画時にシェーダーをバインド
    shader.Bind(context);

    // 描画処理...
    context->Draw(...);
}

// 注意：通常はShaderCacheを使用してシェーダーを管理します
// ShaderProgramを直接使うのは特別な場合のみです
*/
