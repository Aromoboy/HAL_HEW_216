struct PSInput
{
    float4 pos    : SV_POSITION;
    float3 normal : NORMAL0;
    float2 uv     : TEXCOORD0;
    float4 color  : COLOR0;
    float4 wPos   : POSITION0;
};

cbuffer Material : register(b0)
{
    float4 objDiffuse;
    float4 objAmbient;
    float4 objSpecular;
};

cbuffer Light : register(b1)
{
    float4 lightColor;
    float4 lightDir;
};

cbuffer Camera : register(b2)
{
    float4 cameraPos;
};

Texture2D diffuseMap   : register(t0);
Texture2D normalMap    : register(t1);
Texture2D roughnessMap : register(t2);
Texture2D metallicMap  : register(t3);
Texture2D aoMap        : register(t4);
SamplerState sampler0  : register(s0);

static const float PI = 3.14159265f;

float3 ApplyNormalMap(float3 sampledNormal, float3 inputNormal)
{
    float3 mapNormal = normalize(sampledNormal * 2.0f - 1.0f);
    float3 blended = normalize(mapNormal + inputNormal);
    return blended;
}

float DistributionGGX(float NdotH, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float denom = (NdotH * NdotH) * (a2 - 1.0f) + 1.0f;
    return a2 / max(denom * denom * PI, 1e-4f);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return NdotV / max(NdotV * (1.0f - k) + k, 1e-4f);
}

float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    float ggx1 = GeometrySchlickGGX(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(1.0f - cosTheta, 5.0f);
}

float4 main(PSInput input) : SV_TARGET
{
    float4 albedoSample = diffuseMap.Sample(sampler0, input.uv) * float4(objDiffuse.rgb, 1.0f);
    float3 albedo = albedoSample.rgb * input.color.rgb;

    float roughness = saturate(roughnessMap.Sample(sampler0, input.uv).r);
    float metallic = saturate(metallicMap.Sample(sampler0, input.uv).r);
    float ao = saturate(aoMap.Sample(sampler0, input.uv).r);

    float3 normalSample = normalMap.Sample(sampler0, input.uv).xyz;
    float3 N = normalize(ApplyNormalMap(normalSample, input.normal));
    float3 V = normalize(cameraPos.xyz - input.wPos.xyz);
    float3 L = normalize(-lightDir.xyz);
    float3 H = normalize(L + V);

    float NdotL = saturate(dot(N, L));
    float NdotV = saturate(dot(N, V));
    float NdotH = saturate(dot(N, H));
    float VdotH = saturate(dot(V, H));

    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
    float3 F = FresnelSchlick(VdotH, F0);
    float D = DistributionGGX(NdotH, roughness);
    float G = GeometrySmith(NdotV, NdotL, roughness);

    float3 numerator = D * G * F;
    float denom = max(4.0f * NdotV * NdotL, 1e-4f);
    float3 specular = numerator / denom;

    float3 kS = F;
    float3 kD = (1.0f - kS) * (1.0f - metallic);

    float3 diffuse = kD * albedo / PI;
    float3 radiance = lightColor.rgb;
    float3 lighting = (diffuse + specular) * radiance * NdotL;

    float3 ambient = albedo * objAmbient.rgb * ao;
    float3 color = ambient + lighting;

    color = saturate(color);
    return float4(color, albedoSample.a);
}
