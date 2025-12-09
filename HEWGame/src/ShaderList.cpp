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
        SetLight(DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), DirectX::XMFLOAT3(-1.0f, -1.0f, -1.0f));
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
	DirectX::XMFLOAT4 param[3] = {
		material.diffuse,
		material.ambient,
		material.specular
	};
	param[1].w = material.pTexture ? 1.0f : 0.0f;
	for (int i = 0; i < PS_KIND_MAX; ++i)
	{
		m_pPS[i]->SetTexture(0, material.pTexture);
		m_pPS[i]->WriteBuffer(0, param);
	}
}

void ShaderList::SetMaterial(const MaterialDX& material)
{
	DirectX::XMFLOAT4 param[3] = {
		material.GetDiffuse(),
		material.GetAmbient(),
		material.GetSpecular()
	};
	param[1].w = material.GetTexture() ? 1.0f : 0.0f;
	for (int i = 0; i < PS_KIND_MAX; ++i)
	{
		m_pPS[i]->SetTexture(0, material.GetTexture());
		m_pPS[i]->WriteBuffer(0, param);
	}
}

void ShaderList::SetLight(DirectX::XMFLOAT4 color, DirectX::XMFLOAT3 dir)
{
        DirectX::XMFLOAT4 param[2] = {
                color
        };
        DirectX::XMStoreFloat4(&param[1], DirectX::XMVector3Normalize(
                DirectX::XMVectorSet(dir.x, dir.y, dir.z, 0.0f)
        ));
        m_pPS[PS_LAMBERT]->WriteBuffer(1, param);
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
        float3 normal : NORMAL0;
        float2 uv : TEXCOORD0;
        float4 color : COLOR0;
        float4 wPos : POSITION0;
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
        vout.wPos = vout.pos;
        vout.pos = mul(vout.pos, View);
        vout.pos = mul(vout.pos, Projection);
        vout.normal = mul(vin.normal, (float3x3)World);
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
	float3 normal : NORMAL0;
	float2 uv : TEXCOORD0;
	float4 color : COLOR0;
};
cbuffer Material : register(b0)
{
	float4 objDiffuse;
	float4 objAmbient;
	float4 objSpecular;
};
cbuffer Light : register(b1)
{
	float4 lightDiffuse;
	float4 lightDir;
};
Texture2D tex : register(t0);
SamplerState samp : register(s0);
float4 main(PS_IN pin) : SV_TARGET
{
	float4 color = float4(1.0f, 1.0f, 1.0f, 1.0f);
	if(objAmbient.a >= 1.0f)
		color = tex.Sample(samp, pin.uv);
	float3 N = normalize(pin.normal);
	float3 L = normalize(-lightDir);
	float dotNL = saturate((dot(N, L) + 0.5f) / 1.5f);
	float3 diffuse = objDiffuse.rgb * lightDiffuse.rgb;
	float3 ambient = objAmbient.rgb * lightDiffuse.rgb;
	float3 specular = objSpecular.rgb * lightDiffuse.rgb;
	// {LambertgUˁiv\oȂ̂ō̗p
	// color.rgb *= saturate(diffuse * dotNL + ambient);
	// ŊgU˕̐FςȂ悤lerp(,diffuse,dotNL)ŌvZ
	// ク΍(Z)AΔ(Z)ƂȂ悤ɁAevZ`ŕ
	diffuse *= color.rgb;
	color.rgb = saturate(lerp(
		lerp(diffuse * ambient, diffuse + ambient, pow(ambient, 4.0f)),
		diffuse, dotNL));
	// {ȂKvȂʔˁALambertɎ኱Kp
	color.rgb += specular * pow(saturate(dotNL), max(0.01f, objSpecular.a) * 0.5f) * 0.5f;
	return color;
})EOT";
        m_pPS[PS_LAMBERT] = new PixelShader();
        m_pPS[PS_LAMBERT]->Compile(code);
}
