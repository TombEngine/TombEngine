#version 450 core

#include "CBCamera.glsl"
#include "CBItem.glsl"
#include "Blending.glsl"
#include "Math.glsl"
#include "VertexInput.glsl"

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
    vec4 PositionCopy;
    float Depth;
} vs_out;

void main()
{
    // Blend and apply world matrix
    mat4 blended = Skinned != 0 ? BlendBoneMatrices(in_BoneIndex, in_BoneWeight, Bones, (Skinned == 2)) : Bones[in_BoneIndex[0]];
    mat4 world = World * blended;

    gl_Position = ViewProjection * world * vec4(in_Position, 1.0);
    vs_out.Depth = gl_Position.z / gl_Position.w;
    vs_out.PositionCopy = gl_Position;
}
