struct SLANG_ParameterGroup_PS_CONSTANT_BUFFER_std140_0
{
    @align(16) col0_0 : vec4<f32>,
    @align(16) col1_0 : vec4<f32>,
    @align(16) uv_start_0 : vec2<f32>,
    @align(8) uv_end_0 : vec2<f32>,
};

@binding(0) @group(0) var<uniform> PS_CONSTANT_BUFFER_0 : SLANG_ParameterGroup_PS_CONSTANT_BUFFER_std140_0;
fn rcp_0( x_0 : f32) -> f32
{
    return 1.0f / x_0;
}

struct pixelOutput_0
{
    @location(0) output_0 : vec4<f32>,
};

struct pixelInput_0
{
    @location(0) col_0 : vec4<f32>,
    @location(1) uv_0 : vec2<f32>,
};

@fragment
fn main_ps( _S1 : pixelInput_0, @builtin(position) pos_0 : vec4<f32>) -> pixelOutput_0
{
    var _S2 : vec4<f32> = PS_CONSTANT_BUFFER_0.col0_0;
    var delta_0 : vec2<f32> = PS_CONSTANT_BUFFER_0.uv_end_0 - PS_CONSTANT_BUFFER_0.uv_start_0;
    var delta_length_0 : f32 = length(delta_0);
    var col_out_0 : vec4<f32>;
    if(delta_length_0 > 0.00100000004749745f)
    {
        col_out_0 = mix(PS_CONSTANT_BUFFER_0.col0_0, PS_CONSTANT_BUFFER_0.col1_0, vec4<f32>(saturate(dot(delta_0 / vec2<f32>(delta_length_0), _S1.uv_0 - PS_CONSTANT_BUFFER_0.uv_start_0) * rcp_0(delta_length_0))));
    }
    else
    {
        col_out_0 = _S2;
    }
    var _S3 : pixelOutput_0 = pixelOutput_0( col_out_0 );
    return _S3;
}

