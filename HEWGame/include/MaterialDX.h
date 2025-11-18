// ============================================================================
// 概要: MaterialDX に関するインターフェースやデータ構造を宣言し、include から再利用できるようにする。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: include 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

#pragma once
#include "TextureDX.h"
#include <DirectXMath.h>
#include <memory>

class MaterialDX
{
public:
	MaterialDX(DirectX::XMFLOAT4 diffuse, DirectX::XMFLOAT4 ambient, DirectX::XMFLOAT4 specular, std::shared_ptr<Texture2D> a_tecture);
	~MaterialDX();

        ID3D11ShaderResourceView* GetTexture() const;
        void BindTexture(ID3D11DeviceContext* context, UINT slot = 0) const;
	
	DirectX::XMFLOAT4 GetDiffuse() const;
	DirectX::XMFLOAT4 GetAmbient() const;
	DirectX::XMFLOAT4 GetSpecular() const;

private:

	DirectX::XMFLOAT4 m_diffuse;
	DirectX::XMFLOAT4 m_ambient;
	DirectX::XMFLOAT4 m_specular;

	std::shared_ptr<Texture2D> m_texture;




	
};

