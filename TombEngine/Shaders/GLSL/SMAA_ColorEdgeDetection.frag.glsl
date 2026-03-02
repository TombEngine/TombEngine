#version 450 core

#include "SMAA.glsl"

layout(binding = 1) uniform sampler2D colorTexGamma;

in VS_OUT {
    vec2 texcoord;
    vec4 offset[3];
} fs_in;

layout(location = 0) out vec2 FragColor;

void main()
{
    FragColor = SMAAColorEdgeDetectionPS(fs_in.texcoord, fs_in.offset, colorTexGamma);
}
