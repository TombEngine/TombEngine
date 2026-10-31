// CBDustStorm.hlsli - Constant buffer for the volumetric dust storm shader.
// Bound to register b10 (reuses the Hud slot - dust runs between gun flashes
// and HUD, so there is no overlap).
// Must match CDustStormBuffer in C++ exactly.

#ifndef CB_DUST_STORM_HLSLI
#define CB_DUST_STORM_HLSLI

cbuffer CBDustStorm : register(b10)
{
    // Row 0
    float3 DustColor;
    float  DustDensity;
    //--
    // Row 1
    float DustMinHeight;
    float DustMaxHeight;
    float DustTime;
    float DustTurbulenceScale;
    //--
    // Row 2
    float2 DustWindDirection;
    float  DustWindSpeed;
    float  DustStepCount;
    //--
    // Row 3
    float2 DustViewSize;
    float2 DustInvViewSize;
    //--
    // Row 4
    float3 DustCameraPos;
    float  DustFarPlane;
    //--
    // Row 5
    float3 DustLightDirection;
    float  DustBaseStepDist;
    //--
    // Row 6
    float3 DustLightColor;
    float  DustAmbientStrength;
    //--
    // Row 7
    float3 DustFogColor;
    float  DustFogStartDistance;
    //--
    // Row 8
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
    matrix DustInvViewProjection;
    //--
    // Rows 14-17
    matrix DustViewProjection; // Forward VP for world -> UV reprojection.
    //--
    // Row 18
    float DustBleedTopDepthRatio;
    float DustBleedEdgeFadeStart;
    float DustBleedDepthFadeStart;
    int   DustNumBleedVolumes;
    //--
    // Rows 19-22
    float4 DustBleedVolumeCenterAndStrength[4];
    // Rows 23-26
    float4 DustBleedVolumeInvBasis0[4];
    // Rows 27-30
    float4 DustBleedVolumeInvBasis1[4];
    // Rows 31-34
    float4 DustBleedVolumeInvBasis2[4];
};

#endif // CB_DUST_STORM_HLSLI
