#version 450 core

layout(binding = 0) uniform sampler2D Texture;

// Push constants for gradient colors (shared with vertex shader)
// Offset 0-63: mat4 ProjectionMatrix (in vertex shader)
// Offset 64-95: vec4 ColorStart
// Offset 96-127: vec4 ColorEnd
layout(push_constant) uniform PushConstants {
    layout(offset = 64) vec4 ColorStart;
    vec4 ColorEnd;
} pc;

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec2 vUV;

layout(location = 0) out vec4 FragColor;

void main()
{
    // Linear gradient based on Y coordinate
    vec4 gradientColor = mix(pc.ColorStart, pc.ColorEnd, vUV.y);
    vec4 texColor = texture(Texture, vUV);
    FragColor = gradientColor * texColor * vColor;
}
