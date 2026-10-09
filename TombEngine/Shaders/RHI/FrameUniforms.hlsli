#ifndef TEN_FRAME_UNIFORMS
#define TEN_FRAME_UNIFORMS

// Uniform blocks shared by the scene shaders. Slot 0 is pushed once per frame, slot 1 per draw.
// Layouts are mirrored by the C++ structs in Renderer/Next/NextRenderer.cpp.

#include "./ShaderInterop.hlsli"

TEN_VS_UNIFORMS(FrameVertexUniforms, 0)
{
    float4x4 ViewProjection;
    float3   CameraPosition;
    float    FogMinDistance;
    float    FogMaxDistance;
    uint     Frame;
    float    InterpolatedFrame;
    uint     CameraUnderwater;
};

TEN_VS_UNIFORMS(DrawVertexUniforms, 1)
{
    int    AnimatedSetIndex;
    uint   IsWaterRoom; // Rooms only: water rooms seen from outside the water are refracted.
    float2 DrawPadding;
};

TEN_PS_UNIFORMS(FramePixelUniforms, 0)
{
    float4   FogColor;
    float3   PixelCameraPosition;
    float    AmbientOcclusionExponent; // Zero when ambient occlusion is disabled.
    float3   CameraDirection;
    float    FramePixelPadding0;
    float4x4 View;
    float2   InverseViewportSize;
    float2   FramePixelPadding1;
};

TEN_PS_UNIFORMS(DrawPixelUniforms, 1)
{
    float AlphaThreshold;
    uint  BlendMode;
    uint  LightOffset; // Rooms only: slice of the frame light buffer.
    uint  LightCount;  // Light count in the low 16 bits, light type mask in the high bits.
    uint  MaterialIndex;
    uint  DrawPixelPadding0;
    uint  DrawPixelPadding1;
    uint  DrawPixelPadding2;
};

#endif // TEN_FRAME_UNIFORMS
