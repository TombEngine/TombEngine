#ifndef CBMATERIALSHADER
#define CBMATERIALSHADER

#define MATERIAL_OPAQUE                 0
#define MATERIAL_REFLECTIVE             1
#define MATERIAL_SKYBOX_REFLECTIVE      2

cbuffer CBMaterial : register(b2)
{
    float4 MaterialParameters0;
    //--
    float4 MaterialParameters1;
    //--
    float4 MaterialParameters2;
    //--
    float4 MaterialParameters3;
    //--
    unsigned int MaterialType;
};

#endif