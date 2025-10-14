#version 450
layout(row_major) uniform;
layout(row_major) buffer;

#line 31 0
layout(binding = 2)
uniform texture2D texture0_0;


#line 30
layout(binding = 1)
uniform sampler sampler0_0;


#line 1779 1
layout(location = 0)
out vec4 entryPointParam_main_ps_0;


#line 1779
layout(location = 0)
in vec4 input_col_0;


#line 1779
layout(location = 1)
in vec2 input_uv_0;


#line 33 0
void main()
{

#line 33
    entryPointParam_main_ps_0 = input_col_0 * (texture(sampler2D(texture0_0,sampler0_0), (input_uv_0)));

#line 33
    return;
}

