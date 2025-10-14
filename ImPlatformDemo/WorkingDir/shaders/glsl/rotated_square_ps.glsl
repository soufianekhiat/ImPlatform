#version 450
layout(row_major) uniform;
layout(row_major) buffer;

#line 34 0
layout(binding = 0)
uniform texture2D texture0_0;


#line 33
layout(binding = 0)
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


#line 36 0
void main()
{

    vec4 col_out_0 = vec4(1.0, 1.0, 1.0, 1.0);


    vec2 _S1 = input_uv_0 - 0.5;

#line 42
    vec2 P_0 = _S1;
    P_0[1] = - _S1.y;

#line 48
    float _S2 = sqrt(2.0);

#line 63
    col_out_0.xyz = vec3(bvec3((min(min(max(abs(_S2 / 2.0 * (P_0.x - P_0.y)), abs(_S2 / 2.0 * (P_0.x + P_0.y))) - 0.28571429848670959, length(P_0 - _S2 / 2.0 * vec2(1.0, -1.0) / 3.5) - 0.28571429848670959), length(P_0 - _S2 / 2.0 * vec2(-1.0, -1.0) / 3.5) - 0.28571429848670959)) < 0.0));


    vec4 _S3 = col_out_0 * (input_col_0 * (texture(sampler2D(texture0_0,sampler0_0), (input_uv_0))));

#line 66
    col_out_0 = _S3;

#line 66
    entryPointParam_main_ps_0 = _S3;

#line 66
    return;
}

