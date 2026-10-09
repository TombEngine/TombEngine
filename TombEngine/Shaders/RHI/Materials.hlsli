#ifndef TEN_MATERIALS
#define TEN_MATERIALS

// Surface sampling of the legacy material system (Shaders/Materials.hlsli): color, normal, ORSH and emissive
// maps of an atlas page, parallax occlusion mapping and per-material properties from a frame storage buffer.
// Pixel textures use slots 0-3, slot 4 is the screen space ambient occlusion, slot 5 the shadow map
// (Shadows.hlsli), slot 6 the caustics, slot 7 the previous frame reflections and slot 8 the sky reflections;
// storage buffers follow at index 9 (lights), 10 (materials) and 11 (decals, Decals.hlsli).

#include "./ShaderInterop.hlsli"
#include "./Common.hlsli"
#include "./FrameUniforms.hlsli"

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

// Material maps are texture arrays shared by the pages of the same size and format; TextureLayer selects the page.
TEN_PS_TEXTURE(Texture2DArray, ColorMap, 0);
TEN_PS_TEXTURE(Texture2DArray, NormalMap, 1);
TEN_PS_TEXTURE(Texture2DArray, ORSHMap, 2);
TEN_PS_TEXTURE(Texture2DArray, EmissiveMap, 3);
TEN_PS_TEXTURE(Texture2D, AmbientOcclusionMap, 4);
TEN_PS_TEXTURE(Texture2D, CausticsMap, 6);
TEN_PS_TEXTURE(Texture2D, ReflectionMap, 7); // Previous frame, half resolution.
TEN_PS_TEXTURE(Texture2DArray, SkyReflectionMap, 8); // Dual paraboloid sky, one hemisphere per layer.
TEN_PS_STORAGE_BUFFER(GpuMaterial, Materials, 10);

float4 SampleColorMap(float2 uv)
{
    return ColorMap.Sample(ColorMapSampler, float3(uv, TextureLayer));
}

float4 SampleNormalMap(float2 uv)
{
    return NormalMap.Sample(NormalMapSampler, float3(uv, TextureLayer));
}

float4 SampleORSHMap(float2 uv)
{
    return ORSHMap.Sample(ORSHMapSampler, float3(uv, TextureLayer));
}

float4 SampleEmissiveMap(float2 uv)
{
    return EmissiveMap.Sample(EmissiveMapSampler, float3(uv, TextureLayer));
}

struct Surface
{
    float4 Color;
    float3 Normal;
    float3 Emissive;
    float  Occlusion;
    float  Roughness;
    float  Specular;
};

// A negative threshold keeps only the texels below its magnitude: the soft edge pass of alpha tested buckets.
// The NO_ALPHA_TEST permutation has no clip, so draws without alpha test keep early depth testing.
void ApplyAlphaTest(float alpha, float alphaThreshold)
{
#ifndef NO_ALPHA_TEST
    clip((alphaThreshold >= 0.0f) ? (alpha - alphaThreshold) : (-alphaThreshold - alpha - 0.0001f));
#endif
}

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
    return 1.0f - SampleORSHMap(frac(uv)).w;
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
    // Clamped after the conversion: a NaN view vector would otherwise give an unbounded loop.
    int sampleCount = clamp((int)ceil(lerp((float)POM_MAX_STEPS, (float)POM_MIN_STEPS, factor)), POM_MIN_STEPS, POM_MAX_STEPS);
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

// Samples every map of the bucket's atlas page. The normal is perturbed by the normal map in the tangent frame.
Surface SampleSurface(GpuMaterial material, float2 uv, float alphaThreshold, float3 position, float3 cameraPosition,
                      float3 normal, float3 tangent, float3 binormal, float3 faceNormal)
{
    uv = ParallaxOcclusionMapping(material, tangent, binormal, faceNormal, position, cameraPosition, uv);

    Surface surface;
    surface.Color = SampleColorMap(uv);
    ApplyAlphaTest(surface.Color.a, alphaThreshold);

    float4 orsh = SampleORSHMap(uv);
    surface.Occlusion = orsh.x;
    surface.Roughness = orsh.y;
    surface.Specular = orsh.z;
    surface.Emissive = SampleEmissiveMap(uv).rgb * GetEmissiveIntensity(material);

    float3x3 tangentFrame = float3x3(tangent, binormal, normal);
    surface.Normal = EnsureNormal(mul(UnpackNormalMap(SampleNormalMap(uv)), tangentFrame), position, cameraPosition);
    return surface;
}

// Prepass surface: normal and emissive, the latter feeding the glow as the legacy GBuffer emissive target.
float3 SampleSurfaceNormal(GpuMaterial material, float2 uv, float alphaThreshold, float3 position, float3 cameraPosition,
                           float3 normal, float3 tangent, float3 binormal, float3 faceNormal, out float3 emissive)
{
    uv = ParallaxOcclusionMapping(material, tangent, binormal, faceNormal, position, cameraPosition, uv);
    ApplyAlphaTest(SampleColorMap(uv).a, alphaThreshold);

    emissive = SampleEmissiveMap(uv).rgb * GetEmissiveIntensity(material);

    float3x3 tangentFrame = float3x3(tangent, binormal, normal);
    return EnsureNormal(mul(UnpackNormalMap(SampleNormalMap(uv)), tangentFrame), position, cameraPosition);
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

// Legacy CalculateSkyBoxReflections: the exaggerated reflection vector picks a texel of the dual paraboloid sky.
// The legacy paraboloid view looks down -Y with X up, which maps a direction to (z, x, y).
float3 SampleSkyReflection(float3 position, float3 cameraPosition, float3 normal)
{
    float3 viewVector = normalize(cameraPosition - position);
    float3 reflection = normalize(lerp(viewVector, reflect(-viewVector, normal), 1.5f));
    float3 direction = normalize(reflection.zxy);

    float2 uv = (direction.xy / max(1.0f + abs(direction.z), 1e-6f)) * 0.5f + 0.5f;
    uv = clamp(0.5f + (uv - 0.5f) * 0.985f, 1e-6f, 1.0f - 1e-6f); // Crop hemisphere edges.
    uv.y = 1.0f - uv.y;
    return SkyReflectionMap.Sample(SkyReflectionMapSampler, float3(uv, (direction.z <= 0.0f) ? 0.0f : 1.0f)).rgb;
}

// Legacy CalculateLegacyReflections: the view space reflection vector picks a texel of the previous frame,
// cropped to its central square.
float3 SampleScreenReflection(float3 position, float3 cameraPosition, float3 normal, float4x4 view, float aspectRatio)
{
    float3 viewNormal = normalize(mul(float4(normal, 0.0f), view).xyz);
    float3 viewVector = normalize(mul(float4(cameraPosition - position, 0.0f), view).xyz);
    float3 reflection = reflect(-viewVector, viewNormal);

    float2 uv = reflection.xy * 0.5f + 0.5f;
    uv.y = 1.0f - uv.y;
    if (aspectRatio >= 1.0f)
        uv.x = uv.x / aspectRatio + (0.5f - 0.5f / aspectRatio);
    else
        uv.y = uv.y * aspectRatio + (0.5f - 0.5f * aspectRatio);

    return ReflectionMap.Sample(ReflectionMapSampler, uv).rgb;
}

float3 ApplyReflections(GpuMaterial material, float3 color, float3 position, float3 cameraPosition, float3 normal, float specular,
                        float4x4 view, float aspectRatio)
{
    uint type = material.TypeAndFlags & MATERIAL_TYPE_MASK;
    if (type != MATERIAL_REFLECTIVE && type != MATERIAL_SKYBOX_REFLECTIVE)
        return color;

    float3 reflectedColor = (type == MATERIAL_SKYBOX_REFLECTIVE) ?
        SampleSkyReflection(position, cameraPosition, normal) :
        SampleScreenReflection(position, cameraPosition, normal, view, aspectRatio);
    reflectedColor *= saturate(material.Properties[0].rgb);

    // Vertical mask: offset in Properties[1].x, signed feather in Properties[2].x.
    float mask = 1.0f;
    float bias = material.Properties[2].x;
    if (abs(bias) > 1e-6f)
        mask = saturate(((bias < 0.0f) ? (position.y - material.Properties[1].x) : (material.Properties[1].x - position.y)) / abs(bias));

    return lerp(color, reflectedColor, saturate(specular) * material.Properties[0].a * mask);
}

struct PrepassOutput
{
    float4 Normal   : SV_Target0;
    float4 Emissive : SV_Target1;
};

// Distance fog fades the emissive as in the legacy GBuffer pass. The normal alpha is the outdoor mask of the legacy
// GBuffer, read by the dust storm.
PrepassOutput GetPrepassOutput(float3 viewNormal, float3 emissive, float fog, bool isOutdoor)
{
    PrepassOutput output;
    output.Normal = float4(viewNormal, isOutdoor ? 1.0f : 0.0f);
    output.Emissive = float4(emissive * (1.0f - fog * fog), 1.0f);
    return output;
}

float3 EncodeViewNormal(float3 normal, float4x4 view)
{
    return normalize(mul(normal, (float3x3)view)) * 0.5f + 0.5f;
}

#endif // TEN_MATERIALS
