#include "ShaderList.h"


VertexShader* ShaderList::m_pVS[VS_KIND_MAX];
PixelShader* ShaderList::m_pPS[PS_KIND_MAX];


ShaderList::ShaderList()
{
}
ShaderList::~ShaderList()
{
}

void ShaderList::Init()
{
        MakeWorldVS();
        MakeLambertPS();

        ShaderList::WVPMatrix identityWvp = {};
        DirectX::XMStoreFloat4x4(&identityWvp.world, DirectX::XMMatrixIdentity());
        DirectX::XMStoreFloat4x4(&identityWvp.view, DirectX::XMMatrixIdentity());
        DirectX::XMStoreFloat4x4(&identityWvp.projection, DirectX::XMMatrixIdentity());
        SetWVP(identityWvp);

	Model::Material material = {
		DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
		DirectX::XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f),
		DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
		nullptr
	};
        SetMaterial(material);
}

void ShaderList::Uninit()
{
	for (int i = 0; i < VS_KIND_MAX; ++i)
	{
		if (m_pVS[i])
		{
			delete m_pVS[i];
			m_pVS[i] = nullptr;
		}
	}
	for (int i = 0; i < PS_KIND_MAX; ++i)
	{
		if (m_pPS[i])
		{
			delete m_pPS[i];
			m_pPS[i] = nullptr;
		}
	}
}

VertexShader* ShaderList::GetVS(VSKind vs)
{
	return m_pVS[vs];
}
PixelShader* ShaderList::GetPS(PSKind ps)
{
	return m_pPS[ps];
}

void ShaderList::SetWVP(const ShaderList::WVPMatrix& wvp)
{
        if (m_pVS[VS_WORLD])
        {
                m_pVS[VS_WORLD]->WriteBuffer(0, &wvp);
        }
}
void ShaderList::SetMaterial(const Model::Material& material)
{
        DirectX::XMFLOAT4 param[1] = {
                material.diffuse,
        };
        for (int i = 0; i < PS_KIND_MAX; ++i)
        {
                m_pPS[i]->SetTexture(0, material.pTexture);
                m_pPS[i]->WriteBuffer(0, param);
        }
}

void ShaderList::SetMaterial(const MaterialDX& material)
{
        DirectX::XMFLOAT4 param[1] = {
                material.GetDiffuse(),
        };
        for (int i = 0; i < PS_KIND_MAX; ++i)
        {
                m_pPS[i]->SetTexture(0, material.GetTexture());
                m_pPS[i]->WriteBuffer(0, param);
        }
}

void ShaderList::MakeWorldVS()
{
        const char* code = R"EOT(
struct VSInput {
        float3 pos    : POSITION;
        float3 normal : NORMAL;
        float2 uv     : TEXCOORD0;
        float4 color  : COLOR0;
};
struct VS_OUT {
        float4 pos : SV_POSITION;
        float2 uv : TEXCOORD0;
        float4 color : COLOR0;
};
cbuffer WVP : register(b0) {
        float4x4 World;
        float4x4 View;
        float4x4 Projection;
};
VS_OUT main(VSInput vin) {
        VS_OUT vout;
        vout.pos = float4(vin.pos, 1.0f);
        vout.pos = mul(vout.pos, World);
        vout.pos = mul(vout.pos, View);
        vout.pos = mul(vout.pos, Projection);
        vout.uv = vin.uv;
        vout.color = vin.color;
        return vout;
})EOT";
        // MeshDX::Vertex layout: POSITION(float3), NORMAL(float3), TEXCOORD(float2), COLOR(float4) = 48 bytes
        m_pVS[VS_WORLD] = new VertexShader();
        m_pVS[VS_WORLD]->Compile(code);
}
void ShaderList::MakeLambertPS()
{
        const char* code = R"EOT(
struct PS_IN {
        float4 pos : SV_POSITION;
        float2 uv : TEXCOORD0;
        float4 color : COLOR0;
};
cbuffer Material : register(b0)
{
        float4 objDiffuse;
};
Texture2D tex : register(t0);
SamplerState samp : register(s0);
float4 main(PS_IN pin) : SV_TARGET
{
        float4 sampled = tex.Sample(samp, pin.uv);
        float4 baseColor = sampled * objDiffuse;
        float3 color = baseColor.rgb * pin.color.rgb;
        return float4(color, baseColor.a);
})EOT";
        m_pPS[PS_LAMBERT] = new PixelShader();
        m_pPS[PS_LAMBERT]->Compile(code);
}

