#ifndef TEN_MATERIALS
#define TEN_MATERIALS

// Surface sampling of the legacy material system (Shaders/Materials.hlsli): color, normal, ORSH and emissive
// maps of an atlas page, parallax occlusion mapping and per-material properties from a frame storage buffer.
// Pixel textures use slots 0-3, slot 4 is the screen space ambient occlusion; storage buffers follow at
// index 5 (lights) and 6 (materials).

#include "./ShaderInterop.hlsli"
#include "./Common.hlsli"

#define MATERIAL_DEFAULT           0
#define MATERIAL_REFLECTIVE        1
#define MATERIAL_SKYBOX_REFLECTIVE 2

#define MATERIAL_TYPE_MASK      0xFF
#define MATERIAL_FLAG_HEIGHTMAP (1 << 8)
#define MATERIAL_FLAG_OCCLUSION (1 << 9)
#define MATERIAL_FLAG_EMISSIVE  (1 << 10)

#define POM_MIN_STEPS      1
#define POM_MAX_STEPS      16
#define POM_FADE_START     5000.0f
#define POM_FADE_END       7000.0f
#define POM_FADE_TOLERANCE 0.001f
#define POM_MIN_ANGLE      0.4f
#define POM_HEIGHT_SCALE   0.0035f

struct GpuMaterial
{
    float4 Properties[4];
    uint   TypeAndFlags;
    uint3  Padding;
};

TEN_PS_TEXTURE(Texture2D, ColorMap, 0);
TEN_PS_TEXTURE(Texture2D, NormalMap, 1);
TEN_PS_TEXTURE(Texture2D, ORSHMap, 2);
TEN_PS_TEXTURE(Texture2D, EmissiveMap, 3);
TEN_PS_TEXTURE(Texture2D, AmbientOcclusionMap, 4);
TEN_PS_STORAGE_BUFFER(GpuMaterial, Materials, 6);

struct Surface
{
    float4 Color;
    float3 Normal;
    float3 Emissive;
    float  Occlusion;
    float  Roughness;
    float  Specular;
};

float3 UnpackNormalMap(float4 n)
{
    n = n * 2.0f - 1.0f;
    n.z = saturate(1.0f - dot(n.xy, n.xy));
    return n.xyz;
}

float3 EnsureNormal(float3 normal, float3 position, float3 cameraPosition)
{
    normal = any(isnan(normal)) ? float3(0.0f, 0.0f, 0.0f) : normal;
    if (dot(normal, normal) >= 0.001f)
        return normalize(normal);

    float3 view = cameraPosition - position;
    return (dot(view, view) >= 0.001f) ? normalize(view) : float3(0.0f, 0.0f, 1.0f);
}

float GetEmissiveIntensity(GpuMaterial material)
{
    return ((material.TypeAndFlags & MATERIAL_TYPE_MASK) == MATERIAL_DEFAULT) ? material.Properties[0].x : 1.0f;
}

float SampleHeight(float2 uv)
{
    return 1.0f - ORSHMap.Sample(ORSHMapSampler, frac(uv)).w;
}

float2 ParallaxOcclusionMapping(GpuMaterial material, float3 tangent, float3 binormal, float3 faceNormal, float3 position, float3 cameraPosition, float2 baseUV)
{
    if (!(material.TypeAndFlags & MATERIAL_FLAG_HEIGHTMAP))
        return baseUV;

    float3 cameraVector = cameraPosition - position;
    float fade = saturate(1.0f - (length(cameraVector) - POM_FADE_START) / (POM_FADE_END - POM_FADE_START));
    if (fade <= POM_FADE_TOLERANCE)
        return baseUV;

    // Orthonormal basis; the binormal keeps the handedness of the TR coordinate system.
    float3 t = normalize(tangent);
    float3 n = normalize(faceNormal);
    float3 recomputedBinormal = cross(n, t);
    float3 b = recomputedBinormal * ((dot(recomputedBinormal, normalize(binormal)) >= 0.0f) ? 1.0f : -1.0f);

    float3 view = normalize(cameraVector);
    float3 viewTangent = -normalize(float3(dot(view, t), dot(view, b), dot(view, n)));
    viewTangent.y = -viewTangent.y;

    float factor = saturate((viewTangent.z - POM_MIN_ANGLE) / (1.0f - POM_MIN_ANGLE));
    int sampleCount = max(1, (int)ceil(lerp((float)POM_MAX_STEPS, (float)POM_MIN_STEPS, factor)));
    viewTangent.z = clamp(viewTangent.z, POM_MIN_ANGLE, 1.0f);

    float layerDepth = 1.0f / sampleCount;
    float2 deltaUV = (viewTangent.xy / viewTangent.z) * POM_HEIGHT_SCALE / sampleCount;

    float currentDepth = 0.0f;
    float2 currentUV = baseUV;
    float currentMapDepth = SampleHeight(currentUV);

    [loop]
    for (int i = 0; i < sampleCount; i++)
    {
        if (currentDepth >= currentMapDepth)
            break;

        currentUV += deltaUV;
        currentDepth += layerDepth;
        currentMapDepth = SampleHeight(currentUV);
    }

    float2 previousUV = currentUV - deltaUV;
    float afterDepth = currentMapDepth - currentDepth;
    float beforeDepth = SampleHeight(previousUV) - (currentDepth - layerDepth);

    float weight = saturate(afterDepth / (afterDepth - beforeDepth + 0.001f));
    return lerp(baseUV, lerp(frac(currentUV), frac(previousUV), weight), fade);
}

// Samples every map of the bucket's atlas page. The normal is perturbed by the normal map in the
// tangent frame; reflections are not ported yet.
Surface SampleSurface(GpuMaterial material, float2 uv, float alphaThreshold, float3 position, float3 cameraPosition,
                      float3 normal, float3 tangent, float3 binormal, float3 faceNormal)
{
    uv = ParallaxOcclusionMapping(material, tangent, binormal, faceNormal, position, cameraPosition, uv);

    Surface surface;
    surface.Color = ColorMap.Sample(ColorMapSampler, uv);
    clip(surface.Color.a - alphaThreshold);

    float4 orsh = ORSHMap.Sample(ORSHMapSampler, uv);
    surface.Occlusion = orsh.x;
    surface.Roughness = orsh.y;
    surface.Specular = orsh.z;
    surface.Emissive = EmissiveMap.Sample(EmissiveMapSampler, uv).rgb * GetEmissiveIntensity(material);

    float3x3 tangentFrame = float3x3(tangent, binormal, normal);
    surface.Normal = EnsureNormal(mul(UnpackNormalMap(NormalMap.Sample(NormalMapSampler, uv)), tangentFrame), position, cameraPosition);
    return surface;
}

// Color alpha test and perturbed normal only, for the depth and normals prepass.
float3 SampleSurfaceNormal(GpuMaterial material, float2 uv, float alphaThreshold, float3 position, float3 cameraPosition,
                           float3 normal, float3 tangent, float3 binormal, float3 faceNormal)
{
    uv = ParallaxOcclusionMapping(material, tangent, binormal, faceNormal, position, cameraPosition, uv);
    clip(ColorMap.Sample(ColorMapSampler, uv).a - alphaThreshold);

    float3x3 tangentFrame = float3x3(tangent, binormal, normal);
    return EnsureNormal(mul(UnpackNormalMap(NormalMap.Sample(NormalMapSampler, uv)), tangentFrame), position, cameraPosition);
}

// Legacy CalculateOcclusion: blended surfaces fade the occlusion with their alpha, parallax surfaces skip it.
float GetAmbientOcclusion(GpuMaterial material, float2 screenUV, float exponent, uint blendMode, float alpha)
{
    bool isSupported = (blendMode == BLENDMODE_OPAQUE || blendMode == BLENDMODE_ALPHATEST || blendMode == BLENDMODE_ALPHABLEND);
    if (exponent <= 0.0f || !isSupported || (material.TypeAndFlags & MATERIAL_FLAG_HEIGHTMAP))
        return 1.0f;

    float occlusion = pow(AmbientOcclusionMap.Sample(AmbientOcclusionMapSampler, screenUV).x, exponent);
    return (blendMode == BLENDMODE_ALPHABLEND) ? lerp(occlusion, 1.0f, alpha) : occlusion;
}

float3 EncodeViewNormal(float3 normal, float4x4 view)
{
    return normalize(mul(normal, (float3x3)view)) * 0.5f + 0.5f;
}

#endif // TEN_MATERIALS
