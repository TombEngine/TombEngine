#ifndef CBBLENDINGSHADER
#define CBBLENDINGSHADER

#ifndef BLENDING_CB_MERGED
layout(std140, binding = 3) uniform CBBlending
{
    uint BlendMode;
    int AlphaTest;
    float AlphaThreshold;
    int CBBlending_Padding0;
};
#endif

#endif
