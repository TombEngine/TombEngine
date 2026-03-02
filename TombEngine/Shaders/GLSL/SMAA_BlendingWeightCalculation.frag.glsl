#version 450 core

#include "SMAA.glsl"

layout(binding = 5) uniform sampler2D edgesTex;
layout(binding = 7) uniform sampler2D areaTex;
layout(binding = 8) uniform sampler2D searchTex;

layout(std140, binding = 13) uniform SMAABuffer
{
    vec4 subsampleIndices;
    float blendFactor;
    float threshld;
    float maxSearchSteps;
    float maxSearchStepsDiag;
    float cornerRounding;
    vec3 SMAABuffer_Padding0;
};

in VS_OUT {
    vec2 texcoord;
    vec2 pixcoord;
    vec4 offset[3];
} fs_in;

layout(location = 0) out vec4 FragColor;

void main()
{
    FragColor = SMAABlendingWeightCalculationPS(fs_in.texcoord, fs_in.pixcoord, fs_in.offset, edgesTex, areaTex, searchTex, subsampleIndices);
}
