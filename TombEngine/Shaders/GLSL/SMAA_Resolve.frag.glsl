#version 450 core

#include "SMAA.glsl"

layout(binding = 0) uniform sampler2D colorTex;
layout(binding = 2) uniform sampler2D colorTexPrev;

in VS_OUT {
    vec2 texcoord;
} fs_in;

layout(location = 0) out vec4 FragColor;

void main()
{
    FragColor = SMAAResolvePS(fs_in.texcoord, colorTex, colorTexPrev);
}
