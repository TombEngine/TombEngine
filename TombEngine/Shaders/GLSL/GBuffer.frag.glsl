#version 450 core

#include "CBCamera.glsl"
#include "AnimatedTextures.glsl"
#include "Blending.glsl"
#include "Math.glsl"
#include "Materials.glsl"

layout(binding = 0) uniform sampler2D Texture;
layout(binding = 1) uniform sampler2D NormalTexture;

in VS_OUT {
    vec3 Normal;
    vec3 Tangent;
    vec3 Binormal;
    vec2 UV;
    vec4 PositionCopy;
    float DistanceFog;
} fs_in;

layout(location = 0) out vec4 FragNormals;
layout(location = 1) out float FragDepth;
layout(location = 2) out vec4 FragEmissive;

vec3 DecodeNormalMap(vec4 n)
{
    n = n * 2.0 - 1.0;
    n.z = clamp(1.0 - dot(n.xy, n.xy), 0.0, 1.0);
    return n.xyz;
}

vec3 EncodeNormal(vec3 n)
{
    n = (n + 1.0) * 0.5;
    return n.xyz;
}

void main()
{
    vec2 uv = fs_in.UV;

    if (Animated != 0u && Type == 1u)
    {
        if (IsWaterfall != 0)
            uv = CalculateUVRotateForLegacyWaterfalls(uv, 0u);
        else
            uv = CalculateUVRotate(uv, 0u);
    }

    vec4 color = texture(Texture, uv);

    DoAlphaTest(color);

    vec4 emissive = texture(EmissiveTexture, uv);
    float specular = texture(ORSHTexture, uv).z;

    mat3 TBN = mat3(fs_in.Tangent, fs_in.Binormal, fs_in.Normal);
    vec3 normal = DecodeNormalMap(texture(NormalTexture, uv));
    normal = EncodeNormal(normalize(mat3(View) * (TBN * normal)));

    FragNormals.xyz = normal;
    FragDepth = color.w > 0.0 ? fs_in.PositionCopy.z / fs_in.PositionCopy.w : 0.0;
    FragEmissive.xyz = DoDistanceFogForPixel(vec4(emissive.xyz, 1.0), vec4(0.0), pow(fs_in.DistanceFog, 2.0)).xyz;
    FragEmissive.w = specular;
}
