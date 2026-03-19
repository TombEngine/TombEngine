#ifndef CBBLENDINGSHADER
#define CBBLENDINGSHADER

#ifndef BLENDING_CB_MERGED
#ifndef REG_CB_BLENDING
#define REG_CB_BLENDING b3
#endif
cbuffer CBBlending : register(REG_CB_BLENDING)
{
    uint BlendMode;
    int AlphaTest;
    float AlphaThreshold;
    int CBBlending_Padding0;
};
#endif

#endif