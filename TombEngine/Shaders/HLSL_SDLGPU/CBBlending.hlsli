#ifndef CBBLENDINGSHADER
#define CBBLENDINGSHADER

#ifndef REG_CB_BLENDING
#define REG_CB_BLENDING b12
#endif
cbuffer CBBlending : register(REG_CB_BLENDING)
{
    uint BlendMode;
    int AlphaTest;
    float AlphaThreshold;
    int CBBlending_Padding0;
};

#endif