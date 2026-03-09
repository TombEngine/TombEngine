#version 450 core

#include "Math.glsl"
#include "CBCamera.glsl"
#include "CBPostProcess.glsl"

layout(binding = 0) uniform sampler2D DepthTexture;
layout(binding = 1) uniform sampler2D NormalsTexture;
layout(binding = 2) uniform sampler2D NoiseTexture;

in VS_OUT {
    vec2 UV;
    vec4 Color;
} fs_in;

layout(location = 0) out float FragColor;

vec3 DecodeNormal(vec3 n)
{
    return (n * 2.0 - 1.0);
}

vec3 ReconstructPositionFromDepth(vec2 uv)
{
    float x = uv.x * 2.0 - 1.0;
    float y = uv.y * 2.0 - 1.0;
    float z = texture(DepthTexture, uv).x;

    vec4 projectedPosition = vec4(x, y, z, 1.0);
    vec4 position = InverseProjection * projectedPosition;

    return position.xyz / position.w;
}

void main()
{
    vec2 noiseScale = vec2(ViewSize) / 4.0;

    vec3 position = ReconstructPositionFromDepth(fs_in.UV);
    vec3 encodedNormal = texture(NormalsTexture, fs_in.UV).xyz;

    float farMask = step(40960.0, length(position));
    float noNormalMask = step(length(encodedNormal), 0.0001);
    float earlyExit = clamp(farMask + noNormalMask, 0.0, 1.0);

    if (earlyExit > 0.0)
    {
        FragColor = 1.0;
        return;
    }

    vec3 normal = DecodeNormal(encodedNormal);
    vec3 randomVec = texture(NoiseTexture, fs_in.UV * noiseScale).xyz;

    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = SafeNormalize(cross(normal, tangent));
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    int kernelSize = 64;
    float radius = 64.0;
    float bias = 4.0;

    for (int i = 0; i < kernelSize; ++i)
    {
        vec3 samplePos = TBN * SSAOKernel[i].xyz;
        samplePos = position + samplePos * radius;

        vec4 offset = vec4(samplePos, 1.0);
        offset = Projection * offset;
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;

        float sampleDepth = ReconstructPositionFromDepth(offset.xy).z;
        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(position.z - sampleDepth));

        occlusion += mix(0.0, rangeCheck, step(0.0, sampleDepth - samplePos.z - bias));
    }

    occlusion = 1.0 - (occlusion / float(kernelSize));

    FragColor = occlusion;
}
