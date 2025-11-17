#ifndef CBANIMATEDTEXTURESHADER
#define CBANIMATEDTEXTURESHADER

struct AnimatedFrameUV
{
    float2 TopLeft;
    float2 TopRight;
	//--
    float2 BottomRight;
    float2 BottomLeft;
};

cbuffer CBAnimatedTexture : register(b6)
{
    AnimatedFrameUV AnimFrames[256];
	//--
    unsigned int NumAnimFrames;
    unsigned int FPS;
    unsigned int Type;
    unsigned int Animated;
    //--
    float UVRotateDirection;
    float UVRotateSpeed;
    int IsWaterfall;
}

#endif