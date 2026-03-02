#version 450 core

#include "SMAA.glsl"

layout(binding = 6) uniform sampler2D depthTex;

in VS_OUT {
    vec2 texcoord;
    vec4 offset[3];
} fs_in;

layout(location = 0) out vec2 FragColor;

void main()
{
    FragColor = SMAADepthEdgeDetectionPS(fs_in.texcoord, fs_in.offset, depthTex);
}
