#include "MaterialDX.h"

MaterialDX::MaterialDX(
        DirectX::XMFLOAT4 diffuse,
        DirectX::XMFLOAT4 ambient,
        DirectX::XMFLOAT4 specular,
        std::shared_ptr<Texture2D> a_texture)
        : m_diffuse(diffuse)
        , m_ambient(ambient)
        , m_specular(specular)
        , m_texture(std::move(a_texture))
        , m_pTexture(nullptr)
{
        m_pTexture = m_texture ? m_texture->Get() : nullptr;
}

MaterialDX::~MaterialDX() = default;

ID3D11ShaderResourceView* MaterialDX::GetTexture() const
{
        return m_pTexture;
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
