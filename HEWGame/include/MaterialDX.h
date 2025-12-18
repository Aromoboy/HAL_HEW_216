#pragma once
#include "TextureDX.h"
#include <DirectXMath.h>
#include <array>
#include <memory>

class MaterialDX
{
public:
	enum class TextureSlot : UINT
	{
	    Diffuse = 0,
	    Normal = 1,
	    Specular = 2,
	    Count = 3
	};

	MaterialDX(DirectX::XMFLOAT4 diffuse, DirectX::XMFLOAT4 ambient, DirectX::XMFLOAT4 specular,
	    std::shared_ptr<Texture2D> diffuseTexture,
	    std::shared_ptr<Texture2D> normalTexture = nullptr,
	    std::shared_ptr<Texture2D> specularTexture = nullptr);
	~MaterialDX();

        ID3D11ShaderResourceView* GetTexture(TextureSlot slot = TextureSlot::Diffuse) const;
        void BindTexture(ID3D11DeviceContext* context, TextureSlot slot = TextureSlot::Diffuse) const;
        void BindTextures(ID3D11DeviceContext* context) const;

        DirectX::XMFLOAT4 GetDiffuse() const;
        DirectX::XMFLOAT4 GetAmbient() const;
        DirectX::XMFLOAT4 GetSpecular() const;

        std::shared_ptr<Texture2D> GetTextureResource(TextureSlot slot = TextureSlot::Diffuse) const;
        std::shared_ptr<Texture2D> GetDiffuseTextureResource() const;
        std::shared_ptr<Texture2D> GetNormalTextureResource() const;
        std::shared_ptr<Texture2D> GetSpecularTextureResource() const;

        void SetDiffuse(const DirectX::XMFLOAT4& diffuse);
        void SetAmbient(const DirectX::XMFLOAT4& ambient);
        void SetSpecular(const DirectX::XMFLOAT4& specular);

private:

	DirectX::XMFLOAT4 m_diffuse;
	DirectX::XMFLOAT4 m_ambient;
	DirectX::XMFLOAT4 m_specular;

	std::array<std::shared_ptr<Texture2D>, static_cast<size_t>(TextureSlot::Count)> m_textures;
};
