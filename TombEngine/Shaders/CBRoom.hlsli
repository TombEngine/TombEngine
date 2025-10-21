#ifndef CBROOMSHADER
#define CBROOMSHADER

#include "./ShaderLight.hlsli"

cbuffer RoomBuffer : register(b5)
{
    int Water;
    int Caustics;
    int NumRoomLights;
    int NumRoomDecals;
    float2 CausticsStartUV;
    float2 CausticsSize;
    float4 AmbientColor;
    ShaderLight RoomLights[MAX_LIGHTS_PER_ROOM];
    ShaderDecal RoomDecals[MAX_DECALS_PER_ROOM];
};

#endif // CBROOMSHADER