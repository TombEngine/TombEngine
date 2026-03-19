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
#include "ShaderLight.glsl"
#include "SpriteEffects.glsl"
#include "VertexEffects.glsl"

#define INSTANCED_SPRITES_BUCKET_SIZE 512

struct InstancedSprite
{
    mat4 World;
    vec4 UV[2];
    vec4 Color;
    float IsBillboard;
    float IsSoftParticle;
    int RenderType;
    int PerVertexColor;
};

layout(std140, binding = 1) uniform InstancedSpriteBuffer
{
    InstancedSprite Sprites[INSTANCED_SPRITES_BUCKET_SIZE];
};

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
    vec4 PositionCopy;
    vec4 FogBulbs;
    float DistanceFog;
    flat uint InstanceID;
} vs_out;

void main()
{
    uint instanceID = uint(gl_InstanceID);
    InstancedSprite sprite = Sprites[instanceID];

    vec4 worldPosition;

    if (sprite.IsBillboard == 1.0)
    {
        worldPosition = sprite.World * vec4(in_Position, 1.0);
        gl_Position = ViewProjection * sprite.World * vec4(in_Position, 1.0);
    }
    else
    {
        worldPosition = vec4(in_Position, 1.0);
        gl_Position = ViewProjection * vec4(in_Position, 1.0);
    }

    int polyIndex = DecodeIndexInPoly(in_Effects);

    vs_out.PositionCopy = gl_Position;
    vs_out.Color = mix(sprite.Color, in_Color, clamp(float(sprite.PerVertexColor), 0.0, 1.0));
    vs_out.UV = vec2(sprite.UV[0][polyIndex], sprite.UV[1][polyIndex]);
    vs_out.InstanceID = instanceID;

    vs_out.FogBulbs = DoFogBulbsForVertex(worldPosition.xyz);
    vs_out.DistanceFog = DoDistanceFogForVertex(worldPosition.xyz);
}
