// ImPlatform Shader: Arrow/House Shape
// Creates a geometric arrow or house-like shape using signed distance fields
// Combines diamond shape with rectangular cutout for arrow effect

cbuffer vertexBuffer : register(b0)
{
	float4x4 ProjectionMatrix;
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

PS_INPUT main_vs(VS_INPUT input)
{
	PS_INPUT output;
	output.pos = mul( ProjectionMatrix, float4(input.pos.xy, 0.f, 1.f));
	output.col = input.col;
	output.uv  = input.uv;
	return output;
}

SamplerState sampler0 : register(s0);
Texture2D texture0 : register(t0);

float4 main_ps(PS_INPUT input) : SV_Target
{
	float2 uv = input.uv;
	float4 col_out = float4(1.0f, 1.0f, 1.0f, 1.0f);

	// Center the UV coordinates
	float2 P = uv - 0.5f;
	P.y = -P.y;

	float size = 1.0f;
	float x = P.x;
	float y = P.y;

	// Calculate three distance fields:
	// r1: Diamond shape (Manhattan distance from center)
	float r1 = abs( x ) + abs( y ) - size / 2.0f;

	// r2: Left half box (creates the notch/cutout on the left side)
	float r2 = max( abs( x + size / 2.0f ), abs( y ) ) - size / 2;

	// r3: Small rectangle in the center (shaft of arrow)
	float r3 = max( abs( x - size / 6.0f ) - size / 4.0f, abs( y ) - size / 4.0f );

	// Combine the shapes:
	// - Take the intersection of 0.75*r1 (scaled diamond) and r2 (with cutout)
	// - Then take minimum with r3 (union with center rectangle)
	// Result is white where distance is negative (inside the shape)
	col_out.rgb = ( min( r3, max( .75f * r1, r2 ) ) < 0.0f ).xxx;

	// Multiply with texture
	col_out *= input.col * texture0.Sample( sampler0, input.uv );

	return col_out;
}
