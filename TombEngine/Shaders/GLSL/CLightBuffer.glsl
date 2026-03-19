// Merged constant buffer: Material + ShadowLight + Blending at binding 3.
// Suppresses individual CB declarations.
#ifndef CLIGHTBUFFER_GLSL
#define CLIGHTBUFFER_GLSL

#define MATERIAL_CB_MERGED 1
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

layout(binding = 3, std140) uniform CLightBuffer
{
    // Material (80 bytes)
    vec4 MaterialParameters0;
    vec4 MaterialParameters1;
    vec4 MaterialParameters2;
    vec4 MaterialParameters3;
    uint MaterialTypeAndFlags;
    int CBMaterial_Padding0;
    int CBMaterial_Padding1;
    int CBMaterial_Padding2;

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
