#pragma once
#include "Texture.h"
#include <DirectXMath.h>
#include <memory>

class MaterialDX
{
public:
	MaterialDX(DirectX::XMFLOAT4 diffuse, DirectX::XMFLOAT4 ambient, DirectX::XMFLOAT4 specular, ID3D11ShaderResourceView* m_texture );
	~MaterialDX();

	ID3D11ShaderResourceView* GetTexture() const;
	
	DirectX::XMFLOAT4 GetDiffuse() const;
	DirectX::XMFLOAT4 GetAmbient() const;
	DirectX::XMFLOAT4 GetSpecular() const;

private:

	DirectX::XMFLOAT4 m_diffuse;
	DirectX::XMFLOAT4 m_ambient;
	DirectX::XMFLOAT4 m_specular;

	ID3D11ShaderResourceView* m_texture;




	
};

