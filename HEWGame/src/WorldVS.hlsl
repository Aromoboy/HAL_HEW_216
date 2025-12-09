struct VSInput {
    float3 pos    : POSITION;
    float3 normal : NORMAL;
    float2 uv     : TEXCOORD0;
    float4 color  : COLOR0;
};

struct VSOutput {
    float4 pos   : SV_POSITION;
    float3 normal: NORMAL0;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR0;
    float4 wPos  : POSITION0;
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
    vout.wPos = vout.pos;
    vout.pos = mul(vout.pos, view);
    vout.pos = mul(vout.pos, proj);
    vout.normal = mul(vin.normal, (float3x3)world);
    vout.uv = vin.uv;
    vout.color = vin.color;
    return vout;
}
