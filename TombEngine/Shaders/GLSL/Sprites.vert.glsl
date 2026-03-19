#version 450 core

#include "CBCamera.glsl"

#define BLENDING_CB_MERGED 1
layout(std140, binding = 2) uniform CBBlending
{
    uint BlendMode;
    int AlphaTest;
    float AlphaThreshold;
    int CBBlending_Padding0;
};

#include "Blending.glsl"
#include "VertexInput.glsl"
#include "Math.glsl"
#include "SpriteEffects.glsl"
#include "ShaderLight.glsl"

layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec4 in_Normal;
layout(location = 2) in vec2 in_UV;
layout(location = 3) in vec4 in_Color;

out VS_OUT {
    vec3 Normal;
    vec2 UV;
    vec4 Color;
    vec4 PositionCopy;
    vec4 FogBulbs;
    float DistanceFog;
} vs_out;

void main()
{
    vec4 worldPosition = vec4(in_Position, 1.0);

    gl_Position = ViewProjection * worldPosition;
    vs_out.PositionCopy = gl_Position;
    vs_out.Normal = in_Normal.xyz;
    vs_out.Color = in_Color;
    vs_out.UV = in_UV;

    vs_out.FogBulbs = DoFogBulbsForVertex(worldPosition.xyz);
    vs_out.DistanceFog = DoDistanceFogForVertex(worldPosition.xyz);
}
