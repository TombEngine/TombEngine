#ifndef CBOBJECTS_HLSLI
#define CBOBJECTS_HLSLI

#include "./Object.hlsli"

#define INSTANCED_STATIC_MESH_BUCKET_SIZE 100

// Single object constant buffer used by every Objects.hlsl draw. Items draw with
// instance_count=1 and populate Objects[0] + Bones/Skinned at the top. Instanced statics
// fill Objects[0..N-1] and leave Skinned=0; the bone-blend / per-bone-mode branches in the
// shader fall through uniformly because the flag is constant for the entire draw call.
//
// Total budget at MAX_BONES=32, MAX_LIGHTS_PER_ITEM=8, BUCKET_SIZE=100:
//   2048 (Bones) + 128 (BoneLightModes) + 16 (Skinned+pad) + 100*624 (Objects) = 64592 B,
//   inside the 65536 B cbuffer ceiling.
cbuffer CBObjects : register(b3)
{
    float4x4 Bones[MAX_BONES];
    int4     BoneLightModes[MAX_BONES / 4]; // packed per-bone LightMode values
    int      Skinned;                       // 0=not skinned (statics), 1=skinned, 2=classic blend
    int      CBObjects_Padding0;
    int      CBObjects_Padding1;
    int      CBObjects_Padding2;
    Object   Objects[INSTANCED_STATIC_MESH_BUCKET_SIZE];
};

#endif // CBOBJECTS_HLSLI
