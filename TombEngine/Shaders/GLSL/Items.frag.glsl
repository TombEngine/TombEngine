#version 450 core

#include "Math.glsl"
#include "CBCamera.glsl"
#include "CBItem.glsl"
#include "ShaderLight.glsl"
#include "Blending.glsl"
#include "AnimatedTextures.glsl"
#include "Shadows.glsl"
#include "Materials.glsl"

layout(binding = 0) uniform sampler2D Texture;
layout(binding = 1) uniform sampler2D NormalTexture;
layout(binding = 7) uniform sampler2D AmbientMapFrontTexture;
layout(binding = 8) uniform sampler2D AmbientMapBackTexture;

in VS_OUT {
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
} fs_in;

layout(location = 0) out vec4 FragColor;

void main()
{
    vec2 uv = fs_in.UV;
    vec3 inNormal = fs_in.Normal;
    vec3 inWorldPosition = fs_in.WorldPosition;
    vec4 inColor = fs_in.Color;
    vec3 inTangent = fs_in.Tangent;
    vec3 inBinormal = fs_in.Binormal;
    vec3 inFaceNormal = fs_in.FaceNormal;

    uv = ConvertAnimUV(uv);

    // Apply parallax mapping
    mat3 TBNf = mat3(inTangent, inBinormal, inFaceNormal);
    uv = ParallaxOcclusionMapping(TBNf, inWorldPosition, uv);

    vec4 ORSH = ConvertAnimOSRH(texture(ORSHTexture, uv));
    float ambientOcclusion = ORSH.x;
    float roughness = ORSH.y;
    float specular = ORSH.z;

    vec3 emissive = texture(EmissiveTexture, uv).xyz;

    mat3 TBN = mat3(inTangent, inBinormal, inNormal);
    vec3 normal = ConvertAnimNormal(UnpackNormalMap(texture(NormalTexture, uv)));
    normal = EnsureNormal(TBN * normal, inWorldPosition);

    vec4 tex = texture(Texture, uv);
    DoAlphaTest(tex);

    // Material effects
    tex.xyz = CalculateReflections(inWorldPosition, tex.xyz, normal, specular);

    // Ambient occlusion
    float occlusion = CalculateOcclusion(GetSamplePosition(fs_in.PositionCopy), tex.w);
    occlusion *= ambientOcclusion;

    uint boneIdx = fs_in.Bone;
    vec3 color_out = (BoneLightModes[boneIdx / 4u][boneIdx % 4u] == 0) ?
        CombineLights(
            AmbientLight.xyz,
            inColor.xyz,
            tex.xyz,
            inWorldPosition,
            normal,
            fs_in.Sheen,
            ItemLights,
            NumItemLights,
            fs_in.FogBulbs.w,
            emissive,
            specular,
            roughness) :
        StaticLight(inColor.xyz, tex.xyz, fs_in.FogBulbs.w, emissive);

    float shadowable = step(0.5, float((NumItemLights & SHADOWABLE_MASK) == SHADOWABLE_MASK));
    vec3 shadow = DoShadow(inWorldPosition, normal, color_out, -0.5);
    shadow = DoBlobShadows(inWorldPosition, shadow);
    color_out = mix(color_out, shadow, shadowable);

    FragColor = clamp(vec4(color_out * occlusion, tex.w), 0.0, 1.0);
    FragColor = DoFogBulbsForPixel(FragColor, vec4(fs_in.FogBulbs.xyz, 1.0));
    FragColor = DoDistanceFogForPixel(FragColor, FogColor, fs_in.DistanceFog);
    FragColor.w *= inColor.w;
}
