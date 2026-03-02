#ifndef CBCAMERASHADER
#define CBCAMERASHADER

#include "Math.glsl"

layout(std140, binding = 0) uniform CBCamera
{
    mat4 ViewProjection;
    //--
    mat4 View;
    //--
    mat4 Projection;
    //--
    mat4 InverseView;
    //--
    mat4 InverseProjection;
    //--
    mat4 DualParaboloidView;
    //--
    vec4 CamPositionWS;
    //--
    vec4 CamDirectionWS;
    //--
    vec2 ViewSize;
    vec2 InvViewSize;
    //--
    uint Frame;
    uint RoomNumber;
    uint CameraUnderwater;
    int Hemisphere;
    //--
    int AmbientOcclusion;
    int AmbientOcclusionExponent;
    float AspectRatio;
    float TanHalfFOV;
    //--
    vec4 FogColor;
    //--
    float FogMinDistance;
    float FogMaxDistance;
    float NearPlane;
    float FarPlane;
    //--
    int RefreshRate;
    int NumFogBulbs;
    float InterpolatedFrame;
    float CCameraMatrixBuffer_Padding0;
    //--
    ShaderFogBulb FogBulbs[MAX_FOG_BULBS];
};

#endif
