// Vertical gradient between two colors (b1).
cbuffer vertexBuffer : register(b0)
{
    float4x4 ProjMtx;
};
cbuffer pixelBuffer : register(b1)
{
    float4 ColorStart;
    float4 ColorEnd;
};

struct VS_INPUT
{
    float2 pos : POSITION;
    float4 col : COLOR0;
    float2 uv  : TEXCOORD0;
};
struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
    float2 uv  : TEXCOORD0;
};

PS_INPUT vs_main(VS_INPUT input)
{
    PS_INPUT output;
    output.pos = mul(ProjMtx, float4(input.pos.xy, 0.0, 1.0));
    output.col = input.col;
    output.uv = input.uv;
    return output;
}

float4 ps_main(PS_INPUT input) : SV_Target
{
    return lerp(ColorStart, ColorEnd, input.uv.y);
}
