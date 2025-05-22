#ifndef VERTEXINPUT
#define VERTEXINPUT

struct VertexShaderInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD0;

    float4 Color : COLOR;
    float4 ColorB1 : COLB0;
    float4 ColorB2 : COLB1;
    float4 ColorB3 : COLB2;
    float4 ColorB4 : COLB3;
    float4 ColorB5 : COLB4;
    float4 ColorB6 : COLB5;

    float3 Tangent : TANGENT;
    float3 Binormal : BINORMAL;

    uint4 BoneIndex : BONEINDICES;
    uint4 BoneWeight : BONEWEIGHTS;

    uint AnimationFrameOffset : ANIMATIONFRAMEOFFSET;
    float4 Effects : EFFECTS;
    uint PolyIndex : POLYINDEX;
    uint DrawIndex : DRAWINDEX;
    uint Hash : HASH;
};

#endif // VERTEXINPUT
