#include "MaterialDX.h"
#include <cctype>

namespace
{
    std::string NormalizeMaterialKey(std::string value)
    {
        for (char& c : value)
        {
            if (c == '\\')
            {
                c = '/';
            }
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return value;
    }

    const MaterialTuningProfile& ResolveMaterialProfile(
        const MaterialTuningSettings& settings,
        const std::string& profileName)
    {
        const auto profileIt = settings.profiles.find(profileName);
        if (profileIt != settings.profiles.end())
        {
            return profileIt->second;
        }

        static const MaterialTuningProfile fallbackProfile{};
        return fallbackProfile;
    }
}

void ApplyMaterialTuning(MaterialDX& material, const std::string& materialKey, const MaterialTuningSettings& settings)
{
    static const std::string kDefaultProfileKey = "default";
    const std::string normalizedKey = NormalizeMaterialKey(materialKey);
    const auto overrideIt = settings.overrides.find(normalizedKey);
    const MaterialTuningOverride* overrideSettings = (overrideIt != settings.overrides.end())
        ? &overrideIt->second
        : nullptr;
    const std::string& profileName = (overrideSettings && !overrideSettings->profileName.empty())
        ? overrideSettings->profileName
        : kDefaultProfileKey;
    const auto profileIt = settings.profiles.find(profileName);
    const MaterialTuningProfile* selectedProfile = nullptr;
    if (profileIt != settings.profiles.end())
    {
        selectedProfile = &profileIt->second;
    }
    else if (profileName != kDefaultProfileKey)
    {
        const auto defaultIt = settings.profiles.find(kDefaultProfileKey);
        if (defaultIt != settings.profiles.end())
        {
            selectedProfile = &defaultIt->second;
        }
    }

    const MaterialTuningProfile& profile = selectedProfile
        ? *selectedProfile
        : ResolveMaterialProfile(settings, profileName);
    material.SetLightResponse(profile.lightResponse);
    material.SetSssIntensity(profile.sssIntensity);
    material.SetSssColor(profile.sssColor);

    if (overrideSettings)
    {
        if (overrideSettings->lightResponse)
        {
            material.SetLightResponse(*overrideSettings->lightResponse);
        }
        if (overrideSettings->sssIntensity)
        {
            material.SetSssIntensity(*overrideSettings->sssIntensity);
        }
        if (overrideSettings->sssColor)
        {
            material.SetSssColor(*overrideSettings->sssColor);
        }
    }
}

MaterialDX::MaterialDX(DirectX::XMFLOAT4 diffuse, DirectX::XMFLOAT4 ambient, DirectX::XMFLOAT4 specular, std::shared_ptr<Texture2D> a_texture)
	: m_diffuse(diffuse)
	, m_ambient(ambient)
	, m_specular(specular)
	, m_texture(a_texture)
{}

MaterialDX::~MaterialDX()
{
}


//	eQb^
ID3D11ShaderResourceView* MaterialDX::GetTexture() const
{
	return m_texture ? m_texture->Get() : nullptr;
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

const std::string& MaterialDX::GetMaterialKey() const
{
        return m_materialKey;
}

void MaterialDX::SetMaterialKey(std::string key)
{
        m_materialKey = std::move(key);
}
