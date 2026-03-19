#ifndef CBINSTANCEDSTATICSSHADER
#define CBINSTANCEDSTATICSSHADER

#include "ShaderLight.glsl"

#define INSTANCED_STATIC_MESH_BUCKET_SIZE 100

struct InstancedStatic
{
    mat4 World;
    vec4 Color;
    vec4 AmbientLight;
    ShaderLight InstancedStaticLights[MAX_LIGHTS_PER_ITEM];
    uvec4 LightInfo;
};

layout(std140, binding = 1) uniform CBInstancedStatics
{
    InstancedStatic StaticMeshes[INSTANCED_STATIC_MESH_BUCKET_SIZE];
};

#endif // CBINSTANCEDSTATICSSHADER
