#include "ShaderProgram.h"
#include "Debug.h"
#include <d3dcompiler.h>
#include <filesystem>
#pragma comment(lib, "d3dcompiler.lib")

namespace
{
    bool LoadFromFilesInternal(ID3D11Device* device,
        const std::filesystem::path& vsPath,
        const std::filesystem::path& psPath,
        const std::string& vsLogPath,
        const std::string& psLogPath,
        Microsoft::WRL::ComPtr<ID3D11VertexShader>& vertexShader,
        Microsoft::WRL::ComPtr<ID3D11PixelShader>& pixelShader)
    {
        DebugMessage("[Shader] 頂点シェーダーファイルを読み込みます: " + vsLogPath);
        DebugMessage("[Shader] ピクセルシェーダーファイルを読み込みます: " + psLogPath);

        // 頂点シェーダーをファイルから読み込み
        ID3DBlob* vsBlob = nullptr;
        if (FAILED(D3DReadFileToBlob(vsPath.wstring().c_str(), &vsBlob)))
        {
            DebugMessage("[Shader] 頂点シェーダーファイルを開けません: " + vsLogPath);
            return false;
        }

        // ピクセルシェーダーをファイルから読み込み
        ID3DBlob* psBlob = nullptr;
        if (FAILED(D3DReadFileToBlob(psPath.wstring().c_str(), &psBlob)))
        {
            vsBlob->Release();
            DebugMessage("[Shader] ピクセルシェーダーファイルを開けません: " + psLogPath);
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
}

bool ShaderProgram::LoadFromFiles(ID3D11Device* device,
    const std::filesystem::path& vsPath,
    const std::filesystem::path& psPath)
{
    return LoadFromFilesInternal(device,
        vsPath,
        psPath,
        vsPath.string(),
        psPath.string(),
        vertexShader,
        pixelShader);
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
if (shader.LoadFromFiles(device,
    std::filesystem::path("shaders/basic_vs.cso"),
    std::filesystem::path("shaders/basic_ps.cso")))
{
    // 描画時にシェーダーをバインド
    shader.Bind(context);
    // 描画処理...
    context->Draw(...);
}
// 注意：通常はShaderCacheを使用してシェーダーを管理します
// ShaderProgramを直接使うのは特別な場合のみです
*/
