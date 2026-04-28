// ============================================================================
// DustStorm.hlsl - Screen-space volumetric dust / sand storm.
//
// Adapted from the GLSL "Believable animated volumetric dust storm in 7
// samples" Shadertoy (only the fog(), fogmap() and triNoise3d() functions are
// ported - the scene SDF is intentionally dropped). The pass runs after all
// opaque + transparent geometry; depth from the linear depth render target is
// used both to clamp marching to the closest surface and to blend the dust
// with the engine's distance fog.
//
// Algorithm:
//   1. For every pixel reconstruct the world-space ray from the camera through
//      the pixel's far-plane projection.
//   2. Sample linear depth and convert to a march distance "mt".
//   3. Step from a near distance outward in geometric increments (d *= growth),
//      sampling 3D triangle noise modulated by a height window (MinHeight ..
//      MaxHeight), accumulating the dust color via the same lerp() pattern as
//      the reference shader.
//   4. Output the accumulated dust as RGBA with premultiplied alpha so the C++
//      side can simply blend with SrcAlpha / InvSrcAlpha.
//
// Performance:
//   - StepCount is driven from the CB (5..8 by default, matching the reference).
//   - Distance-LOD: noise scale shrinks with distance, allowing low step counts
//     without visible seams.
//   - No shadow march (the reference's shadow() depended on a scene SDF that
//     does not exist in this engine - a cheap height-based attenuation is used
//     in its place).
// ============================================================================

#include "./CBDustStorm.hlsli"

// Depth target.
Texture2D    DepthTexture : register(t0);
SamplerState LinearSamp   : register(s3); // LinearClamp
SamplerState PointSamp    : register(s1); // PointWrap

// ---------------------------------------------------------------------------
// Fullscreen-triangle vertex shader (same layout as God Ray / PostProcess).
// ---------------------------------------------------------------------------

struct VSInput
{
    float3 Position : POSITION0;
    float2 UV       : TEXCOORD0;
    float4 Color    : COLOR0;
};

struct VSOutput
{
    float4 Position     : SV_POSITION;
    float2 UV           : TEXCOORD0;
    float4 PositionCopy : TEXCOORD1;
};

VSOutput VSDustStorm(VSInput input)
{
    VSOutput output;
    output.Position     = float4(input.Position, 1.0f);
    output.UV           = input.UV;
    output.PositionCopy = output.Position;
    return output;
}

// ---------------------------------------------------------------------------
// Triangle noise (port of triNoise3d / tri / tri3 from the reference).
// ---------------------------------------------------------------------------

float DustTri(float x)
{
    return abs(frac(x) - 0.5f);
}

float3 DustTri3(float3 p)
{
    return float3(
        DustTri(p.z + DustTri(p.y)),
        DustTri(p.z + DustTri(p.x)),
        DustTri(p.y + DustTri(p.x)));
}

float TriNoise3D(float3 p)
{
    float  z  = 1.4f;
    float  rz = 0.0f;
    float3 bp = p;

    [unroll]
    for (int i = 0; i < 3; i++)
    {
        float3 dg = DustTri3(bp);
        p += dg;

        bp *= 2.0f;
        z  *= 1.5f;
        p  *= 1.2f;

        rz += DustTri(p.z + DustTri(p.x + DustTri(p.y))) / z;
        bp += 0.14f;
    }

    return rz;
}

// ---------------------------------------------------------------------------
// fogmap() - density at a sampled world position.
//
// Engine-space units are large (1024 = sector). The reference shader works in
// arbitrary units, so the input position is rescaled into a perceptually
// pleasant noise frequency. WindDirection biases noise advection, WindSpeed
// drives the temporal scroll.
// ---------------------------------------------------------------------------

float DustFogMap(float3 worldPos, float distFromCam)
{
    // Convert engine units (Y-down) into noise space.
    // 1.0/2048 brings a 2-block window into a unit-cube of noise.
    const float NOISE_SCALE = 1.0f / 2048.0f;
    float3 p = worldPos * NOISE_SCALE;

    // Wind advection: scroll noise lookup opposite to wind direction so the
    // density pattern drifts WITH the wind (adding moves the pattern against
    // the wind, making dust appear to travel in the wrong direction).
    float2 advection = DustWindDirection * (DustTime * DustWindSpeed * NOISE_SCALE);
    p.x -= advection.x;
    p.z -= advection.y;

    // Distance-LOD: shrink the effective noise scale with distance to hide
    // sample seams when StepCount is low. (1 / (d/1024 + 8) ratio borrowed
    // from the reference fogmap.)
    float distScaled = distFromCam * (1.0f / 1024.0f);
    float distFreq   = 2.2f / (distScaled + 8.0f);

    float n = TriNoise3D(p * (DustTurbulenceScale * 8.0f) * distFreq);

    // Height window: 0 outside [MinHeight, MaxHeight], peaks in the middle.
    // MinHeight is the world-Y at which dust starts being visible (Y-down,
    // bigger Y = lower in TombEngine), MaxHeight is the upper bound.
    // Heights are passed in world units already.
    float heightMid    = (DustMinHeight + DustMaxHeight) * 0.5f;
    // abs() required: in Y-down space DustMinHeight > DustMaxHeight, so the
    // raw difference is negative and would clamp to near-zero without abs().
    float heightSpread = max(abs(DustMaxHeight - DustMinHeight), 1.0f) * 0.5f;
    float heightFrac   = saturate(1.0f - abs(worldPos.y - heightMid) / heightSpread);

    // Soft top fade so the dust hugs the ground.
    float groundHug = smoothstep(0.0f, 1.0f, heightFrac);

    return n * groundHug;
}

// ---------------------------------------------------------------------------
// fog() - accumulate dust along the ray (port of the reference fog()).
// Returns RGBA: rgb is premultiplied dust color, a is opacity over the scene.
// ---------------------------------------------------------------------------

float4 DustFog(float3 ro, float3 rd, float mt)
{
    float3 col   = float3(0.0f, 0.0f, 0.0f);
    float  alpha = 0.0f;
    float  d     = max(DustBaseStepDist, 1.0f);
    float  growth = max(DustStepGrowth, 1.05f);
    int    steps = (int)max(3.0f, min(DustStepCount, 12.0f));

    [loop]
    for (int i = 0; i < 12; i++)
    {
        if (i >= steps || d > mt)
            break;

        float3 pos = ro + rd * d;

		if (!IsPointInsideDustRoom(pos))
		{
			d *= growth;
			continue;
		}

        // Density at this sample.
        float rz = DustFogMap(pos, d);

        // Distance-blend: matches the reference smoothstep(d, d*1.8, mt).
        // This is what hides the seams between coarse samples - distant samples
        // contribute less when they are close to the depth-clamped end of the
        // ray.
        float fade = smoothstep(d, d * growth, mt);

        // Cheap pseudo-shadow: darker toward the ground, brighter near the cap
        // height (acts like the ground occluding self-illumination).
        // abs() on both terms handles Y-down space (DustMinHeight > DustMaxHeight).
        float heightT  = saturate(abs(pos.y - DustMinHeight) / max(abs(DustMaxHeight - DustMinHeight), 1.0f));
        float shadow   = lerp(0.4f, 1.0f, 1.0f - heightT);

        // Light response: simple wrap diffuse against the sun direction.
        float ndotl    = saturate(dot(-DustLightDirection, float3(0.0f, -1.0f, 0.0f)) * 0.5f + 0.5f);
        float3 lit     = DustColor * (DustLightColor * ndotl + DustAmbientStrength);

        float contrib = saturate(rz * fade) * shadow;

        // Emulate the reference's iterative mix() chain via standard front-to-back
        // alpha compositing. Each step adds (1-alpha)*contrib.
        float a = saturate(contrib * DustDensity * 0.45f);
        col   += lit * a * (1.0f - alpha);
        alpha += a   * (1.0f - alpha);

        // Fully opaque -> early out.
        if (alpha >= 0.99f)
            break;

        d *= growth;
    }

    // Convert from premultiplied (front-to-back accumulation) to non-premultiplied
    // so the engine's standard SrcAlpha / InvSrcAlpha blending equation produces
    // the correct composite. Avoid divide-by-zero on transparent pixels.
    float invA = (alpha > 0.0001f) ? (1.0f / alpha) : 0.0f;
    return float4(col * invA, alpha);
}

// ---------------------------------------------------------------------------
// Reconstruct a world-space ray from the pixel UV.
// ---------------------------------------------------------------------------

float3 ReconstructWorldDir(float2 uv)
{
    // NDC at the far plane.
    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 farClip   = float4(ndc, 1.0f, 1.0f);
    float4 farWorld  = mul(farClip, DustInvViewProjection);
    farWorld.xyz    /= farWorld.w;
    return normalize(farWorld.xyz - DustCameraPos);
}

bool IsPointInsideDustRoom(float3 pos)
{
    int roomCount = (int)DustOutdoorRoomCount;

    [loop]
    for (int i = 0; i < DUST_STORM_MAX_OUTDOOR_ROOMS; i++)
    {
        if (i >= roomCount)
            break;

        float3 boxMin = DustOutdoorRoomMins[i].xyz;
        float3 boxMax = DustOutdoorRoomMaxs[i].xyz;

        if (all(pos >= boxMin) && all(pos <= boxMax))
            return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// PSDustStorm - main pass. Runs over the full main render target with
// SrcAlpha / InvSrcAlpha blending; rgb is premultiplied so we output (rgb, a).
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// GustMultiplier - aperiodic [0,1] envelope driven by three incommensurable
// sine waves. Most of the time near 0 (calm); occasional peaks are gusts.
// ---------------------------------------------------------------------------
float GustMultiplier(float t)
{
    // Three incommensurable frequencies produce a non-repeating beat pattern.
    float g = sin(t * 0.31f) * sin(t * 0.53f + 1.3f) * sin(t * 0.19f + 2.7f);
    // g in [-1,1]; remap to [0,1] then sharpen so peaks are distinct gusts.
    g = saturate(g * 0.5f + 0.5f);
    return pow(g, 2.5f);
}

float4 PSDustStorm(VSOutput input) : SV_TARGET
{
    if (DustDensity * DustIntensityFade < 0.001f)
        return float4(0.0f, 0.0f, 0.0f, 0.0f);

    if (DustOutdoorRoomCount < 0.5f)
        return float4(0.0f, 0.0f, 0.0f, 0.0f);

    // The GBuffer depth target stores NDC depth (clip.z / clip.w, range 0..1).
    // Reconstruct the world-space surface position via the inverse VP matrix
    // and use its distance from the camera as the ray march limit.
    float ndcDepth = DepthTexture.SampleLevel(PointSamp, input.UV, 0).r;

    float2 ndc2d     = float2(input.UV.x * 2.0f - 1.0f, 1.0f - input.UV.y * 2.0f);
    float4 clipPos   = float4(ndc2d, ndcDepth, 1.0f);
    float4 worldPos4 = mul(clipPos, DustInvViewProjection);
    float3 surfaceWS = worldPos4.xyz / worldPos4.w;
    float marchEnd   = min(length(surfaceWS - DustCameraPos), DustFarPlane);

    float3 ro = DustCameraPos;
    float3 rd = ReconstructWorldDir(input.UV);

    // In gust mode, modulate density by an aperiodic time function so the
    // dust appears in organic bursts rather than as continuous fog.
    float gustScale = 1.0f;
    if (DustGustMode > 0.5f)
        gustScale = GustMultiplier(DustTime);

    float4 dust = DustFog(ro, rd, marchEnd);
    dust *= gustScale;

    // Distance-fog-aware tint: blend the accumulated color toward the engine
    // fog color near the far plane so the dust does not punch a hard edge
    // through pre-existing distance fog.
    if (DustFogEndDistance > DustFogStartDistance)
    {
        float fogT = saturate((marchEnd - DustFogStartDistance) /
                              max(DustFogEndDistance - DustFogStartDistance, 1.0f));
        dust.rgb = lerp(dust.rgb, DustFogColor, fogT * 0.5f);
    }

    dust.a *= saturate(DustIntensityFade);
    return dust;
}
