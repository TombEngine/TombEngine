#ifndef CBANIMATEDTEXTURESHADER
#define CBANIMATEDTEXTURESHADER

struct AnimatedFrameUV
{
    vec2 TopLeft;
    vec2 TopRight;
    //--
    vec2 BottomRight;
    vec2 BottomLeft;
};

layout(std140, binding = 2) uniform CBAnimatedTexture
{
    AnimatedFrameUV AnimFrames[256];
    //--
    uint NumAnimFrames;
    uint FPS;
    uint Type;
    uint Animated;
    //--
    float UVRotateDirection;
    float UVRotateSpeed;
    int IsWaterfall;
    int CBAnimatedTexture_Padding0;
};

#endif
