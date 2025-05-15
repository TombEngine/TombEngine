#ifndef VERTEXINPUT
#define VERTEXINPUT

struct VertexShaderInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD0;

    float4 Color : COLOR;
    float4 ColorB1 : TEXCOORD1;
    float4 ColorB2 : TEXCOORD2;
    float4 ColorB3 : TEXCOORD3;

    float3 Tangent : TANGENT;
    float3 Binormal : BINORMAL;

    uint4 BoneIndex : BONEINDICES;
    float4 BoneWeight : BONEWEIGHTS;

    uint AnimationFrameOffset : ANIMATIONFRAMEOFFSET;
    float4 Effects : EFFECTS;
    uint PolyIndex : POLYINDEX;
    uint DrawIndex : DRAWINDEX;
    uint Hash : HASH; // usato come uint per compatibilità driver
};

#endif // VERTEXINPUT
