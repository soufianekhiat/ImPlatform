// ImPlatform Shader: Linear Gradient
// Creates a smooth linear gradient between two colors along a specified direction
// Uses UV start and end points to define gradient direction and extent

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

// Pixel shader constant buffer for gradient parameters
// Note: Uses register b1 to avoid conflict with vertex shader's projection matrix at b0
cbuffer PS_CONSTANT_BUFFER : register(b1)
{
	float4 col0;        // Start color of gradient
	float4 col1;        // End color of gradient
	float2 uv_start;    // UV coordinate where gradient starts
	float2 uv_end;      // UV coordinate where gradient ends
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
	float4 col_out = col0;  // Default to start color

	// Calculate gradient vector from start to end
	float2 delta = uv_end - uv_start;
	float delta_length = length( delta );

	// Only calculate gradient if there's a meaningful distance between start and end
	if ( delta_length > 0.001f )
	{
		// Normalize the gradient direction
		float2 d = delta / delta_length;

		// Calculate reciprocal of gradient length for interpolation
		float l = rcp( delta_length );

		// Vector from gradient start to current UV position
		float2 c = uv - uv_start;

		// Calculate interpolation factor t by projecting c onto d
		// t = 0.0 at uv_start, t = 1.0 at uv_end
		// saturate() clamps t to [0, 1] range
		float t = saturate( dot( d, c ) * l );

		// Linear interpolation between col0 and col1
		col_out = lerp( col0, col1, t );
	}

	return col_out;
}
