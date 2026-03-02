#ifndef MATERIALSSHADER
#define MATERIALSSHADER

#include "Blending.glsl"
#include "CBCamera.glsl"
#include "CBMaterial.glsl"

#define MATERIAL_DEFAULT           0
#define MATERIAL_REFLECTIVE        1
#define MATERIAL_SKYBOX_REFLECTIVE 2

#define MATERIAL_FLAG_MASK 0xFF
#define MATERIAL_FLAG_HEIGHTMAP (1 << 8)
#define MATERIAL_FLAG_OCCLUSION (1 << 9)
#define MATERIAL_FLAG_EMISSIVE  (1 << 10)

#define POM_MIN_STEPS 1
#define POM_MAX_STEPS 16
#define POM_FADE_START 5000
#define POM_FADE_END 7000
#define POM_FADE_TOLERANCE 0.001
#define POM_MIN_ANGLE 0.4
#define POM_HEIGHT_SCALE 0.0035

layout(binding = 9) uniform sampler2D SSAOTexture;
layout(binding = 10) uniform sampler2D ORSHTexture;
layout(binding = 11) uniform sampler2D EmissiveTexture;
layout(binding = 12) uniform sampler2D LegacyReflectionsTexture;
layout(binding = 13) uniform sampler2DArray SkyboxReflectionsTexture;

vec2 ToCentralSquare(vec2 uvEnv, float aspect)
{
    if (aspect >= 1.0)
    {
        float sx = 1.0 / aspect;
        return vec2(uvEnv.x * sx + (0.5 - 0.5 * sx), uvEnv.y);
    }
    else
    {
        float sy = aspect;
        return vec2(uvEnv.x, uvEnv.y * sy + (0.5 - 0.5 * sy));
    }
}

vec3 CalculateSkyBoxReflections(vec3 worldPosition, vec3 normal, float specular, vec3 pixelColor)
{
    vec3 V = normalize(CamPositionWS.xyz - worldPosition);
    vec3 R = reflect(-V, normal);

    // Angle exaggeration factor to fake angular reflection difference.
    R = normalize(mix(V, R, 1.5));
    vec3 d = normalize((DualParaboloidView * vec4(R, 0.0)).xyz);

    float denom = max(1.0 + abs(d.z), EPSILON);
    vec2 uv = (d.xy / denom) * 0.5 + 0.5;

    // Crop hemisphere to avoid edge artifacts.
    uv = 0.5 + (uv - 0.5) * 0.985;
    uv = clamp(uv, EPSILON, 1.0 - EPSILON);

    int slice = (d.z <= 0.0) ? 0 : 1;

    vec3 reflectedColor = texture(SkyboxReflectionsTexture, vec3(uv, float(slice))).rgb;
    return mix(pixelColor, reflectedColor, clamp(specular, 0.0, 1.0));
}

vec3 CalculateLegacyReflections(vec3 worldPosition, vec3 normal, float specular, vec3 pixelColor)
{
    vec3 N = normalize((View * vec4(normal, 0.0)).xyz);
    vec3 V = normalize((View * vec4(CamPositionWS.xyz - worldPosition, 0.0)).xyz);
    vec3 R = reflect(-V, N);

    // Project reflection vector into pseudo-screen UVs
    vec2 uv = R.xy * 0.5 + 0.5;

    // Preserve aspect ratio
    uv = ToCentralSquare(uv, AspectRatio);

    // Sample legacy reflection buffer
    vec3 reflectedColor = texture(LegacyReflectionsTexture, uv).rgb;
    return mix(pixelColor, reflectedColor, specular);
}

vec3 CalculateReflections(vec3 position, vec3 color, vec3 normal, float specular)
{
    int materialType = int(MaterialTypeAndFlags & uint(MATERIAL_FLAG_MASK));

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

vec2 ParallaxOcclusionMapping(mat3 TBN, vec3 pos, vec2 baseUV)
{
    if ((MaterialTypeAndFlags & uint(MATERIAL_FLAG_HEIGHTMAP)) == 0u)
        return baseUV;

    vec3 camVector = CamPositionWS.xyz - pos;

    // Discard parallax mapping over the distance.
    float fade = clamp(1.0 - (length(camVector) - float(POM_FADE_START)) / float(POM_FADE_END - POM_FADE_START), 0.0, 1.0);
    if (fade <= POM_FADE_TOLERANCE)
        return baseUV;

    // Build orthonormal TBN basis (fix potential handedness in TR coordinate system).
    vec3 t = normalize(TBN[0]);
    vec3 b = normalize(TBN[1]);
    vec3 n = normalize(TBN[2]);
    vec3 recomputedB = cross(n, t);
    float handedness = dot(recomputedB, b) >= 0.0 ? 1.0 : -1.0;
    b = recomputedB * handedness;

    // Compute view direction in tangent space.
    vec3 viewW = normalize(camVector);
    vec3 viewDirTangent = -normalize(vec3(dot(viewW, t), dot(viewW, b), dot(viewW, n)));

    // Flip tangent-space Y to match inverted TR coordinate system.
    viewDirTangent.y = -viewDirTangent.y;

    // Bring parallax angle to full available range, otherwise effect will be undersampled.
    float factor = clamp((viewDirTangent.z - POM_MIN_ANGLE) / (1.0 - POM_MIN_ANGLE), 0.0, 1.0);
    int numSamples = max(1, int(ceil(mix(float(POM_MAX_STEPS), float(POM_MIN_STEPS), factor))));

    // Clamp steep angles to avoid artifacts.
    viewDirTangent.z = clamp(viewDirTangent.z, POM_MIN_ANGLE, 1.0);

    // Adaptive sample count based on angle.
    float layerDepth = 1.0 / float(numSamples);

    // Parallax amount & delta UV.
    vec2 deltaUV = (viewDirTangent.xy / viewDirTangent.z) * POM_HEIGHT_SCALE / float(numSamples);

    // Iterative depth search.

    float currentDepth = 0.0;
    vec2 currentUV = baseUV;
    vec2 wrappedUV = fract(currentUV);
    float currentMapDepth = 1.0 - texture(ORSHTexture, wrappedUV).w;

    for (int i = 0; i < numSamples; i++)
    {
        if (currentDepth >= currentMapDepth)
            break;

        currentUV += deltaUV;
        currentDepth += layerDepth;

        wrappedUV = fract(currentUV);
        currentMapDepth = 1.0 - texture(ORSHTexture, wrappedUV).w;
    }

    // Linear refinement between last two steps.
    vec2 prevUV = currentUV - deltaUV;
    vec2 wrappedPrevUV = fract(prevUV);
    float mapDepthPrev = 1.0 - texture(ORSHTexture, wrappedPrevUV).w;

    float afterDepth = currentMapDepth - currentDepth;
    float beforeDepth = mapDepthPrev - (currentDepth - layerDepth);

    float weight = clamp(afterDepth / (afterDepth - beforeDepth + EPSILON), 0.0, 1.0);
    vec2 finalUV = mix(fract(currentUV), wrappedPrevUV, weight);

    // Distance fade between parallax UV and flat UV.
    return mix(baseUV, finalUV, fade);
}

float CalculateOcclusion(vec2 samplePosition, float alpha)
{
    if (AmbientOcclusion == 0 || !BlendModeSupportsSSAO() || (MaterialTypeAndFlags & uint(MATERIAL_FLAG_HEIGHTMAP)) != 0u)
        return 1.0;

    float occlusion = pow(texture(SSAOTexture, samplePosition).x, float(AmbientOcclusionExponent));

    if (BlendMode == uint(BLENDMODE_ALPHABLEND))
        occlusion = mix(occlusion, 1.0, alpha);

    return occlusion;
}

vec3 EnsureNormal(vec3 n, vec3 worldPos)
{
    // Eliminate NaNs by replacing them with 0.
    n = mix(vec3(0, 0, 0), n, vec3(equal(n, n)));

    float l2 = dot(n, n);

    // If too small, choose a fallback facing the camera
    if (l2 < EPSILON)
    {
        // Camera direction in world space
        vec3 viewDir = normalize(CamPositionWS.xyz - worldPos);

        // Guarantee a valid vector even if Cam == worldPos
        if (any(isnan(viewDir)) || dot(viewDir, viewDir) < EPSILON)
            viewDir = vec3(0, 0, 1);

        return viewDir;
    }

    return normalize(n);
}

#endif
