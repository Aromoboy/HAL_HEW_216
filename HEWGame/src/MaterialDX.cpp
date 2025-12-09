#include "MaterialDX.h"
#include "Debug.h"

MaterialDX::MaterialDX(
        DirectX::XMFLOAT4 diffuse,
        DirectX::XMFLOAT4 ambient,
        DirectX::XMFLOAT4 specular,
        std::shared_ptr<Texture2D> a_texture,
        bool usedFallback,
        std::shared_ptr<Texture2D> normalTexture,
        std::shared_ptr<Texture2D> roughnessTexture,
        std::shared_ptr<Texture2D> metallicTexture,
        std::shared_ptr<Texture2D> aoTexture)
        : m_diffuse(diffuse)
        , m_ambient(ambient)
        , m_specular(specular)
        , m_texture(std::move(a_texture))
        , m_normalTexture(std::move(normalTexture))
        , m_roughnessTexture(std::move(roughnessTexture))
        , m_metallicTexture(std::move(metallicTexture))
        , m_aoTexture(std::move(aoTexture))
        , diffuseSRV(nullptr)
        , normalSRV(nullptr)
        , roughnessSRV(nullptr)
        , metallicSRV(nullptr)
        , aoSRV(nullptr)
        , m_usedFallback(usedFallback)
{
        diffuseSRV = m_texture ? m_texture->Get() : nullptr;
        normalSRV = m_normalTexture ? m_normalTexture->Get() : nullptr;
        roughnessSRV = m_roughnessTexture ? m_roughnessTexture->Get() : nullptr;
        metallicSRV = m_metallicTexture ? m_metallicTexture->Get() : nullptr;
        aoSRV = m_aoTexture ? m_aoTexture->Get() : nullptr;
}

MaterialDX::~MaterialDX()
{
}


//	eQb^
ID3D11ShaderResourceView* MaterialDX::GetTexture() const
{
        return diffuseSRV;
}

void MaterialDX::BindTexture(ID3D11DeviceContext* context, UINT slot) const
{
        if (!context)
        {
                return;
        }

        ID3D11ShaderResourceView* textureView = GetTexture();
        if (!textureView && !m_warnedMissingTexture)
        {
                if (m_usedFallback)
                {
                        DebugMessage("[MaterialDX] WARNING: SRV is NULL after applying fallback texture");
                }
                else
                {
                        DebugMessage("[MaterialDX] WARNING: SRV is NULL for material");
                }
                m_warnedMissingTexture = true;
        }
        context->PSSetShaderResources(slot, 1, &textureView);
}

void MaterialDX::Apply(ID3D11DeviceContext* context) const
{
        if (!context)
        {
                return;
        }

        ID3D11ShaderResourceView* srvs[] = { diffuseSRV, normalSRV, roughnessSRV, metallicSRV, aoSRV };

        if (!srvs[0] && !m_warnedMissingTexture)
        {
                if (m_usedFallback)
                {
                        DebugMessage("[MaterialDX] WARNING: SRV is NULL after applying fallback texture");
                }
                else
                {
                        DebugMessage("[MaterialDX] WARNING: SRV is NULL for material");
                }
                m_warnedMissingTexture = true;
        }

        for (auto& srv : srvs)
        {
                if (!srv)
                {
                        srv = nullptr;
                }
        }

        context->PSSetShaderResources(0, 5, srvs);
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

std::shared_ptr<Texture2D> MaterialDX::GetTextureResource() const
{
        return m_texture;
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
