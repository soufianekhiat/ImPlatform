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
    var _S3 : f32 = sqrt(2.0f);
    var _S4 : vec2<f32> = vec2<f32>(3.5f);
    var _S5 : vec3<f32> = vec3<f32>(vec3<bool>(((min(min(max(abs(_S3 / 2.0f * (P_0.x - P_0.y)), abs(_S3 / 2.0f * (P_0.x + P_0.y))) - 0.28571429848670959f, length(P_0 - vec2<f32>((_S3 / 2.0f)) * vec2<f32>(1.0f, -1.0f) / _S4) - 0.28571429848670959f), length(P_0 - vec2<f32>((_S3 / 2.0f)) * vec2<f32>(-1.0f, -1.0f) / _S4) - 0.28571429848670959f)) < 0.0f)));
    col_out_0.x = _S5.x;
    col_out_0.y = _S5.y;
    col_out_0.z = _S5.z;
    var _S6 : vec4<f32> = col_out_0 * (_S1.col_0 * (textureSample((texture0_0), (sampler0_0), (_S1.uv_0))));
    col_out_0 = _S6;
    var _S7 : pixelOutput_0 = pixelOutput_0( _S6 );
    return _S7;
}

