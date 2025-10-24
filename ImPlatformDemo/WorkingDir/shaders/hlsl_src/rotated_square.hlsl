// ImPlatform Shader: Rotated Square with Rounded Corners
// Creates a diamond/square shape rotated 45 degrees with circular rounded corners
// Uses signed distance field (SDF) operations for smooth anti-aliased edges

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

	// Rotate by 45 degrees (sqrt(2)/2 = cos(45°) = sin(45°))
	float x = sqrt(2.0f) / 2.0f * ( P.x - P.y );
	float y = sqrt(2.0f) / 2.0f * ( P.x + P.y );

	// Calculate three distance fields:
	// r1: Rotated square (using max of abs values)
	float r1 = max( abs( x ), abs( y ) ) - size / 3.5f;

	// r2: Circle at top-right corner
	float r2 = length( P - sqrt( 2.0f ) / 2.0f * float2( +1.0f, -1.0f ) * size / 3.5f ) - size / 3.5f;

	// r3: Circle at top-left corner
	float r3 = length( P - sqrt( 2.0f ) / 2.0f * float2( -1.0f, -1.0f ) * size / 3.5f ) - size / 3.5f;

	// Combine all three distance fields (min creates the union)
	// Result is white where distance is negative (inside the shape)
	col_out.rgb = ( min( min( r1, r2 ), r3 ) < 0.0f ).xxx;

	// Multiply with texture
	col_out *= input.col * texture0.Sample( sampler0, input.uv );

	return col_out;
}
