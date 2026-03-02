#ifndef CBITEMSHADER
#define CBITEMSHADER

#include "ShaderLight.glsl"

layout(std140, binding = 1) uniform CBItem
{
    mat4 World;
    //--
    mat4 Bones[MAX_BONES];
    //--
    vec4 Color;
    //--
    vec4 AmbientLight;
    //--
    ivec4 BoneLightModes[MAX_BONES / 4];
    //--
    ShaderLight ItemLights[MAX_LIGHTS_PER_ITEM];
    //--
    int NumItemLights;
    int Skinned;
    int CBItem_Padding0;
    int CBItem_Padding1;
};

#endif // CBITEMSHADER
