#version 450 core

#include "CBCamera.glsl"
#include "CBItem.glsl"
#include "Blending.glsl"
#include "VertexInput.glsl"
#include "ShaderLight.glsl"
#include "AnimatedTextures.glsl"
#include "VertexEffects.glsl"
#include "Materials.glsl"
#include "Math.glsl"

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
    vec3 Normal;
    vec3 WorldPosition;
    vec2 UV;
    vec4 Color;
    float Sheen;
    vec3 Tangent;
    vec3 Binormal;
    vec3 FaceNormal;
} vs_out;

void main()
{
    mat4 blended = Skinned != 0 ? BlendBoneMatrices(in_BoneIndex, in_BoneWeight, Bones, (Skinned == 2)) : Bones[in_BoneIndex[0]];
    mat4 world = World * blended;

    gl_Position = ViewProjection * world * vec4(in_Position, 1.0);
    vs_out.Normal = mat3(world) * in_Normal.xyz;
    vs_out.Tangent = normalize(mat3(world) * in_Tangent.xyz);
    vs_out.Binormal = SafeNormalize(mat3(world) * cross(in_Normal.xyz, in_Tangent.xyz));
    vs_out.Color = in_Color;
    vs_out.UV = GetUVPossiblyAnimated(in_UV, DecodeIndexInPoly(in_Effects), DecodeAnimationFrameOffset(in_AnimationFrameOffsetIndexHash));
    vs_out.WorldPosition = (world * vec4(in_Position, 1.0)).xyz;
    vs_out.Sheen = DecodeSheen(in_Effects);
    vs_out.FaceNormal = normalize(mat3(world) * in_FaceNormal.xyz);
}
