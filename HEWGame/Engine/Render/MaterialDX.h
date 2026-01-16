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
        float GetLightResponse() const;
        float GetSssIntensity() const;
        DirectX::XMFLOAT3 GetSssColor() const;

        std::shared_ptr<Texture2D> GetTextureResource() const;

        void SetDiffuse(const DirectX::XMFLOAT4& diffuse);
        void SetAmbient(const DirectX::XMFLOAT4& ambient);
        void SetSpecular(const DirectX::XMFLOAT4& specular);
        void SetLightResponse(float lightResponse);
        void SetSssIntensity(float sssIntensity);
        void SetSssColor(const DirectX::XMFLOAT3& sssColor);

private:

	DirectX::XMFLOAT4 m_diffuse;
	DirectX::XMFLOAT4 m_ambient;
	DirectX::XMFLOAT4 m_specular;
	float m_lightResponse = 1.0f;
	float m_sssIntensity = 0.0f;
	DirectX::XMFLOAT3 m_sssColor{ 1.0f, 1.0f, 1.0f };

	std::shared_ptr<Texture2D> m_texture;
};
