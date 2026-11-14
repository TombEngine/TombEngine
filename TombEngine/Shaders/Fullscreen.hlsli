#ifndef TEN_FULLSCREEN
#define TEN_FULLSCREEN

// Fullscreen triangle without vertex buffers, for post-process and screen space passes.

struct FullscreenPixelInput
{
    float4 Position : SV_POSITION;
    float2 UV       : TEXCOORD0;
};

// Vertices 0, 1 and 2 get UVs (0, 0), (2, 0) and (0, 2): one triangle covering the whole screen, with UV
// (0, 0) at the top left.
FullscreenPixelInput FullscreenVS(uint vertexId)
{
    float2 uv = float2((vertexId << 1) & 2, vertexId & 2);

    FullscreenPixelInput output;
    output.Position = float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    output.UV = uv;
    return output;
}

#endif // TEN_FULLSCREEN
