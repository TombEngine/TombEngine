#ifndef VERTEXINPUT
#define VERTEXINPUT

#define MAX_BONE_WEIGHTS 4

struct VertexShaderInput 
{
	float3 Position: POSITION0;
	float3 Normal: NORMAL0;
	float2 UV: TEXCOORD0;
	float4 Color: COLOR0;
	float3 Tangent: TANGENT0;
	float3 Binormal: BINORMAL0;
	unsigned int AnimationFrameOffset: ANIMATIONFRAMEOFFSET;
	float4 Effects: EFFECTS;
	unsigned int BoneIndex[MAX_BONE_WEIGHTS]: BONEINDICES;
	float BoneWeight[MAX_BONE_WEIGHTS]: BONEWEIGHTS;
	unsigned int PolyIndex : POLYINDEX;
	unsigned int Index: DRAWINDEX;
	int Hash : HASH;
};

#endif // VERTEXINPUT