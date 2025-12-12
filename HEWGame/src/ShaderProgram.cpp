// ShaderProgram.cpp
#include "ShaderProgram.h"
#include "Debug.h"
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

bool ShaderProgram::LoadFromFiles(ID3D11Device* device,
    const std::string& vsPath,
    const std::string& psPath)
{
    DebugMessage("[Shader] 頂点シェーダーファイルを読み込みます: " + vsPath);
    DebugMessage("[Shader] ピクセルシェーダーファイルを読み込みます: " + psPath);

    // 頂点シェーダーをファイルから読み込み
    ID3DBlob* vsBlob = nullptr;
    std::wstring wVsPath(vsPath.begin(), vsPath.end());
    if (FAILED(D3DReadFileToBlob(wVsPath.c_str(), &vsBlob)))
    {
        DebugMessage("[Shader] 頂点シェーダーファイルを開けません: " + vsPath);
        return false;
    }

    // ピクセルシェーダーをファイルから読み込み
    ID3DBlob* psBlob = nullptr;
    std::wstring wPsPath(psPath.begin(), psPath.end());
    if (FAILED(D3DReadFileToBlob(wPsPath.c_str(), &psBlob)))
    {
        vsBlob->Release();
        DebugMessage("[Shader] ピクセルシェーダーファイルを開けません: " + psPath);
        return false;
    }

    // 頂点シェーダーオブジェクトを作成
    if (FAILED(device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, vertexShader.GetAddressOf())))
    {
        vsBlob->Release();
        psBlob->Release();
        return false;
    }

    // ピクセルシェーダーオブジェクトを作成
    if (FAILED(device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, pixelShader.GetAddressOf())))
    {
        vsBlob->Release();
        psBlob->Release();
        return false;
    }

    // Blobを解放
    vsBlob->Release();
    psBlob->Release();

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