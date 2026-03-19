#ifndef CBMATERIALSHADER
#define CBMATERIALSHADER

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
    unsigned int MaterialTypeAndFlags;
    int CBMaterial_Padding0;
    int CBMaterial_Padding1;
    int CBMaterial_Padding2;
};

#endif