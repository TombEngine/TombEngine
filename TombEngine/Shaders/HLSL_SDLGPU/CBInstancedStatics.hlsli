#ifndef CBINSTANCEDSTATICSSHADER
#define CBINSTANCEDSTATICSSHADER

#include "./ShaderLight.hlsli"

// Reduced from 100 to stay within Vulkan's minimum maxUniformBufferRange (16KB).
// 624 bytes/instance * 25 = 15600 bytes.
#define INSTANCED_STATIC_MESH_BUCKET_SIZE 25

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