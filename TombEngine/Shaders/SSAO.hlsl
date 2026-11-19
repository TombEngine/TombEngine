// TEN_SHADER: Vertex VS samplers=0 storage_textures=0 storage_buffers=0 uniform_buffers=0
// TEN_SHADER: Pixel PS samplers=3 storage_textures=0 storage_buffers=0 uniform_buffers=3
// TEN_SHADER: Pixel PSBlurHorizontal samplers=3 storage_textures=0 storage_buffers=0 uniform_buffers=3
// TEN_SHADER: Pixel PSBlurVertical samplers=3 storage_textures=0 storage_buffers=0 uniform_buffers=3

// Screen space ambient occlusion (SceneRendererSSAO.cpp), computed at half resolution
// from the depth and view space normals of the prepass, then blurred with a separable bilateral filter.

#include "./ShaderInterop.hlsli"
#include "./Fullscreen.hlsli"

#define SSAO_KERNEL_SIZE 16
#define BLUR_SIZE        5
#define BLUR_SIGMA       3.0f
#define BLUR_BSIGMA      0.3f

// Slot 2, so the frame uniforms in slots 0 and 1 stay valid for the scene pass.
TEN_PS_UNIFORMS(SsaoUniforms, 2)
{
    float4x4 Projection;
    float4x4 InverseProjection;
    float2   TargetSize;
    float2   TexelSize;
    float    Radius;
    float    Bias;
    float2   SsaoPadding;
    float4   Kernel[SSAO_KERNEL_SIZE];
};

// Slot 0 is the depth for the occlusion pass and the occlusion for the blur passes.
TEN_PS_TEXTURE(Texture2D, SourceMap, 0);
TEN_PS_TEXTURE(Texture2D, NormalMap, 1);
TEN_PS_TEXTURE(Texture2D, NoiseMap, 2);

typedef FullscreenPixelInput PixelInput;

PixelInput VS(uint vertexId : SV_VertexID)
{
    return FullscreenVS(vertexId);
}

float3 ReconstructViewPosition(float2 uv)
{
    float depth = SourceMap.SampleLevel(SourceMapSampler, uv, 0.0f).x;
    float4 position = mul(float4(uv.x * 2.0f - 1.0f, (1.0f - uv.y) * 2.0f - 1.0f, depth, 1.0f), InverseProjection);
    return position.xyz / position.w;
}

// View space Z only: with a perspective projection it does not depend on the screen position.
float ReconstructViewDepth(float2 uv)
{
    float depth = SourceMap.SampleLevel(SourceMapSampler, uv, 0.0f).x;
    float z = depth * InverseProjection._33 + InverseProjection._43;
    float w = depth * InverseProjection._34 + InverseProjection._44;
    return z / w;
}

// The occlusion target has half the depth resolution, so its pixel centers fall between four depth texels. Position
// and normal come from the center of one texel, otherwise the depth of one texel is placed at the position of another
// and slanted surfaces occlude themselves in stripes.
float2 SnapToDepthTexel(float2 uv)
{
    float width = 0.0f;
    float height = 0.0f;
    SourceMap.GetDimensions(width, height);

    float2 size = float2(width, height);
    return (floor(uv * size) + 0.5f) / size;
}

float PS(PixelInput input) : SV_Target
{
    float2 uv = SnapToDepthTexel(input.UV);
    float3 position = ReconstructViewPosition(uv);
    float3 encodedNormal = NormalMap.SampleLevel(NormalMapSampler, uv, 0.0f).xyz;

    // Sky and far geometry have no occlusion.
    if (length(position) >= 40960.0f || length(encodedNormal) <= 0.0001f)
        return 1.0f;

    float3 normal = encodedNormal * 2.0f - 1.0f;
    float3 randomVector = NoiseMap.SampleLevel(NoiseMapSampler, input.UV * TargetSize / 4.0f, 0.0f).xyz;

    // Hemisphere kernel oriented around the normal, rotated by the tiled noise vector (Gram-Schmidt).
    float3 tangent = normalize(randomVector - normal * dot(randomVector, normal));
    float3 bitangent = cross(normal, tangent);
    bitangent *= rsqrt(max(dot(bitangent, bitangent), 1e-8f));
    float3x3 tangentFrame = float3x3(tangent, bitangent, normal);

    float occlusion = 0.0f;

    [unroll]
    for (int i = 0; i < SSAO_KERNEL_SIZE; i++)
    {
        float3 samplePosition = position + mul(Kernel[i].xyz, tangentFrame) * Radius;

        // Only XY and W are needed from the perspective projection.
        float2 offset = samplePosition.xy * float2(Projection._11, Projection._22) / (samplePosition.z * Projection._34);
        offset = offset * 0.5f + 0.5f;
        offset.y = 1.0f - offset.y;

        // Occluded when the stored surface lies in front of the sample; the range check fades distant occluders.
        float sampleDepth = ReconstructViewDepth(offset);
        float rangeCheck = smoothstep(0.0f, 1.0f, Radius / abs(position.z - sampleDepth));
        occlusion += rangeCheck * step(0.0f, sampleDepth - samplePosition.z - Bias);
    }

    return 1.0f - (occlusion / SSAO_KERNEL_SIZE);
}

float GetGaussian(float x, float sigma)
{
    return 0.39894f * exp(-0.5f * x * x / (sigma * sigma)) / sigma;
}

// Bilateral blur along one axis; two passes approximate the 2D filter. The range weight on the occlusion difference
// keeps edges sharp.
float Blur(float2 uv, float2 direction)
{
    const int halfSize = (BLUR_SIZE - 1) / 2;

    float baseValue = SourceMap.SampleLevel(SourceMapSampler, uv, 0.0f).x;
    float normalization = 1.0f / GetGaussian(0.0f, BLUR_BSIGMA);

    float result = 0.0f;
    float weightSum = 0.0f;

    [unroll]
    for (int i = -halfSize; i <= halfSize; i++)
    {
        float value = SourceMap.SampleLevel(SourceMapSampler, uv + direction * (i * TexelSize), 0.0f).x;
        float weight = GetGaussian((float)i, BLUR_SIGMA) * GetGaussian(value - baseValue, BLUR_BSIGMA) * normalization;

        result += weight * value;
        weightSum += weight;
    }

    return result / weightSum;
}

float PSBlurHorizontal(PixelInput input) : SV_Target
{
    return Blur(input.UV, float2(1.0f, 0.0f));
}

float PSBlurVertical(PixelInput input) : SV_Target
{
    return Blur(input.UV, float2(0.0f, 1.0f));
}
