#version 450 core

#include "SMAA.glsl"

layout(location = 0) in vec3 in_Position;
layout(location = 2) in vec2 in_UV;

out VS_OUT {
    vec2 texcoord;
    vec4 offset;
} vs_out;

void main()
{
    gl_Position = vec4(in_Position, 1.0);
    vs_out.texcoord = vec2(in_UV.x, 1.0 - in_UV.y);
    SMAANeighborhoodBlendingVS(vs_out.texcoord, vs_out.offset);
}
