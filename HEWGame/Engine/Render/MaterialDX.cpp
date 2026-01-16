#include "MaterialDX.h"
#include <utility>
MaterialDX::MaterialDX(DirectX::XMFLOAT4 diffuse, DirectX::XMFLOAT4 ambient, DirectX::XMFLOAT4 specular, std::shared_ptr<Texture2D> diffuseTexture)
	: MaterialDX(diffuse, ambient, specular, diffuseTexture, nullptr)
{}

MaterialDX::MaterialDX(DirectX::XMFLOAT4 diffuse, DirectX::XMFLOAT4 ambient, DirectX::XMFLOAT4 specular, std::shared_ptr<Texture2D> diffuseTexture, std::shared_ptr<Texture2D> normalTexture)
	: m_diffuse(diffuse)
	, m_ambient(ambient)
	, m_specular(specular)
	, m_diffuseTexture(std::move(diffuseTexture))
	, m_normalTexture(std::move(normalTexture))
{}

MaterialDX::~MaterialDX()
{
}


//	eQb^
ID3D11ShaderResourceView* MaterialDX::GetTexture() const
{
	return m_diffuseTexture ? m_diffuseTexture->Get() : nullptr;
}

ID3D11ShaderResourceView* MaterialDX::GetNormalTexture() const
{
	return m_normalTexture ? m_normalTexture->Get() : nullptr;
}

void MaterialDX::BindTexture(ID3D11DeviceContext* context, UINT slot) const
{
	if (!context)
	{
		return;
	}

	ID3D11ShaderResourceView* textureView = GetTexture();
	context->PSSetShaderResources(slot, 1, &textureView);
}

void MaterialDX::BindNormalTexture(ID3D11DeviceContext* context, UINT slot) const
{
	if (!context)
	{
		return;
	}

	ID3D11ShaderResourceView* textureView = GetNormalTexture();
	context->PSSetShaderResources(slot, 1, &textureView);
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

float MaterialDX::GetLightResponse() const
{
        return m_lightResponse;
}

float MaterialDX::GetSssIntensity() const
{
        return m_sssIntensity;
}

DirectX::XMFLOAT3 MaterialDX::GetSssColor() const
{
        return m_sssColor;
}

std::shared_ptr<Texture2D> MaterialDX::GetTextureResource() const
{
        return m_diffuseTexture;
}

std::shared_ptr<Texture2D> MaterialDX::GetNormalTextureResource() const
{
        return m_normalTexture;
}

void MaterialDX::SetDiffuse(const DirectX::XMFLOAT4& diffuse)
{
        m_diffuse = diffuse;
}

void MaterialDX::SetAmbient(const DirectX::XMFLOAT4& ambient)
{
        m_ambient = ambient;
}

void MaterialDX::SetSpecular(const DirectX::XMFLOAT4& specular)
{
        m_specular = specular;
}

void MaterialDX::SetLightResponse(float lightResponse)
{
        m_lightResponse = lightResponse;
}

void MaterialDX::SetSssIntensity(float sssIntensity)
{
        m_sssIntensity = sssIntensity;
}

void MaterialDX::SetSssColor(const DirectX::XMFLOAT3& sssColor)
{
        m_sssColor = sssColor;
}
