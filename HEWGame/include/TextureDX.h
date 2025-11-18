// ============================================================================
// 概要: TextureDX に関するインターフェースやデータ構造を宣言し、include から再利用できるようにする。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: include 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

#ifndef __TEXTUREDX_H__
#define __TEXTUREDX_H__

#include <string>
#include <d3d11.h>
#include <wrl/client.h>

class Texture2D
{
public:
    bool LoadFromFile(ID3D11Device* device, const std::wstring& path);
    ID3D11ShaderResourceView* Get() const { return textureView.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> textureView;
};
#endif