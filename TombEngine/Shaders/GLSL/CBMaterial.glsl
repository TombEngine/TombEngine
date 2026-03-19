#ifndef CBMATERIALSHADER
#define CBMATERIALSHADER

#ifndef MATERIAL_CB_MERGED
layout(std140, binding = 2) uniform CBMaterial
{
    vec4 MaterialParameters0;
    //--
    vec4 MaterialParameters1;
    //--
    vec4 MaterialParameters2;
    //--
    vec4 MaterialParameters3;
    //--
    uint MaterialTypeAndFlags;
    int CBMaterial_Padding0;
    int CBMaterial_Padding1;
    int CBMaterial_Padding2;
};
#endif

#endif
