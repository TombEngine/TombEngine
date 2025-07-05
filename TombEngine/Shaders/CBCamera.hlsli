#ifndef CBCAMERASHADER
#define CBCAMERASHADER

#include "./Math.hlsli"

cbuffer CBCamera : register(b0)
{
	float4x4 ViewProjection;
	float4x4 View;
    float4x4 Projection;
    float4x4 InverseProjection;
	float4x4 DualParaboloidView;
	float4 CamPositionWS;
	float4 CamDirectionWS;
	//--
	float2 ViewSize;
	float2 InvViewSize;
	//--
	unsigned int Frame;
	unsigned int RoomNumber;
	unsigned int CameraUnderwater;
	int Emisphere;
	//--
	int AmbientOcclusion;
	int AmbientOcclusionExponent;
	float AspectRatio;
	float TanHalfFOV;
	//--
	float4 FogColor;
	//--
	float FogMinDistance;
	float FogMaxDistance;
	float NearPlane;
	float FarPlane;
	//--
    int RefreshRate;
	int NumFogBulbs;
	float2 Padding2;
	//--
	ShaderFogBulb FogBulbs[MAX_FOG_BULBS];
};

#endif