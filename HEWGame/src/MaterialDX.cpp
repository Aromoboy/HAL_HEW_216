#include "MaterialDX.h"

MaterialDX::MaterialDX(DirectX::XMFLOAT4 diffuse, DirectX::XMFLOAT4 ambient, DirectX::XMFLOAT4 specular, std::shared_ptr<Texture2D> a_texture)
	: m_diffuse(diffuse)
	, m_ambient(ambient)
	, m_specular(specular)
	, m_texture(a_texture)
{}

MaterialDX::~MaterialDX()
{
}


//	ŠeŽíƒQƒbƒ^
ID3D11ShaderResourceView* MaterialDX::GetTexture() const
{
	return m_texture->Get();
}

DirectX::XMFLOAT4 MaterialDX::GetDiffuse() const
{
	return m_diffuse;
}

DirectX::XMFLOAT4 MaterialDX::GetAmbient() const
{
	return m_ambient;

}

DirectX::XMFLOAT4 MaterialDX::GetSpecular() const
{
	return m_specular;
}
