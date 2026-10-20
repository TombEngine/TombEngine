// TEN_SHADER: Vertex VS samplers=0 storage_textures=0 storage_buffers=0 uniform_buffers=1
// TEN_SHADER: Pixel PS samplers=5 storage_textures=0 storage_buffers=2 uniform_buffers=2
// TEN_SHADER: Pixel PSReflection samplers=4 storage_textures=0 storage_buffers=0 uniform_buffers=2
// TEN_SHADER: Vertex VSFullscreen samplers=0 storage_textures=0 storage_buffers=0 uniform_buffers=0
// TEN_SHADER: Pixel PSUnderwaterFog samplers=2 storage_textures=0 storage_buffers=0 uniform_buffers=3

// Water surfaces generated at load on the portals between water rooms and the dry rooms above them. The surface grid
// rises and falls with a few swells, and two layers of the wave normal map add the ripples. The opaque scene is
// copied before the surfaces are drawn: refraction samples it with the wave normals, screen space reflections march
// its depth at half resolution (PSReflection). Rays leaving the screen fall back to the sky or the room ambient light,
// hidden by the Fresnel term and a per pixel jitter. Water seen from above absorbs light with its depth; from below,
// total internal reflection mirrors the water beyond the critical angle, and the underwater fog (PSUnderwaterFog)
// absorbs the light between the camera and the scene. Room lights add specular highlights through the light clusters.
// Drawn by SceneRendererWater.cpp: VS with PSReflection, then with PS; VSFullscreen with PSUnderwaterFog.

#include "./ShaderInterop.hlsli"
#include "./Common.hlsli"
#include "./Fullscreen.hlsli"
#include "./Lighting.hlsli"

TEN_VS_UNIFORMS(FrameVertexUniforms, 0)
{
    TEN_FRAME_VERTEX_UNIFORMS
};

TEN_PS_UNIFORMS(FramePixelUniforms, 0)
{
    TEN_FRAME_PIXEL_UNIFORMS
};

TEN_PS_UNIFORMS(WaterPixelUniforms, 1)
{
    TEN_WATER_PIXEL_UNIFORMS
};

TEN_PS_UNIFORMS(UnderwaterFogUniforms, 2)
{
    TEN_UNDERWATER_FOG_UNIFORMS
};

TEN_PS_TEXTURE(Texture2D, SceneColor, 0);
TEN_PS_TEXTURE(Texture2D, SceneDepth, 1);
TEN_PS_TEXTURE(Texture2D, WaveNormals, 2);
TEN_PS_TEXTURE(Texture2DArray, SkyReflectionMap, 3);
TEN_PS_TEXTURE(Texture2D, ReflectionTrace, 4);
TEN_PS_RAW_BUFFER(GpuLight, Lights, 5);
TEN_PS_RAW_BUFFER(uint2, LightClusters, 6);

#include "./LightClusters.hlsli"

#define WATER_UP                    float3(0.0f, -1.0f, 0.0f) // World Y points down.
#define WATER_BLOCK                 1024.0f
#define WATER_FRAMES_PER_SECOND     30.0f
#define WATER_TWO_PI                6.28318531f
#define WATER_GRAVITY               5023.0f // World units per squared second, at 512 units per meter.
#define WATER_SWELL_COUNT           4
#define WATER_SWELL_SPEED           0.6f    // Slower than deep water swells, which read too fast in small pools.
#define WATER_FOG_DENSITY           0.35f   // Below the surface, of the absorption: the scene stays readable for several blocks.
#define WATER_FRESNEL_F0            0.04f
#define WATER_FRESNEL_POWER         3.0f
#define WATER_INDEX                 1.33f
#define WATER_SNELL_EDGE_SOFTNESS   0.3f    // Transmitted sine range over which the edge of the Snell window fades.
#define WATER_SHININESS             512.0f
#define WATER_SHORE_DEPTH           96.0f
#define WATER_REFRACTION_FADE_DEPTH 8192.0f // Distant water is refracted less, as its waves get smaller on screen.
#define WATER_SECOND_WAVE_SCALE     0.37f
#define WATER_THIRD_WAVE_SCALE      0.13f
#define WATER_CHOPPINESS_SCALE      7.0f    // Wave tiles across a patch of calmer or choppier ripples.
#define WATER_SSR_STEPS             10
#define WATER_SSR_REFINE_STEPS      3
#define WATER_SSR_THICKNESS         128.0f
#define WATER_SSR_EDGE_FADE         0.1f
#define WATER_SSR_FRESNEL_MIN       0.08f
#define WATER_SKY_DEPTH             0.99999f

// Swells: xy direction, z wavelength, w amplitude in world units.
static const float4 WATER_SWELLS[WATER_SWELL_COUNT] =
{
    float4( 0.60f, 0.80f, 3100.0f, 9.0f),
    float4( 0.95f, 0.31f, 1900.0f, 6.0f),
    float4(-0.20f, 0.98f, 1300.0f, 4.0f),
    float4( 0.79f,-0.61f,  830.0f, 2.5f)
};

struct VertexInput
{
    TEN_LOCATION(0) float4 Position : TEXCOORD0; // W: swell weight, zero on the open edges of the surface.
};

struct PixelInput
{
    float4 Position      : SV_POSITION;
    float3 WorldPosition : TEXCOORD0;
    float  ViewDepth     : TEXCOORD1;
    float  Fog           : TEXCOORD2;
    float  RestHeight    : TEXCOORD3;
    float  SwellWeight   : TEXCOORD4;
};

// Height above the rest surface of a few swells with the deep water dispersion, and its slope. Each swell grows and
// fades across the surface and in time, so their sum does not read as a regular pattern.
float GetSwellHeight(float2 position, float time, out float2 slope)
{
    float height = 0.0f;
    slope = float2(0.0f, 0.0f);

    [unroll]
    for (int i = 0; i < WATER_SWELL_COUNT; i++)
    {
        float4 swell = WATER_SWELLS[i];
        float number = WATER_TWO_PI / swell.z;
        // Deep water dispersion: phase speed sqrt(g / k), with k the wave number.
        float speed = sqrt(WATER_GRAVITY / number) * WATER_SWELL_SPEED;
        float phase = number * (dot(swell.xy, position) - speed * time) + (float)i * 1.9f;
        float group = 0.55f + 0.45f * sin(dot(float2(-swell.y, swell.x), position) * 0.0003f * (float)(i + 1) + time * 0.2f + (float)i * 2.3f);

        float amplitude = swell.w * group;
        height += amplitude * sin(phase);
        slope += swell.xy * (amplitude * number * cos(phase));
    }

    return height;
}

// Displaces the surface grid by the swells (subtracted, as world Y points down).
PixelInput VS(VertexInput input)
{
    float2 slope;
    float3 position = input.Position.xyz;
    position.y -= GetSwellHeight(position.xz, InterpolatedFrame / WATER_FRAMES_PER_SECOND, slope) * input.Position.w;

    PixelInput output;
    output.Position = mul(float4(position, 1.0f), ViewProjection);
    output.WorldPosition = position;
    output.ViewDepth = output.Position.w;
    output.Fog = GetDistanceFog(position, CameraPosition, FogMinDistance, FogMaxDistance);
    output.RestHeight = input.Position.y;
    output.SwellWeight = input.Position.w;
    return output;
}

FullscreenPixelInput VSFullscreen(uint vertexId : SV_VertexID)
{
    return FullscreenVS(vertexId);
}

// View depth of a depth buffer value, for the right handed projection with depth from 0 to 1.
float GetViewDepth(float depth)
{
    return (WaterNearPlane * WaterFarPlane) / (WaterFarPlane - depth * (WaterFarPlane - WaterNearPlane));
}

float GetSceneViewDepth(float2 uv)
{
    return GetViewDepth(SceneDepth.SampleLevel(SceneDepthSampler, uv, 0.0f).r);
}

float2 GetClipUV(float4 clip)
{
    float2 ndc = clip.xy / clip.w;
    return float2(ndc.x * 0.5f + 0.5f, 0.5f - ndc.y * 0.5f);
}

// Fades the texels near the screen borders, where reflected rays lose their surroundings.
float GetEdgeFade(float2 uv)
{
    float2 distance = min(uv, 1.0f - uv);
    return saturate(min(distance.x, distance.y) / WATER_SSR_EDGE_FADE);
}

// Interleaved gradient noise, constant in screen space.
float GetJitter(float2 pixel)
{
    return frac(52.9829189f * frac(dot(pixel, float2(0.06711056f, 0.00583715f))));
}

float3 GetDeepWaterColor()
{
    return WaterColor * WaterAmbient * COLOR_MODULATION_SCALE;
}

// Light crossing a length of water is absorbed and replaced by the light the water scatters.
float3 ApplyWaterAbsorption(float3 color, float pathLength, float density)
{
    float3 transmittance = exp(-WaterAbsorption * density * pathLength / WATER_BLOCK);
    return color * transmittance + GetDeepWaterColor() * (1.0f - transmittance);
}

float2 SampleWaveSlope(float2 uv)
{
    return WaveNormals.Sample(WaveNormalsSampler, uv).rg * 2.0f - 1.0f;
}

// Three layers of the wave normal map scrolling across each other, in drifting patches of calmer and choppier water,
// on top of the slope of the swells.
float3 GetWaveNormal(float3 position, float swellWeight)
{
    float2 flow = WaterFlowDirection * WaterFlowSpeed * WaterTime;
    float2 crossFlow = float2(-WaterFlowDirection.y, WaterFlowDirection.x) * WaterFlowSpeed * WaterTime;

    float2 uv = position.xz / WaterWaveTile;
    float2 slope = SampleWaveSlope(uv + flow) +
                   SampleWaveSlope(uv / WATER_SECOND_WAVE_SCALE - crossFlow * 0.6f) * 0.5f +
                   SampleWaveSlope(uv / WATER_THIRD_WAVE_SCALE + (flow - crossFlow) * 1.7f) * 0.25f;

    float choppiness = 0.4f + length(SampleWaveSlope(uv / WATER_CHOPPINESS_SCALE + crossFlow * 0.15f));
    slope *= WaterWaveStrength * choppiness;

    float2 swellSlope;
    GetSwellHeight(position.xz, WaterTime, swellSlope);
    slope += swellSlope * swellWeight;

    return normalize(float3(-slope.x, -1.0f, -slope.y));
}

// Sky texel in the screen direction of a distant ray, then the sky reflection map for rays going up outdoors, the
// ambient light otherwise. Below the surface, rays reflected back down see deep water.
float3 GetEnvironmentReflection(float3 direction, bool isCameraBelow)
{
    if (isCameraBelow)
        return GetDeepWaterColor();

    float3 environment = (IsWaterSkyReflected && direction.y < 0.0f) ?
        SkyReflectionMap.SampleLevel(SkyReflectionMapSampler, GetSkyReflectionCoordinates(direction), 0.0f).rgb :
        WaterAmbient;

    float4 clip = mul(float4(direction, 0.0f), WaterViewProjection);
    if (clip.w <= 0.0f)
        return environment;

    float2 uv = GetClipUV(clip);
    if (any(uv < 0.0f) || any(uv > 1.0f) || SceneDepth.SampleLevel(SceneDepthSampler, uv, 0.0f).r < WATER_SKY_DEPTH)
        return environment;

    return lerp(environment, SceneColor.SampleLevel(SceneColorSampler, uv, 0.0f).rgb, GetEdgeFade(uv));
}

float GetRayViewDepth(float startInverseDepth, float endInverseDepth, float t)
{
    return 1.0f / lerp(startInverseDepth, endInverseDepth, t);
}

// March in screen space with perspective correct depth, denser near the surface where the reflections of the shore
// are. Each step that crosses behind the depth buffer is refined by a binary search; a crossing deeper than the
// thickness passes behind an object and the march goes on. Returns the reflected color and its confidence, and the
// distance the ray travelled to its hit.
float4 TraceScreenReflection(float3 position, float3 direction, float jitter, out float hitDistance)
{
    hitDistance = 0.0f;

    float rayLength = WaterFarPlane * 2.0f;
    float4 startClip = mul(float4(position, 1.0f), WaterViewProjection);
    float4 endClip = mul(float4(position + direction * rayLength, 1.0f), WaterViewProjection);

    // Rays reach as far as the view; rays toward the camera end at the near plane.
    if (endClip.w < WaterNearPlane)
    {
        float nearT = (startClip.w - WaterNearPlane) / max(startClip.w - endClip.w, 1e-4f);
        endClip = lerp(startClip, endClip, nearT);
        rayLength *= nearT;
    }

    // View depth is linear along the world ray: it gives back the distance of a hit.
    float depthRange = endClip.w - startClip.w;

    float2 startUV = GetClipUV(startClip);
    float2 endUV = GetClipUV(endClip);
    float startInverseDepth = 1.0f / startClip.w;
    float endInverseDepth = 1.0f / endClip.w;

    // Rays leaving the screen are cut at its border, so their last steps still sample the depth buffer.
    float2 delta = endUV - startUV;
    float2 safeDelta = sign(delta) * max(abs(delta), 1e-6f) + (delta == 0.0f) * 1e-6f;
    float2 exitT = (step(0.0f, delta) - startUV) / safeDelta;
    float screenT = saturate(min(exitT.x, exitT.y));
    endUV = startUV + delta * screenT;
    endInverseDepth = lerp(startInverseDepth, endInverseDepth, screenT);

    float previousT = 0.0f;
    bool wasInFront = true;

    [loop]
    for (int i = 1; i <= WATER_SSR_STEPS; i++)
    {
        float step = ((float)i - jitter) / WATER_SSR_STEPS;
        float t = step * step;
        float2 uv = lerp(startUV, endUV, t);
        if (any(uv < 0.0f) || any(uv > 1.0f))
            break;

        float rayDepth = GetRayViewDepth(startInverseDepth, endInverseDepth, t);
        bool isInFront = (rayDepth <= GetSceneViewDepth(uv));
        if (!isInFront && wasInFront)
        {
            float low = previousT;
            float high = t;

            [unroll]
            for (int j = 0; j < WATER_SSR_REFINE_STEPS; j++)
            {
                float middle = (low + high) * 0.5f;
                if (GetRayViewDepth(startInverseDepth, endInverseDepth, middle) > GetSceneViewDepth(lerp(startUV, endUV, middle)))
                    high = middle;
                else
                    low = middle;
            }

            // Rays nearly parallel to the view span a long depth in one texel: the hit is valid when the scene is not in
            // front of the refined segment, otherwise the ray passed behind an object.
            float2 hitUV = lerp(startUV, endUV, high);
            float frontDepth = GetRayViewDepth(startInverseDepth, endInverseDepth, low);
            if (GetSceneViewDepth(hitUV) > frontDepth - max(WATER_SSR_THICKNESS, frontDepth * 0.05f))
            {
                float hitDepth = GetRayViewDepth(startInverseDepth, endInverseDepth, high);
                float rayT = (abs(depthRange) > 1.0f) ? (hitDepth - startClip.w) / depthRange : high * screenT;
                hitDistance = saturate(rayT) * rayLength;
                return float4(SceneColor.SampleLevel(SceneColorSampler, hitUV, 0.0f).rgb, GetEdgeFade(hitUV));
            }
        }

        wasInFront = isInFront;
        previousT = t;
    }

    return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

// Softer than the physical fifth power, so reflections read on clear shallow water away from grazing angles.
float GetSchlickFresnel(float cosine)
{
    return WATER_FRESNEL_F0 + (1.0f - WATER_FRESNEL_F0) * pow(1.0f - cosine, WATER_FRESNEL_POWER);
}

// Water to air reflectance of unpolarized light, total beyond the critical angle, where the sine of the transmitted
// ray would exceed 1. The edge of the Snell window fades in before the critical angle, as the scattering of real
// water blurs it.
float GetUnderwaterFresnel(float cosine)
{
    float transmittedSine2 = WATER_INDEX * WATER_INDEX * (1.0f - cosine * cosine);
    if (transmittedSine2 >= 1.0f)
        return 1.0f;

    float transmittedCosine = sqrt(1.0f - transmittedSine2);
    float perpendicular = (WATER_INDEX * cosine - transmittedCosine) / (WATER_INDEX * cosine + transmittedCosine);
    float parallel = (cosine - WATER_INDEX * transmittedCosine) / (cosine + WATER_INDEX * transmittedCosine);
    float reflectance = (perpendicular * perpendicular + parallel * parallel) * 0.5f;
    return max(reflectance, smoothstep(1.0f - WATER_SNELL_EDGE_SOFTNESS, 1.0f, sqrt(transmittedSine2)));
}

float GetFresnel(float3 normal, float3 viewVector, bool isCameraBelow)
{
    float cosine = saturate(abs(dot(normal, viewVector)));
    return isCameraBelow ? GetUnderwaterFresnel(cosine) : GetSchlickFresnel(cosine);
}

float3 GetLightSpecular(GpuLight light, float3 position, float3 normal, float3 viewVector)
{
    float3 lightDirection;
    float attenuation = 1.0f;

    if (light.Type == LT_SUN)
    {
        lightDirection = -light.Direction;
    }
    else if (light.Type == LT_POINT || light.Type == LT_SPOT)
    {
        float3 lightVector = light.Position - position;
        float lightDistance = length(lightVector);
        lightDirection = lightVector / max(lightDistance, 1e-4f);
        attenuation = saturate((light.Out - lightDistance) / max(light.Out - light.In, 1e-4f));

        if (light.Type == LT_SPOT)
            attenuation *= saturate((dot(-lightDirection, light.Direction) - light.OutRange) / max(light.InRange - light.OutRange, 1e-4f));
    }
    else
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    float3 halfVector = normalize(lightDirection + viewVector);
    return light.Color * light.Intensity * attenuation * pow(saturate(dot(normal, halfVector)), WATER_SHININESS);
}

float3 GetMaskedSpecular(uint bits, uint firstLight, float3 position, float3 normal, float3 viewVector)
{
    float3 specular = float3(0.0f, 0.0f, 0.0f);

    [loop]
    while (bits != 0)
    {
        uint light = firstLight + firstbitlow(bits);
        bits &= bits - 1;
        specular += GetLightSpecular(TEN_LOAD_RAW(GpuLight, Lights, light), position, normal, viewVector);
    }

    return specular;
}

// Room lights start the frame light buffer.
float3 GetSpecular(float3 position, float3 normal, float3 viewVector)
{
    uint count = min(WaterLightCount, LIGHTS_PER_DRAW_MAX);
    if (count == 0 || WaterSpecular <= 0.0f)
        return float3(0.0f, 0.0f, 0.0f);

    float3 specular = float3(0.0f, 0.0f, 0.0f);
    uint2 mask;
    if (count <= LIGHT_CLUSTER_LIGHTS_MAX && GetClusterLightMask(position, mask))
    {
        specular += GetMaskedSpecular(mask.x, 0, position, normal, viewVector);
        specular += GetMaskedSpecular(mask.y, 32, position, normal, viewVector);
    }
    else
    {
        for (uint i = 0; i < count; i++)
            specular += GetLightSpecular(TEN_LOAD_RAW(GpuLight, Lights, i), position, normal, viewVector);
    }

    return specular * WaterSpecular;
}

// Refracted scene: the distorted texel unless the distortion picks geometry in front of the water.
float3 GetRefraction(float2 uv, float2 offset, float surfaceDepth, out float waterDepth)
{
    float2 refractedUV = uv + offset;
    waterDepth = GetSceneViewDepth(refractedUV) - surfaceDepth;
    if (waterDepth <= 0.0f)
    {
        refractedUV = uv;
        waterDepth = GetSceneViewDepth(uv) - surfaceDepth;
    }

    return SceneColor.SampleLevel(SceneColorSampler, refractedUV, 0.0f).rgb;
}

// Shading inputs of a surface texel, shared by the reflection trace and the surface shading.
struct WaterTexel
{
    float3 ViewVector;
    float  CameraDistance;
    bool   IsCameraBelow;
    float  Shore;
    float3 Normal;
    float3 ReflectionNormal;
};

// The camera side is taken at the rest height, so the swells do not flip it near the surface.
WaterTexel GetWaterTexel(PixelInput input, float2 uv)
{
    WaterTexel texel;
    float3 toCamera = PixelCameraPosition - input.WorldPosition;
    texel.CameraDistance = length(toCamera);
    texel.ViewVector = toCamera / max(texel.CameraDistance, 1e-4f);
    texel.IsCameraBelow = (PixelCameraPosition.y > input.RestHeight);

    // Waves calm down where the water meets the geometry below it.
    texel.Shore = saturate((GetSceneViewDepth(uv) - input.ViewDepth) / WATER_SHORE_DEPTH);
    texel.Normal = normalize(lerp(WATER_UP, GetWaveNormal(input.WorldPosition, input.SwellWeight), texel.Shore));

    // Reflections use flatter waves, which keep the reflected image readable.
    float3 up = texel.IsCameraBelow ? -WATER_UP : WATER_UP;
    texel.ReflectionNormal = normalize(lerp(up, texel.IsCameraBelow ? -texel.Normal : texel.Normal, 0.5f));
    return texel;
}

// Half resolution pass: reflected color premultiplied by its confidence, which the surface shading upsamples. Below
// the surface, the reflected light crosses the water to the hit and is absorbed like the direct view.
float4 PSReflection(PixelInput input) : SV_TARGET
{
    float2 uv = GetClipUV(mul(float4(input.WorldPosition, 1.0f), WaterViewProjection));
    WaterTexel texel = GetWaterTexel(input, uv);

    // Texels that barely reflect, such as water seen from above, are not traced.
    if (GetFresnel(texel.ReflectionNormal, texel.ViewVector, texel.IsCameraBelow) * WaterReflectionStrength < WATER_SSR_FRESNEL_MIN)
        return float4(0.0f, 0.0f, 0.0f, 0.0f);

    float hitDistance;
    float4 traced = TraceScreenReflection(input.WorldPosition, reflect(-texel.ViewVector, texel.ReflectionNormal), GetJitter(input.Position.xy), hitDistance);
    if (texel.IsCameraBelow)
        traced.rgb = ApplyWaterAbsorption(traced.rgb, hitDistance, WATER_FOG_DENSITY);

    return float4(traced.rgb * traced.a, traced.a);
}

// Steps: refraction with absorption, reflection (traced, completed by the environment), Fresnel blend, specular, fog.
float4 PS(PixelInput input) : SV_TARGET
{
    float2 uv = input.Position.xy * InverseViewportSize;
    WaterTexel texel = GetWaterTexel(input, uv);

    float waterDepth;
    float2 offset = texel.Normal.xz * WaterRefractionStrength * texel.Shore / (1.0f + input.ViewDepth / WATER_REFRACTION_FADE_DEPTH);
    float3 color = GetRefraction(uv, offset, input.ViewDepth, waterDepth);

    // Light crossing the water from above is absorbed along the view ray. Below the surface, the underwater fog
    // absorbs the light between the camera and the surface.
    if (!texel.IsCameraBelow)
        color = ApplyWaterAbsorption(color, waterDepth * texel.CameraDistance / max(input.ViewDepth, 1.0f), 1.0f);

    float3 direction = reflect(-texel.ViewVector, texel.ReflectionNormal);
    float4 traced = ReflectionTrace.SampleLevel(ReflectionTraceSampler, uv, 0.0f);
    float3 reflection = traced.rgb;
    if (traced.a < 1.0f)
        reflection += GetEnvironmentReflection(direction, texel.IsCameraBelow) * (1.0f - traced.a);

    float fresnel = GetFresnel(texel.ReflectionNormal, texel.ViewVector, texel.IsCameraBelow) * WaterReflectionStrength * saturate(texel.Shore * 4.0f);
    color = lerp(color, reflection, saturate(fresnel));

    if (!texel.IsCameraBelow)
        color += GetSpecular(input.WorldPosition, texel.Normal, texel.ViewVector) * texel.Shore;

    return ApplyDistanceFog(float4(color, 1.0f), FogColor, input.Fog, BLENDMODE_OPAQUE);
}

// Water between the camera and the scene behind each pixel. The surfaces are not in the depth buffer: the rays that
// rise through the surface above the camera leave the water at its height.
float4 PSUnderwaterFog(FullscreenPixelInput input) : SV_TARGET
{
    float depth = SceneDepth.SampleLevel(SceneDepthSampler, input.UV, 0.0f).r;
    float4 world = mul(float4(input.UV.x * 2.0f - 1.0f, 1.0f - input.UV.y * 2.0f, depth, 1.0f), UnderwaterInverseViewProjection);
    float3 toScene = world.xyz / world.w - PixelCameraPosition;

    // Rays rising above the surface are shortened to the part below it (similar triangles on the height).
    float pathLength = length(toScene);
    float cameraDepth = max(PixelCameraPosition.y - UnderwaterSurfaceHeight, 0.0f);
    if (-toScene.y > cameraDepth)
        pathLength *= cameraDepth / -toScene.y;

    float3 color = SceneColor.SampleLevel(SceneColorSampler, input.UV, 0.0f).rgb;
    return float4(ApplyWaterAbsorption(color, pathLength, WATER_FOG_DENSITY), 1.0f);
}
