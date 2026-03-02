#ifndef CBSKYSHADER
#define CBSKYSHADER

#include "Math.glsl"
#include "ShaderLight.glsl"

layout(std140, binding = 8) uniform CBSky
{
    mat4 SkyWorld;
    //--
    vec4 SkyColor;
    //--
    vec4 SkyAmbientLight;
    //--
    int ApplyFogBulbs;
    vec3 CSkyBuffer_Padding0;
};

#endif
