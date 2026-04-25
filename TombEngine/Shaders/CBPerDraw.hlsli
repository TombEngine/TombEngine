#ifndef CBPERDRAW_HLSLI
#define CBPERDRAW_HLSLI

// Combined per-draw cbuffer: Material parameters + Blending state + Animated texture
// metadata. The three lived in separate cbuffers (b2 CBMaterial, b12 CBBlending, b6
// CBAnimatedTexture) and were updated back-to-back at every material/blend/animation
// transition, paying three Map/Unmap pairs per change. Merging them into one CB cuts
// that to one. The 256-entry animated frame UVs are now a structured buffer (t14).
// Layout matches the C++ CPerDrawBuffer struct one-to-one.
cbuffer CBPerDraw : register(b2)
{
    float4       MaterialParameters0;
    float4       MaterialParameters1;
    float4       MaterialParameters2;
    float4       MaterialParameters3;
    unsigned int MaterialTypeAndFlags;
    unsigned int BlendMode;
    int          AlphaTest;
    float        AlphaThreshold;
    unsigned int NumAnimFrames;
    unsigned int FPS;
    unsigned int Type;
    unsigned int Animated;
    float        UVRotateDirection;
    float        UVRotateSpeed;
    int          IsWaterfall;
    int          CBPerDraw_Padding0;
};

#endif // CBPERDRAW_HLSLI
