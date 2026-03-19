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
#include "SpriteEffects.glsl"
#include "ShaderLight.glsl"

#define FADE_FACTOR 0.789

layout(std140, binding = 1) uniform SpriteBuffer
{
    float IsSoftParticle;
    int RenderType;
    int SpriteBuffer_Padding0;
    int SpriteBuffer_Padding1;
};

layout(binding = 0) uniform sampler2D Texture;
layout(binding = 6) uniform sampler2D DepthTexture;

in VS_OUT {
    vec3 Normal;
    vec2 UV;
    vec4 Color;
    vec4 PositionCopy;
    vec4 FogBulbs;
    float DistanceFog;
} fs_in;

layout(location = 0) out vec4 FragColor;

void main()
{
    vec4 output_color = texture(Texture, fs_in.UV) * fs_in.Color;

    if (IsSoftParticle == 1.0)
    {
        vec4 posCopy = fs_in.PositionCopy;
        float particleDepth = posCopy.z / posCopy.w;
        posCopy.xy /= posCopy.w;

        vec2 texCoord = 0.5 * (vec2(posCopy.x, -posCopy.y) + 1.0);
        float sceneDepth = texture(DepthTexture, texCoord).x;

        sceneDepth = LinearizeDepth(sceneDepth, NearPlane, FarPlane);
        particleDepth = LinearizeDepth(particleDepth, NearPlane, FarPlane);

        if (particleDepth - sceneDepth > 0.01)
            discard;

        float fade = (sceneDepth - particleDepth) * 1024.0;
        output_color.w = min(output_color.w, fade);
    }

    if (RenderType == 1)
    {
        output_color = DoLaserBarrierEffect(gl_FragCoord.xyz, output_color, fs_in.UV, FADE_FACTOR, float(Frame));
    }

    if (RenderType == 2)
    {
        output_color = DoLaserBeamEffect(gl_FragCoord.xyz, output_color, fs_in.UV, FADE_FACTOR, float(Frame));
    }

    output_color.xyz *= 1.0 - Luma(fs_in.FogBulbs.xyz);
    output_color.xyz = clamp(output_color.xyz, 0.0, 1.0);

    output_color = DoDistanceFogForPixel(output_color, vec4(0.0), fs_in.DistanceFog);

    FragColor = output_color;
}
