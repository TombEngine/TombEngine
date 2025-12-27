#ifndef MATERIALSSHADER
#define MATERIALSSHADER

#include "./Blending.hlsli"
#include "./CBCamera.hlsli"
#include "./CBMaterial.hlsli"

#define MATERIAL_DEFAULT           0
#define MATERIAL_REFLECTIVE        1
#define MATERIAL_SKYBOX_REFLECTIVE 2

#define MATERIAL_FLAG_MASK 0xFF
#define MATERIAL_FLAG_HEIGHTMAP 1 << 8
#define MATERIAL_FLAG_OCCLUSION 1 << 9
#define MATERIAL_FLAG_EMISSIVE  1 << 10

#define POM_MIN_STEPS 1
#define POM_MAX_STEPS 16
#define POM_FADE_START 5000
#define POM_FADE_END 7000
#define POM_FADE_TOLERANCE 0.001f
#define POM_MIN_ANGLE 0.4f
#define POM_HEIGHT_SCALE 0.0035f

Texture2D SSAOTexture : register(t9);
SamplerState SSAOSampler : register(s9);

Texture2D ORSHTexture : register(t10);
SamplerState ORSHSampler : register(s10);

Texture2D EmissiveTexture : register(t11);
SamplerState EmissiveSampler : register(s11);

Texture2D LegacyReflectionsTexture : register(t12);
SamplerState LegacyReflectionsSampler : register(s12);

Texture2DArray SkyboxReflectionsTexture : register(t13);
SamplerState SkyboxReflectionsSampler : register(s13);

float2 ToCentralSquare(float2 uvEnv, float aspect)
{
    if (aspect >= 1.0)
    {
        float sx = rcp(aspect);
        return float2(uvEnv.x * sx + (0.5 - 0.5 * sx), uvEnv.y);
    }
    else
    {
        float sy = aspect;
        return float2(uvEnv.x, uvEnv.y * sy + (0.5 - 0.5 * sy));
    }
}

float3 CalculateSkyBoxReflections(float3 worldPosition, float3 normal, float specular, float3 pixelColor)
{
    float3 V = normalize(CamPositionWS - worldPosition);
    float3 R = reflect(-V, normal);

    // Angle exaggeration factor to fake angular reflection difference.
    R = normalize(lerp(V, R, 1.5f));
    float3 d = normalize(mul(float4(R, 0.0f), DualParaboloidView).xyz);

    float denom = max(1.0f + abs(d.z), EPSILON);
    float2 uv = (d.xy / denom) * 0.5f + 0.5f;

    // Crop hemisphere to avoid edge artifacts.
    uv = 0.5f + (uv - 0.5f) * 0.985f;
    uv = clamp(uv, EPSILON, 1.0f - EPSILON);

    int slice = (d.z <= 0.0f) ? 0 : 1;
    uv.y = 1.0f - uv.y;
    
    float3 reflectedColor = SkyboxReflectionsTexture.Sample(SkyboxReflectionsSampler, float3(uv, slice)).rgb;
    return lerp(pixelColor, reflectedColor, saturate(specular));
}

float3 CalculateLegacyReflections(float3 worldPosition, float3 normal, float specular, float3 pixelColor)
{
    float3 N = normalize(mul(float4(normal, 0.0f), View).xyz);
    float3 V = normalize(mul(float4(CamPositionWS - worldPosition, 0.0f), View).xyz);
    float3 R = reflect(-V, N);

    // Project reflection vector into pseudo-screen UVs
    float2 uv = R.xy * 0.5f + 0.5f;
    uv.y = 1.0f - uv.y;

    // Preserve aspect ratio
    uv = ToCentralSquare(uv, AspectRatio);

    // Sample legacy reflection buffer
    float3 reflectedColor = LegacyReflectionsTexture.Sample(LegacyReflectionsSampler, uv).rgb;
    return lerp(pixelColor, reflectedColor, specular);
}


float3 CalculateReflections(float3 position, float3 color, float3 normal, float specular)
{
    int materialType = MaterialTypeAndFlags & MATERIAL_FLAG_MASK;
	
    if (materialType == MATERIAL_SKYBOX_REFLECTIVE)
    {
        return CalculateSkyBoxReflections(position, normal, specular, color);
    }
    else if (materialType == MATERIAL_REFLECTIVE)
    {
        return CalculateLegacyReflections(position, normal, specular, color);
    }
    else
    {
        return color;
    }
}

float2 ParallaxOcclusionMapping(float3x3 TBN, float3 pos, float2 baseUV)
{
    if (!(MaterialTypeAndFlags & MATERIAL_FLAG_HEIGHTMAP))
        return baseUV;

    float3 camVector = CamPositionWS - pos;
	
    // Discard parallax mapping over the distance.
    float fade = saturate(1.0f - (length(camVector) - POM_FADE_START) / (POM_FADE_END - POM_FADE_START));
    if (fade <= POM_FADE_TOLERANCE)
        return baseUV;

    // Build orthonormal TBN basis (fix potential handedness in TR coordinate system).
    float3 t = normalize(TBN[0]);
    float3 b = normalize(TBN[1]);
    float3 n = normalize(TBN[2]);
    float3 recomputedB = cross(n, t);
    float handedness = dot(recomputedB, b) >= 0.0f ? 1.0f : -1.0f;
    b = recomputedB * handedness;

    // Compute view direction in tangent space.
    float3 viewW = normalize(camVector);
    float3 viewDirTangent = -normalize(float3(dot(viewW, t), dot(viewW, b), dot(viewW, n)));

    // Flip tangent-space Y to match inverted TR coordinate system.
    viewDirTangent.y = -viewDirTangent.y;

    // Bring parallax angle to full available range, otherwise effect will be undersampled.
    float factor = saturate((viewDirTangent.z - POM_MIN_ANGLE) / (1.0f - POM_MIN_ANGLE));
    int numSamples = max(1, (int)ceil(lerp(POM_MAX_STEPS, POM_MIN_STEPS, factor)));

    // Clamp steep angles to avoid artifacts.
    viewDirTangent.z = clamp(viewDirTangent.z, POM_MIN_ANGLE, 1.0f);

    // Adaptive sample count based on angle.
    float layerDepth = 1.0f / numSamples;

    // Parallax amount & delta UV.
    float2 deltaUV = (viewDirTangent.xy / viewDirTangent.z) * POM_HEIGHT_SCALE / numSamples;

    // Iterative depth search.
	
    float currentDepth = 0.0f;
    float2 currentUV = baseUV;
    float2 wrappedUV = frac(currentUV);
    float currentMapDepth = 1.0f - ORSHTexture.Sample(ORSHSampler, wrappedUV).w;

    [loop]
    for (int i = 0; i < numSamples; i++)
    {
        if (currentDepth >= currentMapDepth)
            break;

        currentUV += deltaUV;
        currentDepth += layerDepth;

        wrappedUV = frac(currentUV);
        currentMapDepth = 1.0f - ORSHTexture.Sample(ORSHSampler, wrappedUV).w;
    }

    // Linear refinement between last two steps.
    float2 prevUV = currentUV - deltaUV;
    float2 wrappedPrevUV = frac(prevUV);
    float mapDepthPrev = 1.0f - ORSHTexture.Sample(ORSHSampler, wrappedPrevUV).w;

    float afterDepth = currentMapDepth - currentDepth;
    float beforeDepth = mapDepthPrev - (currentDepth - layerDepth);

    float weight = saturate(afterDepth / (afterDepth - beforeDepth + EPSILON));
    float2 finalUV = lerp(frac(currentUV), wrappedPrevUV, weight);

    // Distance fade between parallax UV and flat UV.
    return lerp(baseUV, finalUV, fade);
}

float CalculateOcclusion(float2 samplePosition, float alpha)
{
    if (AmbientOcclusion == 0 || !BlendModeSupportsSSAO() || (MaterialTypeAndFlags & MATERIAL_FLAG_HEIGHTMAP))
		return 1.0f;
		
	float occlusion = pow(SSAOTexture.Sample(SSAOSampler, samplePosition).x, AmbientOcclusionExponent);
	
	if (BlendMode == BLENDMODE_ALPHABLEND)
		occlusion = lerp(occlusion, 1.0f, alpha);
	
	return occlusion;
}

inline float3 EnsureNormal(float3 n, float3 worldPos)
{
    // Eliminate NaNs by replacing them with 0.
    n = (n == n) ? n : float3(0, 0, 0);

    float l2 = dot(n, n);

    // If too small, choose a fallback facing the camera
    if (l2 < EPSILON)
    {
        // Camera direction in world space
        float3 viewDir = normalize(CamPositionWS - worldPos);

        // Guarantee a valid vector even if Cam == worldPos
        if (any(isnan(viewDir)) || dot(viewDir,viewDir) < EPSILON)
            viewDir = float3(0, 0, 1);

        return viewDir;
    }

    return normalize(n);
}

#endif