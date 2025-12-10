struct PSInput
{
    float4 pos    : SV_POSITION;
    float2 uv     : TEXCOORD0;
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
SamplerState sampler0  : register(s0);

float4 main(PSInput input) : SV_TARGET
{
    float4 albedoSample = diffuseMap.Sample(sampler0, input.uv) * float4(objDiffuse.rgb, 1.0f);
    float3 lit = (albedoSample.rgb + objAmbient.rgb) * lightColor.rgb;
    return float4(saturate(lit), albedoSample.a * objDiffuse.a);
}
