struct VSOut
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOut VSMain(uint vertexId : SV_VertexID)
{
    VSOut output;

    float2 positions[3] = {
        float2(-1.0f, -1.0f),
        float2(-1.0f,  3.0f),
        float2( 3.0f, -1.0f)
    };

    float2 uvs[3] = {
        float2(0.0f, 1.0f),
        float2(0.0f, -1.0f),
        float2(2.0f, 1.0f)
    };

    output.position = float4(positions[vertexId], 0.0f, 1.0f);
    output.uv = uvs[vertexId];
    return output;
}

Texture2D sceneTex : register(t0);
Texture2D depthTex : register(t1);
SamplerState samp : register(s0);

cbuffer PostProcessParams : register(b0)
{
    float gamma;
    float saturation;
    float contrast;
    float padding;
    float4 fogColor;
    float fogStart;
    float fogRange;
    float cameraNear;
    float cameraFar;
    float vignetteIntensity;
    float microBloomIntensity;
    float2 padding2;
};

float LinearizeDepth(float z)
{
    return (cameraNear * cameraFar) / (cameraFar - z * (cameraFar - cameraNear));
}

float4 PSMain(VSOut input) : SV_TARGET
{
    float3 col = sceneTex.Sample(samp, input.uv).rgb;
    float depth = depthTex.Sample(samp, input.uv).r;
    float gray = dot(col, float3(0.299, 0.587, 0.114));
    col = lerp(float3(gray, gray, gray), col, saturation);
    col = (col - 0.5) * contrast + 0.5;
    col = saturate(col);
    float linearDepth = LinearizeDepth(depth);
    float fog = saturate((linearDepth - fogStart) / fogRange);
    col = lerp(col, fogColor.rgb, fog);
    float2 d = input.uv - float2(0.5, 0.5);
    float vignette = 1.0 - dot(d, d) * vignetteIntensity;
    col *= saturate(vignette);
    float luminance = dot(col, float3(0.2126, 0.7152, 0.0722));
    col += max(luminance - 0.8, 0.0) * microBloomIntensity;
    col = saturate(col);
    return float4(col, 1.0f);
}
