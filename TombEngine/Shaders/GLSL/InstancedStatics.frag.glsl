#version 450 core

#include "Math.glsl"
#include "CBCamera.glsl"
#include "CBInstancedStatics.glsl"
#include "ShaderLight.glsl"
#include "CLightBuffer.glsl"
#include "Blending.glsl"
#include "Shadows.glsl"
#include "AnimatedTextures.glsl"
#include "Materials.glsl"

layout(binding = 0) uniform sampler2D Texture;
layout(binding = 1) uniform sampler2D NormalTexture;

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
    flat uint InstanceID;
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

    uint instID = fs_in.InstanceID;
    uint mode = StaticMeshes[instID].LightInfo.y;
    uint numLights = StaticMeshes[instID].LightInfo.x;

    // Material effects
    tex.xyz = CalculateReflections(inWorldPosition, tex.xyz, normal, specular);

    // Ambient occlusion
    float occlusion = CalculateOcclusion(GetSamplePosition(fs_in.PositionCopy), tex.w);
    occlusion *= ambientOcclusion;

    vec3 color_out = (mode == 0u) ?
        CombineLights(
            StaticMeshes[instID].AmbientLight.xyz,
            inColor.xyz,
            tex.xyz,
            inWorldPosition,
            normal,
            fs_in.Sheen,
            StaticMeshes[instID].InstancedStaticLights,
            int(numLights),
            fs_in.FogBulbs.w,
            emissive,
            specular,
            roughness) :
        StaticLight(inColor.xyz, tex.xyz, fs_in.FogBulbs.w, emissive);

    color_out = DoShadow(inWorldPosition, normal, color_out, -0.5);
    color_out = DoBlobShadows(inWorldPosition, color_out);

    FragColor = vec4(color_out * occlusion, tex.w);
    FragColor = DoFogBulbsForPixel(FragColor, vec4(fs_in.FogBulbs.xyz, 1.0));
    FragColor = DoDistanceFogForPixel(FragColor, FogColor, fs_in.DistanceFog);
    FragColor.w *= inColor.w;
}
