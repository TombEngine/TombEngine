#ifndef CBBLENDINGSHADER
#define CBBLENDINGSHADER

cbuffer CBBlending : register(b12)
{
    uint BlendMode;
    int AlphaTest;
    float AlphaThreshold;
    int CBBlending_Padding0;
};

#endif