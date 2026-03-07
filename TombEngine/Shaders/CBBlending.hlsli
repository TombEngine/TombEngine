#ifndef CBBLENDINGSHADER
#define CBBLENDINGSHADER

cbuffer CBBlending : register(b12)
{
    uint BlendMode;
    int AlphaTest;
    float AlphaThreshold;
};

#endif