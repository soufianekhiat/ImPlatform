// --compute-test: copies the input's red and green, blue from the constants (b0), alpha 1.
Texture2D<float4> inTex : register(t0);
RWTexture2D<unorm float4> outTex : register(u0);
cbuffer Params : register(b0)
{
    float4 p;
};

[numthreads(8, 8, 1)]
void cs_main(uint3 id : SV_DispatchThreadID)
{
    float4 c = inTex.Load(int3(id.xy, 0));
    outTex[id.xy] = float4(c.r, c.g, p.z, 1.0);
}
