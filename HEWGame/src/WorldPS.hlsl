struct PSInput
{
    float4 pos    : SV_POSITION;
    float2 uv     : TEXCOORD0;
    float4 color  : COLOR0;
};

cbuffer Material : register(b0)
{
    float4 objDiffuse;
};

Texture2D diffuseMap   : register(t0);
SamplerState sampler0  : register(s0);

float4 main(PSInput input) : SV_TARGET
{
    float4 sampled = diffuseMap.Sample(sampler0, input.uv);
    float4 baseColor = sampled * objDiffuse;
    float3 color = baseColor.rgb * input.color.rgb;
    return float4(color, baseColor.a);
}
