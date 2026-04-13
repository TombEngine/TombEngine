#ifndef CBGRASS_HLSLI
#define CBGRASS_HLSLI

#define MAX_GRASS_INSTANCES 256
#define MAX_GRASS_INFLUENCES 8

struct GrassInfluenceSphere
{
	float3 Position;
	float Radius;
	//--
	float Intensity;
	float Timestamp;       // Last refresh time (used for decay).
	float BirthTimestamp;  // Creation time (used for rise).
	float Padding1;
};

struct GrassInstanceData
{
	float3 Position;
	float Scale;
	//--
	float3 Normal;
	float Seed;
	//--
	float4 Color;
	//--
	float2 UVOffset;
	float2 UVScale;
	//--
	float3 AmbientLight;
	float WindEnabled;
	//--
	float3 SunDirection;
	float SunIntensity;
	//--
	float3 SunColor;
	float SunPad0;
};

cbuffer GrassSettingsBuffer : register(b9)
{
	float MaxDrawDistance;
	float FadeStartDistance;
	float WindStrength;
	float WindFrequency;
	//--
	float3 WindDirection;
	float Time;
	//--
	float BendRiseSpeed;
	float BendDecaySpeed;
	float BendMaxAngle;
	int NumInfluences;
	//--
	float BladeWidth;
	float BladeHeight;
	int DebugMode;
	float GrassSettingsPad0;
	//--
	GrassInfluenceSphere Influences[MAX_GRASS_INFLUENCES];
};

cbuffer GrassInstanceBuffer : register(b13)
{
	GrassInstanceData GrassInstances[MAX_GRASS_INSTANCES];
};

#endif
