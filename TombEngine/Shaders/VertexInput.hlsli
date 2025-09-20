#ifndef VERTEXINPUT
#define VERTEXINPUT

struct VertexShaderInput 
{
	float3 Position: POSITION0;
	float4 Normal: NORMAL0;
	float2 UV: TEXCOORD0;
	float4 Color: COLOR0;
	float4 Tangent: TANGENT0;
	uint4 BoneIndex: BONEINDICES;
	uint4 BoneWeight: BONEWEIGHTS;
	uint AnimationFrameOffset: ANIMATIONFRAMEOFFSET;
	uint Effects: EFFECTS;
	uint Index: DRAWINDEX;
	int Hash : HASH;
};

#endif // VERTEXINPUT