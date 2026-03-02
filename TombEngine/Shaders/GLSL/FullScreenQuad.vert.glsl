#version 450 core

#include "VertexInput.glsl"

layout(location = 0) in vec3 in_Position;
layout(location = 2) in vec2 in_UV;
layout(location = 3) in vec4 in_Color;

out VS_OUT {
    vec2 UV;
    vec4 Color;
} vs_out;

void main()
{
    gl_Position = vec4(in_Position, 1.0);
    vs_out.Color = in_Color;
    vs_out.UV = in_UV;
}
