#version 450 core

layout(push_constant) uniform PushConstants {
    mat4 ProjectionMatrix;
} pc;

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;

layout(location = 0) out vec4 vColor;
layout(location = 1) out vec2 vUV;

void main()
{
    vColor = aColor;
    vUV = aUV;
    gl_Position = pc.ProjectionMatrix * vec4(aPos.xy, 0.0, 1.0);
}
