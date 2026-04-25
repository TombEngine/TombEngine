#ifndef CBPERDRAW_HLSLI
#define CBPERDRAW_HLSLI

// Combined per-draw cbuffer: Material parameters + Blending state. Both used to live in
// separate cbuffers (b2 for CBMaterial, b12 for CBBlending) and were updated back-to-back
// at every material/blend transition, paying two Map/Unmap pairs per change. Merging them
// halves that cost. Layout matches the C++ CPerDrawBuffer struct one-to-one.
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
};

#endif // CBPERDRAW_HLSLI
