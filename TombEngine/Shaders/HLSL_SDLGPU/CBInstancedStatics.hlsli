#ifndef CBINSTANCEDSTATICSSHADER
#define CBINSTANCEDSTATICSSHADER

#include "./ShaderLight.hlsli"

#define INSTANCED_STATIC_MESH_BUCKET_SIZE 100

struct InstancedStatic
{
    float4x4 World;
    float4 Color;
    float4 AmbientLight;
    ShaderLight InstancedStaticLights[MAX_LIGHTS_PER_ITEM];
    uint4 LightInfo;
};

#ifndef REG_CB_INSTANCED_STATICS
#define REG_CB_INSTANCED_STATICS b3
#endif
cbuffer CBInstancedStatics : register(REG_CB_INSTANCED_STATICS)
{
    InstancedStatic StaticMeshes[INSTANCED_STATIC_MESH_BUCKET_SIZE];
};

#endif // CBINSTANCEDSTATICSSHADER