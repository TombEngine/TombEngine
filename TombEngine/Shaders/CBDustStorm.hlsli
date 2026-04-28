// CBDustStorm.hlsli - Constant buffer for the volumetric dust storm shader.
// Bound to register b10 (reuses the Hud slot - dust runs between gun flashes
// and HUD, so there is no overlap).
// Must match CDustStormBuffer in C++ exactly.

#ifndef CB_DUST_STORM_HLSLI
#define CB_DUST_STORM_HLSLI

#define DUST_STORM_MAX_OUTDOOR_ROOMS 32

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
    float DustGustMode;      // 1 = gust mode (aperiodic density modulation), 0 = continuous.
    //--
    // Rows 9-12
    matrix DustInvViewProjection;
    //--
    float4 DustOutdoorRoomMins[DUST_STORM_MAX_OUTDOOR_ROOMS];
    float4 DustOutdoorRoomMaxs[DUST_STORM_MAX_OUTDOOR_ROOMS];
    float  DustOutdoorRoomCount;
    float3 DustPad1;
};

#endif // CB_DUST_STORM_HLSLI
