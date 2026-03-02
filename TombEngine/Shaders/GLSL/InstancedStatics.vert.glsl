#version 450 core

#include "Math.glsl"
#include "CBCamera.glsl"
#include "CBInstancedStatics.glsl"
#include "ShaderLight.glsl"
#include "VertexEffects.glsl"
#include "VertexInput.glsl"
#include "Blending.glsl"
#include "Shadows.glsl"
#include "AnimatedTextures.glsl"
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
    flat uint InstanceID;
} vs_out;

void main()
{
    uint instanceID = uint(gl_InstanceID);

    float wibble = Wibble(in_Effects, DecodeHash(in_AnimationFrameOffsetIndexHash));
    vec3 pos = Move(in_Position, in_Effects, wibble);
    vec3 col = Glow(in_Color.xyz, in_Effects, wibble);

    vec4 worldPosition = StaticMeshes[instanceID].World * vec4(pos, 1.0);

    gl_Position = ViewProjection * worldPosition;
    vs_out.UV = GetUVPossiblyAnimated(in_UV, DecodeIndexInPoly(in_Effects), DecodeAnimationFrameOffset(in_AnimationFrameOffsetIndexHash));
    vs_out.WorldPosition = worldPosition.xyz;
    vs_out.Color = vec4(col, in_Color.w);
    vs_out.Color *= StaticMeshes[instanceID].Color;
    vs_out.PositionCopy = gl_Position;
    vs_out.Sheen = DecodeSheen(in_Effects);
    vs_out.InstanceID = instanceID;

    vs_out.Normal = normalize(mat3(StaticMeshes[instanceID].World) * in_Normal.xyz);
    vs_out.Tangent = normalize(mat3(StaticMeshes[instanceID].World) * in_Tangent.xyz);
    vs_out.Binormal = SafeNormalize(mat3(StaticMeshes[instanceID].World) * cross(in_Normal.xyz, in_Tangent.xyz));
    vs_out.FaceNormal = normalize(mat3(StaticMeshes[instanceID].World) * in_FaceNormal.xyz);

    vs_out.FogBulbs = DoFogBulbsForVertex(worldPosition.xyz);
    vs_out.DistanceFog = DoDistanceFogForVertex(worldPosition.xyz);
}
