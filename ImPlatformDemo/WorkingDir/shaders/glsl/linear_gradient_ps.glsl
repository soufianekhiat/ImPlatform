#version 450
layout(row_major) uniform;
layout(row_major) buffer;

#line 31 0
struct SLANG_ParameterGroup_PS_CONSTANT_BUFFER_std140_0
{
    vec4 col0_0;
    vec4 col1_0;
    vec2 uv_start_0;
    vec2 uv_end_0;
};


#line 26
layout(binding = 0)
layout(std140) uniform block_SLANG_ParameterGroup_PS_CONSTANT_BUFFER_std140_0
{
    vec4 col0_0;
    vec4 col1_0;
    vec2 uv_start_0;
    vec2 uv_end_0;
}PS_CONSTANT_BUFFER_0;

#line 12038 1
float rcp_0(float x_0)
{

#line 12044
    return 1.0 / x_0;
}


#line 12386
float saturate_0(float x_1)
{

#line 12394
    return clamp(x_1, 0.0, 1.0);
}


#line 10319
layout(location = 0)
out vec4 entryPointParam_main_ps_0;


#line 10319
layout(location = 1)
in vec2 input_uv_0;


#line 46 0
void main()
{

    vec4 _S1 = PS_CONSTANT_BUFFER_0.col0_0;


    vec2 delta_0 = PS_CONSTANT_BUFFER_0.uv_end_0 - PS_CONSTANT_BUFFER_0.uv_start_0;
    float delta_length_0 = length(delta_0);

#line 53
    vec4 col_out_0;


    if(delta_length_0 > 0.00100000004749745)
    {

#line 56
        col_out_0 = mix(PS_CONSTANT_BUFFER_0.col0_0, PS_CONSTANT_BUFFER_0.col1_0, vec4(saturate_0(dot(delta_0 / delta_length_0, input_uv_0 - PS_CONSTANT_BUFFER_0.uv_start_0) * rcp_0(delta_length_0))));

#line 56
    }
    else
    {

#line 56
        col_out_0 = _S1;

#line 56
    }

#line 56
    entryPointParam_main_ps_0 = col_out_0;

#line 56
    return;
}

