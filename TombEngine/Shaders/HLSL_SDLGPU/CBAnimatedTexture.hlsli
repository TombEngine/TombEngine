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

#ifndef REG_CB_ANIMATED_TEXTURE
#define REG_CB_ANIMATED_TEXTURE b6
#endif
cbuffer CBAnimatedTexture : register(REG_CB_ANIMATED_TEXTURE)
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
    int CBAnimatedTexture_Padding0;

}

#endif