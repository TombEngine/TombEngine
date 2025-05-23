#include "./VertexInput.hlsli"
#include "./CBCamera.hlsli"
#include "./CBPostProcess.hlsli"

#define SIGMA 3.0
#define BSIGMA 0.3
#define MSIZE 5

struct PixelShaderInput
{
    float4 Position: SV_POSITION;
    float2 UV: TEXCOORD0;
};

Texture2D DepthTexture : register(t0);
SamplerState DepthSampler : register(s0);

Texture2D NormalsTexture : register(t1);
SamplerState NormalsSampler : register(s1);

Texture2D NoiseTexture : register(t2);
SamplerState NoiseSampler : register(s2);

Texture2D SSAOTexture : register(t9);
SamplerState SSAOSampler : register(s9);

float3 DecodeNormal(float3 n)
{
    return (n * 2.0f - 1.0f);
}

float3 ReconstructPositionFromDepth(float2 uv)
{
    float x = uv.x * 2.0f - 1.0f;
    float y = (1.0f - uv.y) * 2.0f - 1.0f;
    float z = DepthTexture.Sample(DepthSampler, uv).x;

    float4 projectedPosition = float4(x, y, z, 1.0f);
    float4 position = mul(projectedPosition, InverseProjection);

    return position.xyz / position.w;
}

float PS(PixelShaderInput input) : SV_Target
{
    float4 output;

    float2 noiseScale = float2(ViewportWidth / 4.0f, ViewportHeight / 4.0f);

    float3 position = ReconstructPositionFromDepth(input.UV);
    float3 encodedNormal = NormalsTexture.Sample(NormalsSampler, input.UV).xyz;

    float farMask = step(40960.0f, length(position)); // 1 if too far
    float noNormalMask = step(length(encodedNormal), 0.0001f); // 1 if normal is too small
    float earlyExit = saturate(farMask + noNormalMask); // 0 if both are fine
   
    if (earlyExit > 0.0f)
        return float4(1.0f, 1.0f, 1.0f, 1.0f);

    float3 normal = DecodeNormal(encodedNormal);
    float3 randomVec = NoiseTexture.Sample(NoiseSampler, input.UV * noiseScale).xyz;

    float3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    float3 bitangent = cross(normal, tangent);
    float3x3 TBN = float3x3(tangent, bitangent, normal);

    float occlusion = 0.0f;
    int kernelSize = 64;
    float radius = 64.0f;
    float bias = 4.0f;

    for (int i = 0; i < kernelSize; ++i)
    {
        float3 samplePos = mul(SSAOKernel[i], TBN);
        samplePos = position + samplePos * radius;

        float4 offset = float4(samplePos, 1.0);
        offset = mul(offset, Projection); 
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5f + 0.5f; 
        offset.y = 1.0f - offset.y;

        float sampleDepth = ReconstructPositionFromDepth(offset.xy).z;
        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(position.z - sampleDepth));

        occlusion += lerp(0.0f, rangeCheck, step(0.0, sampleDepth - samplePos.z - bias));
        //occlusion += (sampleDepth >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
    }

    occlusion = 1.0 - (occlusion / kernelSize);

    return occlusion;
}

float normpdf(float x, float sigma)
{
    return 0.39894 * exp(-0.5 * x * x / (sigma * sigma)) / sigma;
}

float PSBlur(PixelShaderInput input) : SV_Target
{
    float2 texelSize = 1.0f / float2(ViewportWidth, ViewportHeight);
    float result = 0.0f;

    const int kernelSize = (MSIZE - 1) / 2;
    float kernel[MSIZE];
    float bZ = 0.0;

    // Create the 1-D kernel
    for (int j = 0; j <= kernelSize; j++)
    {
        kernel[kernelSize + j] = kernel[kernelSize - j] = normpdf(float(j), SIGMA);
    }

    float color;
    float baseColor = SSAOTexture.Sample(SSAOSampler, input.UV).x;
    float gfactor;
    float bfactor;
    float bZnorm = 1.0 / normpdf(0.0, BSIGMA);

    // Read out the texels
    for (int i = -kernelSize; i <= kernelSize; i++)
    {
        for (int j = -kernelSize; j <= kernelSize; j++)
        {
            // Color at pixel in the neighborhood
            float2 offset = float2(i, j) * texelSize;
            color = SSAOTexture.Sample(SSAOSampler, input.UV + offset).x;

            // Compute both the gaussian smoothed and bilateral
            gfactor = kernel[kernelSize + j] * kernel[kernelSize + i];
            bfactor = normpdf(color - baseColor, BSIGMA) * bZnorm * gfactor;
            bZ += bfactor;

            result += bfactor * color;
        }
    }

    return (result / bZ);
}
