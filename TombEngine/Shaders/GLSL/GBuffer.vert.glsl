#version 450 core

// GBuffer vertex shader - VSRooms variant (default)
#include "CBCamera.glsl"
#include "CBRoom.glsl"
#include "Materials.glsl"
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
    // Setting effect weight on TE side prevents portal vertices from moving.
    float weight = DecodeWeight(in_Effects);

    // Calculate vertex effects
    float wibble = Wibble(in_Effects, DecodeHash(in_AnimationFrameOffsetIndexHash));
    vec3 pos = Move(in_Position, in_Effects * uint(weight), wibble);

    // Refraction
    vec4 screenPos = ViewProjection * vec4(pos, 1.0);
    vec2 clipPos = screenPos.xy / screenPos.w;

    if (CameraUnderwater != Water)
    {
        float factor = float(Frame) + clipPos.x * 320.0;
        float xOffset = (sin(factor * PI / 20.0)) * (screenPos.z / 1024.0) * 4.0;
        float yOffset = (cos(factor * PI / 20.0)) * (screenPos.z / 1024.0) * 4.0;
        screenPos.x += xOffset * weight;
        screenPos.y += yOffset * weight;
    }

    gl_Position = screenPos;
    vs_out.Normal = in_Normal.xyz;
    vs_out.Tangent = in_Tangent.xyz;
    vs_out.Binormal = cross(in_Normal.xyz, in_Tangent.xyz);
    vs_out.PositionCopy = screenPos;
    vs_out.UV = GetUVPossiblyAnimated(in_UV, DecodeIndexInPoly(in_Effects), DecodeAnimationFrameOffset(in_AnimationFrameOffsetIndexHash));
    vs_out.DistanceFog = DoDistanceFogForVertex(pos);
}
