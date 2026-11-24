// CBDustStorm.hlsli - Uniform block of the volumetric dust storm, declared by the including shader through TEN_DUST_STORM_BLOCK
// (pixel uniform slot 2). Must match CDustStormBuffer in C++ exactly.

#ifndef CB_DUST_STORM_HLSLI
#define CB_DUST_STORM_HLSLI

// Dust storm parameters, filled once per frame by the dust storm update (RendererDustStorm.cpp) and pushed
// as pixel uniforms. Heights and distances are in world units (Y-down: a larger Y is lower). Matrices
// are used with row vectors (mul(v, M)).
TEN_DUST_STORM_BLOCK
{
    // Row 0
    float3 DustColor;
    float  DustDensity;
    //--
    // Row 1
        // World Y where the dust layer starts (lower bound) and ends (upper bound), see DustFogMap.
    float DustMinHeight;
    float DustMaxHeight;
    float DustTime;
    float DustTurbulenceScale;
    //--
    // Row 2
        // Normalized XZ wind direction, wind speed in world units per second and raymarch step count [3, 12].
    float2 DustWindDirection;
    float  DustWindSpeed;
    float  DustStepCount;
    //--
    // Row 3
    float2 DustViewSize;
    float2 DustInvViewSize;
    //--
    // Row 4
        // Camera position and far plane distance, which caps the raymarch length.
    float3 DustCameraPos;
    float  DustFarPlane;
    //--
    // Row 5
        // Sun direction and length of the first raymarch step. Later steps grow geometrically by DustStepGrowth.
    float3 DustLightDirection;
    float  DustBaseStepDist;
    //--
    // Row 6
        // Sun color and ambient term used to light the dust.
    float3 DustLightColor;
    float  DustAmbientStrength;
    //--
    // Row 7
        // Engine distance fog, used to tint the dust at long range so it blends with the fog.
    float3 DustFogColor;
    float  DustFogStartDistance;
    //--
    // Row 8
        // DustIntensityFade is a global [0, 1] fade of the effect.
    float DustFogEndDistance;
    float DustStepGrowth;
    float DustIntensityFade;
    int   DustCameraIsOutdoor; // 1 = camera in outdoor room; 0 = indoor.
    //--
    // Row 9
    float2 DustWindScreenDir; // Kept for padding; world-space bleed march uses DustViewProjection.
    float2 DustBleedPad;
    //--
    // Rows 10-13
        // Used to reconstruct world positions and view rays from screen UV and depth.
    matrix DustInvViewProjection;
    //--
    // Rows 14-17
    matrix DustViewProjection; // Forward VP for world -> UV reprojection.
    //--
    // Row 18
        // Portal bleed: dust blown from outdoor rooms into indoor ones through portals, modelled as up to 4
        // oriented boxes. Ratio of the depth reached at the box top relative to the bottom, and the start
        // (as a fraction of the half extent) of the edge and depth fades.
    float DustBleedTopDepthRatio;
    float DustBleedEdgeFadeStart;
    float DustBleedDepthFadeStart;
    int   DustNumBleedVolumes;
    //--
    // Rows 19-22
        // Per volume: portal center (xyz) and wind inflow strength [0, 1] (w). The three inverse basis rows
        // map a world offset from the center into the box local space: x across the portal and y downwards
        // in [-1, 1], z along the wind direction in [0, 1].
    float4 DustBleedVolumeCenterAndStrength[4];
    // Rows 23-26
    float4 DustBleedVolumeInvBasis0[4];
    // Rows 27-30
    float4 DustBleedVolumeInvBasis1[4];
    // Rows 31-34
    float4 DustBleedVolumeInvBasis2[4];
};

#endif // CB_DUST_STORM_HLSLI
