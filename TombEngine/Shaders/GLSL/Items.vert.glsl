#version 450 core

#include "Math.glsl"
#include "CBCamera.glsl"
#include "CBItem.glsl"
#include "ShaderLight.glsl"
#include "VertexEffects.glsl"
#include "VertexInput.glsl"
#include "Blending.glsl"
#include "AnimatedTextures.glsl"
#include "Shadows.glsl"
#include "Materials.glsl"

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
    vec3 WorldPosition;
    vec3 Normal;
    vec2 UV;
    vec4 Color;
    float Sheen;
    vec4 PositionCopy;
    vec4 FogBulbs;
    float DistanceFog;
    vec3 Tangent;
    vec3 Binormal;
    vec3 FaceNormal;
    flat uint Bone;
} vs_out;

void main()
{
    // Blend and apply world matrix
    mat4 blended = Skinned != 0 ? BlendBoneMatrices(in_BoneIndex, in_BoneWeight, Bones, (Skinned == 2)) : Bones[in_BoneIndex[0]];
    mat4 world = World * blended;

    // Calculate vertex effects
    float wibble = Wibble(in_Effects, DecodeHash(in_AnimationFrameOffsetIndexHash));
    vec3 pos = Move(in_Position, in_Effects, wibble);
    vec3 col = Glow(in_Color.xyz, in_Effects, wibble);
    vec3 worldPosition = (world * vec4(pos, 1.0)).xyz;

    gl_Position = ViewProjection * vec4(worldPosition, 1.0);
    vs_out.UV = GetUVPossiblyAnimated(in_UV, DecodeIndexInPoly(in_Effects), DecodeAnimationFrameOffset(in_AnimationFrameOffsetIndexHash));
    vs_out.Color = vec4(col, in_Color.w);
    vs_out.Color *= Color;
    vs_out.PositionCopy = gl_Position;
    vs_out.Sheen = DecodeSheen(in_Effects);
    vs_out.Bone = in_BoneIndex[0];
    vs_out.WorldPosition = worldPosition;

    vs_out.Normal = normalize(mat3(world) * in_Normal.xyz);
    vs_out.Tangent = normalize(mat3(world) * in_Tangent.xyz);
    vs_out.Binormal = SafeNormalize(mat3(world) * cross(in_Normal.xyz, in_Tangent.xyz));
    vs_out.FaceNormal = normalize(mat3(world) * in_FaceNormal.xyz);

    vs_out.FogBulbs = DoFogBulbsForVertex(worldPosition);
    vs_out.DistanceFog = DoDistanceFogForVertex(worldPosition);
}
