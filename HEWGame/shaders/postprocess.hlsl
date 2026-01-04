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
    float bloomThreshold;
    float padding2;
    float2 invScreenSize;
    float fxaaEdgeThreshold;
    float fxaaEdgeThresholdMin;
    float fxaaSubpixelQuality;
    int fxaaEnable;
    float postTime;
    float grainAmount;
    int grainEnable;
    int grainAnimate;
};

float LinearizeDepth(float z)
{
    return (cameraNear * cameraFar) / (cameraFar - z * (cameraFar - cameraNear));
}

float FxaaLuma(float3 rgb)
{
    return dot(rgb, float3(0.299, 0.587, 0.114));
}

float3 FxaaPixelShader(float2 uv)
{
    float2 texel = invScreenSize;
    float3 rgbM = sceneTex.Sample(samp, uv).rgb;
    float3 rgbN = sceneTex.Sample(samp, uv + float2(0.0f, -texel.y)).rgb;
    float3 rgbS = sceneTex.Sample(samp, uv + float2(0.0f, texel.y)).rgb;
    float3 rgbW = sceneTex.Sample(samp, uv + float2(-texel.x, 0.0f)).rgb;
    float3 rgbE = sceneTex.Sample(samp, uv + float2(texel.x, 0.0f)).rgb;
    float3 rgbNW = sceneTex.Sample(samp, uv + float2(-texel.x, -texel.y)).rgb;
    float3 rgbNE = sceneTex.Sample(samp, uv + float2(texel.x, -texel.y)).rgb;
    float3 rgbSW = sceneTex.Sample(samp, uv + float2(-texel.x, texel.y)).rgb;
    float3 rgbSE = sceneTex.Sample(samp, uv + float2(texel.x, texel.y)).rgb;

    float lumaM = FxaaLuma(rgbM);
    float lumaMin = min(lumaM, min(min(FxaaLuma(rgbN), FxaaLuma(rgbS)), min(FxaaLuma(rgbW), FxaaLuma(rgbE))));
    float lumaMax = max(lumaM, max(max(FxaaLuma(rgbN), FxaaLuma(rgbS)), max(FxaaLuma(rgbW), FxaaLuma(rgbE))));
    lumaMin = min(lumaMin, min(min(FxaaLuma(rgbNW), FxaaLuma(rgbNE)), min(FxaaLuma(rgbSW), FxaaLuma(rgbSE))));
    lumaMax = max(lumaMax, max(max(FxaaLuma(rgbNW), FxaaLuma(rgbNE)), max(FxaaLuma(rgbSW), FxaaLuma(rgbSE))));

    float lumaRange = lumaMax - lumaMin;
    float lumaThreshold = max(fxaaEdgeThresholdMin, lumaMax * fxaaEdgeThreshold);
    if (lumaRange < lumaThreshold)
    {
        return rgbM;
    }

    float lumaNW = FxaaLuma(rgbNW);
    float lumaNE = FxaaLuma(rgbNE);
    float lumaSW = FxaaLuma(rgbSW);
    float lumaSE = FxaaLuma(rgbSE);

    float2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y = ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25f * fxaaSubpixelQuality), 1.0f / 128.0f);
    float rcpDirMin = 1.0f / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, -8.0f, 8.0f) * texel;

    float3 rgbA = 0.5f * (
        sceneTex.Sample(samp, uv + dir * (1.0f / 3.0f - 0.5f)).rgb +
        sceneTex.Sample(samp, uv + dir * (2.0f / 3.0f - 0.5f)).rgb);
    float3 rgbB = rgbA * 0.5f + 0.25f * (
        sceneTex.Sample(samp, uv + dir * -0.5f).rgb +
        sceneTex.Sample(samp, uv + dir * 0.5f).rgb);

    float lumaB = FxaaLuma(rgbB);
    if (lumaB < lumaMin || lumaB > lumaMax)
    {
        return rgbA;
    }

    return rgbB;
}

float4 PSMain(VSOut input) : SV_TARGET
{
    float3 col = sceneTex.Sample(samp, input.uv).rgb;
    if (fxaaEnable != 0)
    {
        col = FxaaPixelShader(input.uv);
    }
    float depth = depthTex.Sample(samp, input.uv).r;
    float gray = FxaaLuma(col);
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
    col += max(luminance - bloomThreshold, 0.0) * microBloomIntensity;
    col = saturate(col);
    if (grainEnable != 0)
    {
        float timeOffset = (grainAnimate != 0) ? postTime : 0.0f;
        float noise = frac(sin(dot(input.position.xy + timeOffset, float2(12.9898f, 78.233f))) * 43758.5453f);
        float grain = (noise - 0.5f) * 2.0f * grainAmount;
        col = saturate(col + grain);
    }
    return float4(col, 1.0f);
}
