#version 450 core

#include "CBCamera.glsl"
#include "CBItem.glsl"
#include "Blending.glsl"
#include "VertexInput.glsl"
#include "Math.glsl"
#include "ShaderLight.glsl"
#include "CBSky.glsl"
#include "AnimatedTextures.glsl"
#include "VertexEffects.glsl"

layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec4 in_Normal;
layout(location = 2) in vec2 in_UV;
layout(location = 3) in vec4 in_Color;
layout(location = 8) in uint in_Effects;
layout(location = 9) in uint in_AnimationFrameOffsetIndexHash;

out VS_OUT {
    vec3 Normal;
    vec2 UV;
    vec4 Color;
    vec4 FogBulbs;
} vs_out;

void main()
{
    vec4 worldPosition = World * vec4(in_Position, 1.0);

    gl_Position = ViewProjection * worldPosition;
    vs_out.Normal = in_Normal.xyz;
    vs_out.Color = in_Color;
    vs_out.UV = GetUVPossiblyAnimated(in_UV, DecodeIndexInPoly(in_Effects), DecodeAnimationFrameOffset(in_AnimationFrameOffsetIndexHash));
    vs_out.FogBulbs = ApplyFogBulbs == 1 ? DoFogBulbsForSky(worldPosition.xyz) : vec4(0.0);
}
