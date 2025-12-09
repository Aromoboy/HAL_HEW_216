#pragma once

#include "TextureDX.h"

#include <DirectXMath.h>
#include <memory>

class MaterialDX
{
public:
        MaterialDX(
                DirectX::XMFLOAT4 diffuse,
                DirectX::XMFLOAT4 ambient,
                DirectX::XMFLOAT4 specular,
                std::shared_ptr<Texture2D> a_texture);
        ~MaterialDX();

        ID3D11ShaderResourceView* GetTexture() const;

        DirectX::XMFLOAT4 GetDiffuse() const;
        DirectX::XMFLOAT4 GetAmbient() const;
        DirectX::XMFLOAT4 GetSpecular() const;

        std::shared_ptr<Texture2D> GetTextureResource() const;

        void SetDiffuse(const DirectX::XMFLOAT4& diffuse);
        void SetAmbient(const DirectX::XMFLOAT4& ambient);
        void SetSpecular(const DirectX::XMFLOAT4& specular);

private:

        DirectX::XMFLOAT4 m_diffuse;
        DirectX::XMFLOAT4 m_ambient;
        DirectX::XMFLOAT4 m_specular;

        std::shared_ptr<Texture2D> m_texture;
        ID3D11ShaderResourceView* m_pTexture;
};
