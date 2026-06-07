#ifndef OBJECT_HLSLI
#define OBJECT_HLSLI

#include "./ShaderLight.hlsli"

// Per-instance object payload. Layout matches the C++ ObjectData struct one-to-one — keep
// them in lockstep when adding fields.
struct Object
{
	float4x4    World;
	float4      Color;
	float4      AmbientLight;
	ShaderLight Lights[MAX_LIGHTS_PER_ITEM];
	uint4       LightInfo; // x=NumLights (with mask bits), y=LightMode, z=ApplyFogBulbs, w=_padding
};

#endif // OBJECT_HLSLI
