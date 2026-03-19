#version 450 core

#include "CBCamera.glsl"
#include "CBInstancedStatics.glsl"
#include "VertexInput.glsl"
#include "VertexEffects.glsl"
#include "AnimatedTextures.glsl"
#include "Blending.glsl"
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
    vec3 Tangent;
    vec3 Binormal;
    vec2 UV;
    vec4 PositionCopy;
    float DistanceFog;
} vs_out;

void main()
{
    uint instanceID = uint(gl_InstanceID);

    // Calculate vertex effects
    float wibble = Wibble(in_Effects, DecodeHash(in_AnimationFrameOffsetIndexHash));
    vec3 pos = Move(in_Position, in_Effects, wibble);

    vec4 worldPosition = StaticMeshes[instanceID].World * vec4(pos, 1.0);

    gl_Position = ViewProjection * worldPosition;
    vs_out.PositionCopy = gl_Position;
    vs_out.UV = GetUVPossiblyAnimated(in_UV, DecodeIndexInPoly(in_Effects), DecodeAnimationFrameOffset(in_AnimationFrameOffsetIndexHash));
    vs_out.Normal = normalize(mat3(StaticMeshes[instanceID].World) * in_Normal.xyz);
    vs_out.Tangent = normalize(mat3(StaticMeshes[instanceID].World) * in_Tangent.xyz);
    vs_out.Binormal = SafeNormalize(mat3(StaticMeshes[instanceID].World) * cross(in_Normal.xyz, in_Tangent.xyz));
    vs_out.DistanceFog = DoDistanceFogForVertex(pos);
}
