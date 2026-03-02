#version 450 core

#include "CBCamera.glsl"
#include "CBRoom.glsl"
#include "Blending.glsl"
#include "Math.glsl"
#include "AnimatedTextures.glsl"
#include "Shadows.glsl"
#include "ShaderLight.glsl"
#include "Materials.glsl"

#define ROOM_LIGHT_COEFF 0.7

layout(binding = 0) uniform sampler2D Texture;
layout(binding = 1) uniform sampler2D NormalTexture;
layout(binding = 2) uniform sampler2D CausticsTexture;

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

    vec4 outColor = texture(Texture, uv);
    DoAlphaTest(outColor);

    // Material effects
    vec3 blendedNormal = normalize(mix(inFaceNormal, normal, 0.1));
    outColor.xyz = CalculateReflections(inWorldPosition, outColor.xyz, blendedNormal, specular);

    // Ambient occlusion
    float occlusion = CalculateOcclusion(GetSamplePosition(fs_in.PositionCopy), outColor.w);
    occlusion *= ambientOcclusion;
    vec3 lighting = inColor.xyz;

    // Shadows
    lighting = DoShadow(inWorldPosition, normal, lighting, -2.5);
    lighting = DoBlobShadows(inWorldPosition, lighting);

    bool onlyPointLights = (NumRoomLights & ~LT_MASK) == LT_MASK_POINT;
    int numLights = NumRoomLights & LT_MASK;

    for (int i = 0; i < numLights; i++)
    {
        if (onlyPointLights)
        {
            lighting += DoPointLight(inWorldPosition, normal, RoomLights[i]) * ROOM_LIGHT_COEFF;
            lighting += DoSpecularPoint(inWorldPosition, normal, RoomLights[i], 0.0, specular, roughness);
        }
        else
        {
            float isPoint = step(0.5, float(RoomLights[i].Type == LT_POINT));
            float isSpot  = step(0.5, float(RoomLights[i].Type == LT_SPOT));

            vec3 pointLight = vec3(0.0);
            vec3 spotLight  = vec3(0.0);
            DoPointAndSpotLight(inWorldPosition, normal, RoomLights[i], specular, roughness, pointLight, spotLight);

            lighting += pointLight * isPoint * ROOM_LIGHT_COEFF + spotLight * isSpot * ROOM_LIGHT_COEFF;
        }
    }

    // Decals
    if (Animated == 0u && NumRoomDecals > 0 && (MaterialTypeAndFlags & uint(MATERIAL_FLAG_HEIGHTMAP)) == 0u)
    {
        float decalMask = 0.0;

        for (int i = 0; i < NumRoomDecals; i++)
        {
            float radius   = RoomDecals[i].Radius;
            vec3 pos       = inWorldPosition - RoomDecals[i].Position;
            float distance = length(pos);

            if (distance > radius * 1.3)
                continue;

            vec2 duv = vec2(dot(pos, inTangent), dot(pos, inBinormal));
            duv *= (8.0 / float(RoomDecals[i].Pattern + 1u) * 2.0 / radius);

            float noiseVal = NebularNoise(duv, 1.0, 0.5, 0.3);

            float noisyRadius = radius * (1.0 + 0.25 * (noiseVal * 2.0 - 1.0));
            float holeRadius = radius / 4.0;

            float edge = clamp((noisyRadius - distance) / noisyRadius, 0.0, 1.0);
            float fade = clamp((radius - distance) / radius, 0.0, 1.0);
            float hole = clamp((holeRadius - distance) / (holeRadius * 1.3), 0.0, 1.0) * float(1 - int(RoomDecals[i].Pattern));

            decalMask = max(decalMask, (edge * fade + hole) * RoomDecals[i].Opacity);
        }

        lighting *= (1.0 - decalMask);
    }

    // Caustics
    if (Caustics != 0)
    {
        float attenuation = clamp(dot(vec3(0.0, -1.0, 0.0), normal), 0.0, 1.0);

        vec3 blending = abs(normal);
        blending = normalize(max(blending, 0.00001));
        float b = (blending.x + blending.y + blending.z);
        blending /= vec3(b);

        vec3 p = fract(inWorldPosition.xyz / 2048.0);

        vec2 uv_x = CausticsStartUV + vec2(p.z, p.y) * CausticsSize;
        vec2 uv_y = CausticsStartUV + vec2(p.z, p.x) * CausticsSize;
        vec2 uv_z = CausticsStartUV + vec2(p.y, p.x) * CausticsSize;

        vec3 xaxis = textureLod(CausticsTexture, uv_x, 0.0).xyz;
        vec3 yaxis = textureLod(CausticsTexture, uv_y, 0.0).xyz;
        vec3 zaxis = textureLod(CausticsTexture, uv_z, 0.0).xyz;

        vec3 xc = xaxis * blending.x;
        vec3 yc = yaxis * blending.y;
        vec3 zc = zaxis * blending.z;

        vec3 caustics = xc + yc + zc;

        lighting += (caustics * attenuation * 2.0);
    }

    // Emissive materials
    lighting += emissive;

    // Fog bulbs and final color and light mixing
    lighting -= vec3(fs_in.FogBulbs.w);
    outColor.xyz = outColor.xyz * lighting * occlusion;
    outColor.xyz = clamp(outColor.xyz, 0.0, 1.0);

    outColor = DoFogBulbsForPixel(outColor, vec4(fs_in.FogBulbs.xyz, 1.0));
    outColor = DoDistanceFogForPixel(outColor, FogColor, fs_in.DistanceFog);

    FragColor = outColor;
}
