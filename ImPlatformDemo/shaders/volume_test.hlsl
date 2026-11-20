// 3D texture test (--texture3d-test): a 2x2x2 texture at t1 sampled at two voxel centers, the left half of the quad
// showing voxel (0, 1, 1) and the right half voxel (1, 0, 0), so a swapped axis or a missing slice shows.
cbuffer vertexBuffer : register(b0)
{
    float4x4 ProjMtx;
};

SamplerState sampler0 : register(s0);
Texture3D volume : register(t1);

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
    float3 uvw = input.uv.x < 0.5 ? float3(0.25, 0.75, 0.75) : float3(0.75, 0.25, 0.25);
    return volume.SampleLevel(sampler0, uvw, 0.0);
}
