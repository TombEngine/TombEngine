#version 450 core

#include "CBCamera.glsl"
#include "CBSky.glsl"
#include "VertexInput.glsl"
#include "VertexEffects.glsl"
#include "Blending.glsl"
#include "Math.glsl"
#include "AnimatedTextures.glsl"
#include "ShaderLight.glsl"

layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec4 in_Normal;
layout(location = 2) in vec2 in_UV;
layout(location = 3) in vec4 in_Color;
layout(location = 4) in vec4 in_Tangent;
layout(location = 5) in vec4 in_FaceNormal;
layout(location = 6) in uvec4 in_BoneIndex;
layout(location = 7) in uvec4 in_BoneWeight;
layout(location = 8) in uint in_Effects;
layout(location = 9) in uint in_AnimationFrameOffsetIndexHash;

out VS_OUT {
    vec2 UV;
    vec4 Color;
    float ClipDepth;
} vs_out;

void main()
{
    // Transform vertex to DP-space
    gl_Position = DualParaboloidView * SkyWorld * vec4(in_Position, 1.0);
    gl_Position /= gl_Position.w;

    // For the back-map z has to be inverted
    gl_Position.z *= float(Hemisphere);

    float L = length(gl_Position.xyz);

    gl_Position /= L;

    vs_out.ClipDepth = gl_Position.z;

    gl_Position.x /= gl_Position.z + 1.0;
    gl_Position.y /= gl_Position.z + 1.0;

    // Set z for z-buffering and neutralize w
    gl_Position.z = (L - NearPlane) / (FarPlane - NearPlane);
    gl_Position.w = 1.0;

    vs_out.UV = GetUVPossiblyAnimated(in_UV, DecodeIndexInPoly(in_Effects), DecodeAnimationFrameOffset(in_AnimationFrameOffsetIndexHash));
    vs_out.Color = SkyColor;
}
