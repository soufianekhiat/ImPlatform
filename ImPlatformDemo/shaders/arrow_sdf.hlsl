// Arrow drawn from a signed distance field over the quad's UVs.
cbuffer vertexBuffer : register(b0)
{
    float4x4 ProjMtx;
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

float sdArrow(float2 p)
{
    // Arrow head (triangle pointing right)
    float2 head = p - float2(0.5, 0.0);
    float dHead = max(abs(head.y) + head.x * 0.5 - 0.25, -head.x - 0.3);

    // Arrow shaft (rectangle)
    float2 shaft = p - float2(-0.1, 0.0);
    float dShaft = max(abs(shaft.y) - 0.1, max(shaft.x - 0.4, -shaft.x - 0.6));

    return min(dHead, dShaft);
}

float4 ps_main(PS_INPUT input) : SV_Target
{
    float2 uv = input.uv * 2.0 - 1.0;
    float d = sdArrow(uv);
    float alpha = 1.0 - smoothstep(0.0, 0.02, d);
    return float4(1.0, 0.5, 0.2, 1.0) * alpha;
}
