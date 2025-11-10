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
	MakeAnimeVS();
	MakeLambertPS();
	MakeSpecularPS();
	MakeToonPS();
	MakeFogPS();

	DirectX::XMFLOAT4X4 mat[250];
	for (int i = 0; i < 250; ++i)
	{
		DirectX::XMStoreFloat4x4(&mat[i], DirectX::XMMatrixIdentity());
	}
	SetWVP(mat);
	SetBones(mat);

	Model::Material material = {
		DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
		DirectX::XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f),
		DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
		nullptr
	};
	SetMaterial(material);
	SetLight(DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), DirectX::XMFLOAT3(-1.0f, -1.0f, -1.0f));
	SetCameraPos(DirectX::XMFLOAT3(0.0f, 1.0f, -2.0f));
	SetFog(DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), 50.0f, 50.0f);
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

void ShaderList::SetWVP(DirectX::XMFLOAT4X4* wvp)
{
	for (int i = 0; i < VS_KIND_MAX; ++i)
	{
		m_pVS[i]->WriteBuffer(0, wvp);
	}
}
void ShaderList::SetBones(DirectX::XMFLOAT4X4* bones200)
{
	m_pVS[VS_ANIME]->WriteBuffer(1, bones200);
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
void ShaderList::SetLight(DirectX::XMFLOAT4 color, DirectX::XMFLOAT3 dir)
{
	DirectX::XMFLOAT4 param[2] = {
		color
	};
	DirectX::XMStoreFloat4(&param[1], DirectX::XMVector3Normalize(
		DirectX::XMVectorSet(dir.x, dir.y, dir.z, 0.0f)
	));
	for (int i = 0; i < PS_KIND_MAX; ++i)
	{
		m_pPS[i]->WriteBuffer(1, param);
	}
}
void ShaderList::SetCameraPos(const DirectX::XMFLOAT3 pos)
{
	DirectX::XMFLOAT4 param[] = {
		{pos.x, pos.y, pos.z, 0.0f}
	};
	m_pPS[PS_SPECULAR]->WriteBuffer(2, param);
	m_pPS[PS_TOON]->WriteBuffer(2, param);
}
void ShaderList::SetFog(DirectX::XMFLOAT4 color, float start, float range)
{
	DirectX::XMFLOAT4 param[] = {
		color,
		{start, range, 0.0f, 0.0f}
	};
	m_pPS[PS_FOG]->WriteBuffer(3, param);
}

void ShaderList::MakeWorldVS()
{
	const char* code = R"EOT(
struct VS_IN {
	float3 pos : POSITION;
	float3 normal : NORMAL0;
	float2 uv : TEXCOORD0;
	float4 color : COLOR0;
};
struct VS_OUT {
	float4 pos : SV_POSITION;
	float3 normal : NORMAL0;
	float2 uv : TEXCOORD0;
	float4 color : COLOR0;
	float4 wPos : POSITION0;
};
cbuffer WVP : register(b0) {
	float4x4 world;
	float4x4 view;
	float4x4 proj;
};
VS_OUT main(VS_IN vin) {
	VS_OUT vout;
	vout.pos = float4(vin.pos, 1.0f);
	vout.pos = mul(vout.pos, world);
	vout.wPos = vout.pos;
	vout.pos = mul(vout.pos, view);
	vout.pos = mul(vout.pos, proj);
	vout.normal = mul(vin.normal, (float3x3)world);
	vout.uv = vin.uv;
	vout.color = vin.color;
	return vout;
})EOT";
	m_pVS[VS_WORLD] = new VertexShader();
	m_pVS[VS_WORLD]->Compile(code);
}
void ShaderList::MakeAnimeVS()
{
	const char* code = R"EOT(
struct VS_IN {
	float3 pos : POSITION;
	float3 normal : NORMAL0;
	float2 uv : TEXCOORD0;
	float4 color : COLOR0;
	float4 weight : WEIGHT0;
	uint4 index : INDEX0;
};
struct VS_OUT {
	float4 pos : SV_POSITION;
	float3 normal : NORMAL0;
	float2 uv : TEXCOORD0;
	float4 color : COLOR0;
	float4 wPos : POSITION0;
};
cbuffer WVP : register(b0) {
	float4x4 world;
	float4x4 view;
	float4x4 proj;
};
cbuffer Bone : register(b1) {
	float4x4 bone[200];
};
VS_OUT main(VS_IN vin) {
	VS_OUT vout;
	float4x4 anime;
	anime  = bone[vin.index.x] * vin.weight.x;
	anime += bone[vin.index.y] * vin.weight.y;
	anime += bone[vin.index.z] * vin.weight.z;
	anime += bone[vin.index.w] * vin.weight.w;
	vout.pos = float4(vin.pos, 1.0f);
	vout.pos = mul(vout.pos, anime);
	vout.pos = mul(vout.pos, world);
	vout.wPos = vout.pos;
	vout.pos = mul(vout.pos, view);
	vout.pos = mul(vout.pos, proj);
	vout.normal = vin.normal;
	vout.normal = mul(vout.normal, (float3x3)anime);
	vout.normal = mul(vout.normal, (float3x3)world);
	vout.uv = vin.uv;
	vout.color = vin.color;
	return vout;
})EOT";
	m_pVS[VS_ANIME] = new VertexShader();
	m_pVS[VS_ANIME]->Compile(code);
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
	// 本来のLambert拡散反射（思った表現が出来なかったので採用せず
	// color.rgb *= saturate(diffuse * dotNL + ambient);
	// 環境光で拡散反射部分の色が変わらないようにlerp(環境光,diffuse,dotNL)で計算
	// 環境光が弱ければ黒(乗算)、強ければ白(加算)となるように、各計算を線形で補間
	diffuse *= color.rgb;
	color.rgb = saturate(lerp(
		lerp(diffuse * ambient, diffuse + ambient, pow(ambient, 4.0f)),
		diffuse, dotNL));
	// 本来なら必要ない鏡面反射、Lambert向けに若干だけ適用
	color.rgb += specular * pow(saturate(dotNL), max(0.01f, objSpecular.a) * 0.5f) * 0.5f;
	return color;
})EOT";
	m_pPS[PS_LAMBERT] = new PixelShader();
	m_pPS[PS_LAMBERT]->Compile(code);
}
void ShaderList::MakeSpecularPS()
{
	const char* code = R"EOT(
struct PS_IN {
	float4 pos : SV_POSITION;
	float3 normal : NORMAL0;
	float2 uv : TEXCOORD0;
	float4 color : COLOR0;
	float4 wPos : POSITION0;
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
cbuffer Camera : register(b2)
{
	float4 cameraPos;
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
	float3 V = normalize(cameraPos.xyz - pin.wPos.xyz);
	float3 R = reflect(-V, N);
	float dotNL = saturate((dot(N, L) + 0.5f) / 1.5f);
	float dotRL = saturate(dot(R, L));
	float3 diffuse = objDiffuse.rgb * lightDiffuse.rgb;
	float3 ambient = objAmbient.rgb * lightDiffuse.rgb;
	float3 specular = objSpecular.rgb * lightDiffuse.rgb;
	// Lambertの計算を参考
	color.rgb *= saturate(lerp(
		lerp(diffuse * ambient, diffuse + ambient, pow(ambient, 4.0f)),
		diffuse, dotNL));
	color.rgb += specular * saturate(pow(dotRL, max(0.01f, objSpecular.a)));
	return color;
})EOT";
	m_pPS[PS_SPECULAR] = new PixelShader();
	m_pPS[PS_SPECULAR]->Compile(code);
}
void ShaderList::MakeToonPS()
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
	float dotNL = dot(N, L); // マイナス込で計算
	float3 diffuse = objDiffuse.rgb * lightDiffuse.rgb;
	float3 ambient = objAmbient.rgb * lightDiffuse.rgb;
	float3 specular = objSpecular.rgb * lightDiffuse.rgb;
	float toonNL = saturate((dot(N, L) + 0.5f) / 1.5f * 100.0f); // 陰の境目を柔らかく
	// Lambertの計算を参考
	color.rgb *= saturate(lerp(
		lerp(diffuse * ambient, diffuse + ambient, pow(ambient, 4.0f)),
		diffuse, toonNL));
	if(objSpecular.a >= 1.0f)
		color.rgb += specular * saturate(pow(dotNL, max(0.01f, objSpecular.a)) * 100.0f);
	return color;
})EOT";
	m_pPS[PS_TOON] = new PixelShader();
	m_pPS[PS_TOON]->Compile(code);
}
void ShaderList::MakeFogPS()
{
	const char* code = R"EOT(
struct PS_IN {
	float4 pos : SV_POSITION;
	float3 normal : NORMAL0;
	float2 uv : TEXCOORD0;
	float4 color : COLOR0;
	float4 wPos : POSITION0;
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
cbuffer Camera : register(b2)
{
	float4 cameraPos;
};
cbuffer Param : register(b3)
{
	float4 fogColor;
	float fogStart;
	float fogRange;
	float2 dummy;
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
	color.rgb *= saturate(diffuse * dotNL + ambient);
	float vLen = length(pin.wPos.xyz - cameraPos.xyz);
	color.rgb = lerp(color.rgb, fogColor.rgb, saturate((vLen - fogStart) / fogRange));
	return color;
})EOT";
	m_pPS[PS_FOG] = new PixelShader();
	m_pPS[PS_FOG]->Compile(code);
}