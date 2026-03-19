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
#include "Math.glsl"
#include "ShaderLight.glsl"
#include "SpriteEffects.glsl"
#include "VertexEffects.glsl"

#define INSTANCED_SPRITES_BUCKET_SIZE 512
#define FADE_FACTOR 0.789

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

layout(binding = 0) uniform sampler2D Texture;
layout(binding = 6) uniform sampler2D DepthTexture;

in VS_OUT {
    vec2 UV;
    vec4 Color;
    vec4 PositionCopy;
    vec4 FogBulbs;
    float DistanceFog;
    flat uint InstanceID;
} fs_in;

layout(location = 0) out vec4 FragColor;

float Contrast(float Input, float ContrastPower)
{
    bool IsAboveHalf = Input > 0.5;
    float ToRaise = clamp(2.0 * (IsAboveHalf ? 1.0 - Input : Input), 0.0, 1.0);
    float Output = 0.5 * pow(ToRaise, ContrastPower);
    Output = IsAboveHalf ? 1.0 - Output : Output;
    return Output;
}

void main()
{
    vec4 output_color = texture(Texture, fs_in.UV) * fs_in.Color;

    InstancedSprite sprite = Sprites[fs_in.InstanceID];

    if (sprite.IsSoftParticle == 1.0)
    {
        vec4 posCopy = fs_in.PositionCopy;
        float particleDepth = posCopy.z / posCopy.w;
        posCopy.xy /= posCopy.w;
        vec2 texCoord = 0.5 * (vec2(posCopy.x, -posCopy.y) + 1.0);
        float sceneDepth = texture(DepthTexture, texCoord).x;

        sceneDepth = LinearizeDepth(sceneDepth, NearPlane, FarPlane);
        particleDepth = LinearizeDepth(particleDepth, NearPlane, FarPlane);

        if (particleDepth - sceneDepth > 0.01)
        {
            discard;
        }

        float fade = (sceneDepth - particleDepth) * 1024.0;
        output_color.w = min(output_color.w, fade);
    }

    if (sprite.RenderType == 1)
    {
        output_color = DoLaserBarrierEffect(gl_FragCoord.xyz, output_color, fs_in.UV, FADE_FACTOR, float(Frame));
    }

    if (sprite.RenderType == 2)
    {
        output_color = DoLaserBeamEffect(gl_FragCoord.xyz, output_color, fs_in.UV, FADE_FACTOR, float(Frame));
    }

    output_color.xyz *= 1.0 - Luma(fs_in.FogBulbs.xyz);
    output_color.xyz = clamp(output_color.xyz, 0.0, 1.0);

    output_color = DoDistanceFogForPixel(output_color, vec4(0.0), fs_in.DistanceFog);

    FragColor = output_color;
}
