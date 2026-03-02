#version 450 core

#include "SMAA.glsl"

layout(binding = 0) uniform sampler2D colorTex;
layout(binding = 6) uniform sampler2D blendTex;

in VS_OUT {
    vec2 texcoord;
    vec4 offset;
} fs_in;

layout(location = 0) out vec4 FragColor;

void main()
{
    FragColor = SMAANeighborhoodBlendingPS(fs_in.texcoord, fs_in.offset, colorTex, blendTex);
}
