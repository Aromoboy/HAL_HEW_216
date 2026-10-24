#include "common.hlsl"

//--------------------------------------------------------------------------------------
Texture2D    txYUV        : register(t0);
SamplerState samLinear    : register(s0);

//--------------------------------------------------------------------------------------
// Nothing special here
v2f VS(VS_INPUT input) {
  v2f output;
  output.Pos = mul(input.Pos, World);
  output.Pos = mul(output.Pos, View);
  output.Pos = mul(output.Pos, Projection);
  output.Tex = input.Tex;
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
  uint width, height;
  txYUV.GetDimensions(width, height);
  float2 halfTexel = 0.5 / float2(width, height);
  // ミカ：各平面の画素中心内に制限し、隣の平面と未使用領域の色が端へ混ざるのを防ぐ
  float2 yCoord = clamp(float2(input.Tex.x, input.Tex.y * 0.5),
      halfTexel, float2(1.0, 0.5) - halfTexel);
  float2 uCoord = clamp(float2(input.Tex.x * 0.5, 0.50 + input.Tex.y * 0.25),
      float2(0.0, 0.50) + halfTexel, float2(0.5, 0.75) - halfTexel);
  float2 vCoord = clamp(float2(input.Tex.x * 0.5, 0.75 + input.Tex.y * 0.25),
      float2(0.0, 0.75) + halfTexel, float2(0.5, 1.0) - halfTexel);
  float y = txYUV.Sample(samLinear, yCoord).r;
  float u = txYUV.Sample(samLinear, uCoord).r;
  float v = txYUV.Sample(samLinear, vCoord).r;
  return float4(YUVToRGB(float3(y,u,v)), 1.f);
}
