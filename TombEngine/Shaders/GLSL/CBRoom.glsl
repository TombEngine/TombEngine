#ifndef CBROOMSHADER
#define CBROOMSHADER

#include "ShaderLight.glsl"

layout(std140, binding = 5) uniform CBRoom
{
    int Water;
    int Caustics;
    int NumRoomLights;
    int NumRoomDecals;
    //--
    vec2 CausticsStartUV;
    vec2 CausticsSize;
    //--
    vec3 AmbientColor;
    float RoomPadding0;
    //--
    ShaderLight RoomLights[MAX_LIGHTS_PER_ROOM];
    //--
    ShaderDecal RoomDecals[MAX_DECALS_PER_ROOM];
};

#endif // CBROOMSHADER
