struct VSInput {
    float3 pos    : POSITION;
    float2 uv     : TEXCOORD0;
};

struct VSOutput {
    float4 pos   : SV_POSITION;
    float2 uv    : TEXCOORD0;
};

cbuffer WVP : register(b0)
{
    float4x4 world;
    float4x4 view;
    float4x4 proj;
};

VSOutput main(VSInput vin)
{
    VSOutput vout;
    vout.pos = float4(vin.pos, 1.0f);
    vout.pos = mul(vout.pos, world);
    vout.pos = mul(vout.pos, view);
    vout.pos = mul(vout.pos, proj);
    vout.uv = vin.uv;
    return vout;
}
