#ifndef CBSKYSHADER
#define CBSKYSHADER

#include "Math.glsl"
#include "ShaderLight.glsl"

#ifndef SKY_CB_MERGED
layout(std140, binding = 1) uniform CBSky
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

#endif
