#ifndef CBBLENDINGSHADER
#define CBBLENDINGSHADER

layout(std140, binding = 12) uniform CBBlending
{
    uint BlendMode;
    int AlphaTest;
    float AlphaThreshold;
    int CBBlending_Padding0;
};

#endif
