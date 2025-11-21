#version 450 core

layout(binding = 0) uniform sampler2D Texture;

// Push constants for arrow parameters (shared with vertex shader)
// Offset 0-63: mat4 ProjectionMatrix (in vertex shader)
// Offset 64-79: vec4 ArrowColor
// Offset 80-83: float ArrowThickness
// Offset 84-87: float ArrowSmoothness
layout(push_constant) uniform PushConstants {
    layout(offset = 64) vec4 ArrowColor;
    float ArrowThickness;
    float ArrowSmoothness;
} pc;

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec2 vUV;

layout(location = 0) out vec4 FragColor;

// SDF for an arrow shape
float sdArrow(vec2 p, float width, float height)
{
    // Transform to center
    p = p - vec2(0.5, 0.5);
    p.y = -p.y; // Flip Y

    // Arrow shaft
    float shaft = abs(p.x) - width * 0.1;
    shaft = max(shaft, abs(p.y + height * 0.15) - height * 0.3);

    // Arrow head (triangle)
    vec2 head_p = p - vec2(0.0, height * 0.15);
    float head = max(abs(head_p.x) - width * 0.4 + head_p.y * 0.8, -head_p.y - height * 0.2);

    return min(shaft, head);
}

void main()
{
    // Calculate SDF
    float d = sdArrow(vUV, 0.5, 0.6);

    // Apply anti-aliasing using smoothstep
    float alpha = 1.0 - smoothstep(-pc.ArrowSmoothness, pc.ArrowSmoothness, d - pc.ArrowThickness);

    // Mix with texture color
    vec4 texColor = texture(Texture, vUV);
    FragColor = vec4(pc.ArrowColor.rgb, pc.ArrowColor.a * alpha) * texColor * vColor;
}
