#version 450 core

#include "CBCamera.glsl"
#include "VertexInput.glsl"

layout(location = 0) in vec3 in_Position;
layout(location = 3) in vec4 in_Color;

out VS_OUT {
    vec4 Color;
} vs_out;

void main()
{
    gl_Position = ViewProjection * vec4(in_Position, 1.0);
    vs_out.Color = in_Color;
}
