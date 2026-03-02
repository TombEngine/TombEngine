#version 450 core

#include "CBCamera.glsl"
#include "CBItem.glsl"
#include "Blending.glsl"
#include "ShaderLight.glsl"
#include "AnimatedTextures.glsl"
#include "Materials.glsl"
#include "Math.glsl"

layout(binding = 0) uniform sampler2D Texture;
layout(binding = 1) uniform sampler2D NormalTexture;

in VS_OUT {
    vec3 Normal;
    vec3 WorldPosition;
    vec2 UV;
    vec4 Color;
    float Sheen;
    vec3 Tangent;
    vec3 Binormal;
    vec3 FaceNormal;
} fs_in;

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 EmissiveOut;

void main()
{
    vec2 uv = fs_in.UV;

    if (Animated != 0u && Type == 1u)
        uv = CalculateUVRotate(uv, 0u);

    vec4 tex = texture(Texture, uv);
    vec3 baseColor = tex.xyz * Color.xyz;
    vec3 pos = normalize(fs_in.WorldPosition);

    vec4 outColor = vec4(baseColor, tex.w * Color.w);

    DoAlphaTest(outColor);

    vec4 ORSH = texture(ORSHTexture, uv);
    float ambientOcclusion = ORSH.x;
    float roughness = ORSH.y;
    float specular = ORSH.z;

    vec3 emissive = texture(EmissiveTexture, uv).xyz;

    mat3 TBN = mat3(fs_in.Tangent, fs_in.Binormal, fs_in.Normal);
    vec3 normal = UnpackNormalMap(texture(NormalTexture, uv));
    normal = normalize(TBN * normal);

    // Material effects
    outColor.xyz = CalculateReflections(fs_in.WorldPosition, outColor.xyz, normal, specular);

    ShaderLight l;
    l.Color = AmbientLight.xyz;
    l.Intensity = 0.3;
    l.Type = LT_SUN;
    l.Direction = normalize(vec3(-1.0, -0.707, -0.5));

    vec3 lighting = DoDirectionalLight(pos, normal, l);
    lighting += DoSpecularSun(normal, l, fs_in.Sheen, specular, roughness);
    lighting += emissive;

    // Emissive material
    outColor.xyz += lighting * outColor.a;
    outColor.xyz = clamp(outColor.xyz, 0.0, 1.0);

    FragColor = outColor;
    EmissiveOut = vec4(emissive, 1.0);
}
