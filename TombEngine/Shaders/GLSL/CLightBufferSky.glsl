// Merged constant buffer: Sky + ShadowLight + Blending at binding 3.
// Used by RoomAmbient shader which needs Sky instead of Material.
// Suppresses individual CB declarations.
#ifndef CLIGHTBUFFERSKY_GLSL
#define CLIGHTBUFFERSKY_GLSL

#define SKY_CB_MERGED 1
#define SHADOWLIGHT_CB_MERGED 1
#define BLENDING_CB_MERGED 1

#ifndef SPHERE_STRUCT_DEFINED
#define SPHERE_STRUCT_DEFINED
struct Sphere
{
    vec3 position;
    float radius;
};
#endif

layout(binding = 3, std140) uniform CLightBufferSky
{
    // Sky (112 bytes)
    mat4 SkyWorld;
    vec4 SkyColor;
    vec4 SkyAmbientLight;
    int ApplyFogBulbs;
    vec3 CSkyBuffer_Padding0;

    // ShadowLight (720 bytes)
    ShaderLight Light;
    mat4 LightViewProjections[6];
    int CastShadows;
    int NumSpheres;
    int ShadowMapSize;
    int ShadowLight_Padding;
    Sphere Spheres[16];

    // Blending (16 bytes)
    uint BlendMode;
    int AlphaTest;
    float AlphaThreshold;
    int CBBlending_Padding0;
};

#endif
