#include "common.hlsl"

//--------------------------------------------------------------------------------------
Texture2D    txLuma       : register(t0);
Texture2D    txChroma     : register(t1);
SamplerState samLinear    : register(s0);

//--------------------------------------------------------------------------------------
cbuffer VideoUvParam : register(b1)
{
  float2 uvPos;
  float2 uvScale;
}

//--------------------------------------------------------------------------------------
// Nothing special here
v2f VS(VS_INPUT input) {
  v2f output;
  output.Pos = mul(input.Pos, World);
  output.Pos = mul(output.Pos, View);
  output.Pos = mul(output.Pos, Projection);
  output.Tex = input.Tex * uvScale + uvPos;
  output.Color = input.Color;
  return output;
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------

float3 YUVToRGB(float3 yuv) {
  // BT.601 coefs
  static const float3 yuvCoef_r = { 1.164f, 0.000f, 1.596f };
  static const float3 yuvCoef_g = { 1.164f, -0.392f, -0.813f };
  static const float3 yuvCoef_b = { 1.164f, 2.017f, 0.000f };
  yuv -= float3(0.0625f, 0.5f, 0.5f);
  return saturate(float3( 
    dot( yuv, yuvCoef_r ),
    dot( yuv, yuvCoef_g ),
    dot( yuv, yuvCoef_b )
    ));
}

float4 PS(v2f input) : SV_Target {
  float2 uv = saturate(input.Tex);

  // NV12 path: Y in t0.r, UV in t1.rg.
  float y = txLuma.Sample(samLinear, uv).r;
  float2 uvSample = txChroma.Sample(samLinear, uv).rg;
  float u = uvSample.x;
  float v = uvSample.y;

  // Legacy fallback: IYUV packed into single R8 texture in t0.
  if (u == 0.0f && v == 0.0f)
  {
    y = txLuma.Sample(samLinear, float2(uv.x, uv.y * 0.5)).r;
    u = txLuma.Sample(samLinear, float2(uv.x * 0.5, 0.50 + uv.y * 0.25)).r;
    v = txLuma.Sample(samLinear, float2(uv.x * 0.5, 0.75 + uv.y * 0.25)).r;
  }

  return float4(YUVToRGB(float3(y, u, v)), 1.f);
}
