#version 130
#define IM_SHADER_GLSL
#ifndef __IM_SHADER_H__
#define __IM_SHADER_H__

#if defined(IM_SHADER_HLSL)
#define IMS_IN        in
#define IMS_OUT       out
#define IMS_INOUT     inout
#define IMS_UNIFORM   uniform
#define IMS_CBUFFER   cbuffer
#elif defined(IM_SHADER_GLSL)
#define IMS_IN        in
#define IMS_OUT       out
#define IMS_INOUT     inout
#define IMS_UNIFORM   const
#define IMS_CBUFFER   uniform
#endif

#if defined(IM_SHADER_HLSL)

#define Mat44f matrix<float, 4, 4>
#define Mat33f matrix<float, 3, 3>

#endif

#if defined(IM_SHADER_GLSL)

#define Mat44f   mat4
#define Mat33f   mat3

#define float2   vec2
#define float3   vec3
#define float4   vec4
#define uint2    uvec2
#define uint3    uvec3
#define uint4    uvec4
#define int2     ivec2
#define int3     ivec3
#define int4     ivec4

#define float4x4 mat4
#define float3x3 mat3
#define float2x2 mat2

#endif
#endif


struct PS_INPUT
{
	//float4 pos : SV_POSITION;
	//float4 col : COLOR0;
	//float2 uv  : TEXCOORD0;
float4 pos : SV_POSITION;
float4 col : COLOR0;
float2 uv  : TEXCOORD0;

};

IMS_CBUFFER PS_CONSTANT_BUFFER
{
float4 col0;
		float4 col1;
		float2 uv_start;
		float2 uv_end;

};



sampler sampler0;
Texture2D texture0;

float4 main_ps(PS_INPUT input) : SV_Target
{
	float2 uv = input.uv.xy;
	float4 col_in = input.col;
	float4 col_out = float4(1.0f, 1.0f, 1.0f, 1.0f);
	float2 delta = uv_end - uv_start;
		float2 d = normalize( delta );
		float l = rcp( length( delta ) );
		float2 c = uv - uv_start;
		float t = saturate( dot( d, c ) * l );
		col_out = lerp( col0, col1, t );

	col_out *= col_in * texture0.Sample( sampler0, input.uv );
	return col_out;
}


