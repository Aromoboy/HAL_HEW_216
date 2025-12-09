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
                std::shared_ptr<Texture2D> a_texture,
                bool usedFallback = false,
                std::shared_ptr<Texture2D> normalTexture = nullptr,
                std::shared_ptr<Texture2D> roughnessTexture = nullptr,
                std::shared_ptr<Texture2D> metallicTexture = nullptr,
                std::shared_ptr<Texture2D> aoTexture = nullptr);
        ~MaterialDX();

        ID3D11ShaderResourceView* GetTexture() const;
        void BindTexture(ID3D11DeviceContext* context, UINT slot = 0) const;
        void Apply(ID3D11DeviceContext* context) const;

        DirectX::XMFLOAT4 GetDiffuse() const;
        DirectX::XMFLOAT4 GetAmbient() const;
        DirectX::XMFLOAT4 GetSpecular() const;

        std::shared_ptr<Texture2D> GetTextureResource() const;

        bool UsedFallbackTexture() const { return m_usedFallback; }

        void SetDiffuse(const DirectX::XMFLOAT4& diffuse);
        void SetAmbient(const DirectX::XMFLOAT4& ambient);
        void SetSpecular(const DirectX::XMFLOAT4& specular);

private:

        DirectX::XMFLOAT4 m_diffuse;
        DirectX::XMFLOAT4 m_ambient;
        DirectX::XMFLOAT4 m_specular;

        std::shared_ptr<Texture2D> m_texture;
        std::shared_ptr<Texture2D> m_normalTexture;
        std::shared_ptr<Texture2D> m_roughnessTexture;
        std::shared_ptr<Texture2D> m_metallicTexture;
        std::shared_ptr<Texture2D> m_aoTexture;

        ID3D11ShaderResourceView* diffuseSRV;
        ID3D11ShaderResourceView* normalSRV;
        ID3D11ShaderResourceView* roughnessSRV;
        ID3D11ShaderResourceView* metallicSRV;
        ID3D11ShaderResourceView* aoSRV;

        bool m_usedFallback = false;
        mutable bool m_warnedMissingTexture = false;




	
};

