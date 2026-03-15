#ifndef CBMATERIALSHADER
#define CBMATERIALSHADER

#ifndef REG_CB_MATERIAL
#define REG_CB_MATERIAL b2
#endif
cbuffer CBMaterial : register(REG_CB_MATERIAL)
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