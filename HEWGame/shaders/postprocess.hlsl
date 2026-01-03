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
SamplerState samp : register(s0);

cbuffer PostProcessParams : register(b0)
{
    float gamma;
    float saturation;
    float contrast;
    float padding;
};

float4 PSMain(VSOut input) : SV_TARGET
{
    float3 col = sceneTex.Sample(samp, input.uv).rgb;
    float gray = dot(col, float3(0.299, 0.587, 0.114));
    col = lerp(float3(gray, gray, gray), col, saturation);
    col = (col - 0.5) * contrast + 0.5;
    col = saturate(col);
    return float4(col, 1.0f);
}
