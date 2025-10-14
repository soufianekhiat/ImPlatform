@binding(0) @group(0) var texture0_0 : texture_2d<f32>;

@binding(0) @group(0) var sampler0_0 : sampler;

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
    var col_out_0 : vec4<f32> = vec4<f32>(1.0f, 1.0f, 1.0f, 1.0f);
    var _S2 : vec2<f32> = _S1.uv_0 - vec2<f32>(0.5f);
    var P_0 : vec2<f32> = _S2;
    P_0[i32(1)] = - _S2.y;
    var x_0 : f32 = P_0.x;
    var _S3 : f32 = abs(P_0.y);
    var _S4 : vec3<f32> = vec3<f32>(vec3<bool>(((min(max(abs(x_0 - 0.1666666716337204f) - 0.25f, _S3 - 0.25f), max(0.75f * (abs(x_0) + _S3 - 0.5f), max(abs(x_0 + 0.5f), _S3) - 0.5f))) < 0.0f)));
    col_out_0.x = _S4.x;
    col_out_0.y = _S4.y;
    col_out_0.z = _S4.z;
    var _S5 : vec4<f32> = col_out_0 * (_S1.col_0 * (textureSample((texture0_0), (sampler0_0), (_S1.uv_0))));
    col_out_0 = _S5;
    var _S6 : pixelOutput_0 = pixelOutput_0( _S5 );
    return _S6;
}

