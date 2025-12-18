#include "MaterialDX.h"

MaterialDX::MaterialDX(DirectX::XMFLOAT4 diffuse, DirectX::XMFLOAT4 ambient, DirectX::XMFLOAT4 specular,
    std::shared_ptr<Texture2D> diffuseTexture,
    std::shared_ptr<Texture2D> normalTexture,
    std::shared_ptr<Texture2D> specularTexture)
	: m_diffuse(diffuse)
	, m_ambient(ambient)
	, m_specular(specular)
	, m_textures{ diffuseTexture, normalTexture, specularTexture }
{
}

MaterialDX::~MaterialDX()
{
}


//	eQb^
ID3D11ShaderResourceView* MaterialDX::GetTexture(TextureSlot slot) const
{
	const size_t index = static_cast<size_t>(slot);
	if (index >= m_textures.size())
	{
		return nullptr;
	}

	return m_textures[index] ? m_textures[index]->Get() : nullptr;
}

void MaterialDX::BindTexture(ID3D11DeviceContext* context, TextureSlot slot) const
{
	if (!context)
	{
		return;
	}

	const UINT slotIndex = static_cast<UINT>(slot);
	ID3D11ShaderResourceView* textureView = GetTexture(slot);
	context->PSSetShaderResources(slotIndex, 1, &textureView);
}

void MaterialDX::BindTextures(ID3D11DeviceContext* context) const
{
	if (!context)
	{
		return;
	}

	ID3D11ShaderResourceView* textureViews[static_cast<size_t>(TextureSlot::Count)] = {};
	for (size_t i = 0; i < m_textures.size(); ++i)
	{
		textureViews[i] = m_textures[i] ? m_textures[i]->Get() : nullptr;
	}

	context->PSSetShaderResources(0, static_cast<UINT>(TextureSlot::Count), textureViews);
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

std::shared_ptr<Texture2D> MaterialDX::GetTextureResource(TextureSlot slot) const
{
	const size_t index = static_cast<size_t>(slot);
	if (index >= m_textures.size())
	{
		return nullptr;
	}
	return m_textures[index];
}

std::shared_ptr<Texture2D> MaterialDX::GetDiffuseTextureResource() const
{
	return GetTextureResource(TextureSlot::Diffuse);
}

std::shared_ptr<Texture2D> MaterialDX::GetNormalTextureResource() const
{
	return GetTextureResource(TextureSlot::Normal);
}

std::shared_ptr<Texture2D> MaterialDX::GetSpecularTextureResource() const
{
	return GetTextureResource(TextureSlot::Specular);
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
