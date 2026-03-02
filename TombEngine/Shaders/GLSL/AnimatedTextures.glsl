#ifndef ANIMATEDTEXTURESSHADER
#define ANIMATEDTEXTURESSHADER

#include "CBAnimatedTexture.glsl"

#define ANIM_TYPE_NORMAL   0
#define ANIM_TYPE_UVROTATE 1
#define ANIM_TYPE_VIDEO    2

vec2 CalculateUVRotate(vec2 uv, uint frame)
{
    if (UVRotateSpeed <= 0.0)
        return uv;

    const float epsilon = 0.001;

    AnimatedFrameUV f = AnimFrames[frame];

    vec2 minUV = min(min(f.TopLeft, f.TopRight), min(f.BottomLeft, f.BottomRight));
    vec2 maxUV = max(max(f.TopLeft, f.TopRight), max(f.BottomLeft, f.BottomRight));
    vec2 uvSize = maxUV - minUV;

    vec2 localUV = (uv - minUV) / uvSize;

    float relPos = InterpolatedFrame * UVRotateSpeed / 30.0;

    float theta = radians(UVRotateDirection + 90.0);
    vec2 dir = vec2(cos(theta), sin(theta));

    vec2 scrolledUV = uv + (-dir * relPos * uvSize);

    scrolledUV = fract(scrolledUV);
    scrolledUV = clamp(scrolledUV, epsilon, 1.0 - epsilon);

    return scrolledUV;
}

vec2 CalculateUVRotateForLegacyWaterfalls(vec2 uv, uint frame)
{
    if (FPS == 0u)
    {
        return uv;
    }
    else
    {
        float stepVal = uv.y - AnimFrames[frame].TopLeft.y;
        float vert = AnimFrames[frame].TopLeft.y + (stepVal / 2.0);

        float height = (AnimFrames[frame].BottomLeft.y - AnimFrames[frame].TopLeft.y) / 2.0;
        float relPos = 1.0 - float(Frame % FPS) / float(FPS);
        float newUV = vert + height * relPos;

        return vec2(uv.x, newUV);
    }
}

vec2 GetFrame(uint index, uint offset)
{
    float speed = float(FPS) / 30.0;
    int frame = int(Frame * uint(speed) + offset) % int(NumAnimFrames);

    vec2 result = vec2(0);

    switch (int(index))
    {
    case 0:
        result = AnimFrames[frame].TopLeft;
        break;

    case 1:
        result = AnimFrames[frame].TopRight;
        break;

    case 2:
        result = AnimFrames[frame].BottomRight;
        break;

    case 3:
        result = AnimFrames[frame].BottomLeft;
        break;
    }

    return result;
}

vec2 GetUVPossiblyAnimated(vec2 inputUV, int index, int frame)
{
    vec2 outputUV = inputUV;

    if (Animated != 0u && Type != uint(ANIM_TYPE_UVROTATE))
        outputUV = GetFrame(uint(index), uint(frame));

    return outputUV;
}

vec2 ConvertAnimUV(vec2 inputUV)
{
    vec2 outputUV = inputUV;

    if (Animated != 0u && Type == uint(ANIM_TYPE_UVROTATE))
    {
        if (IsWaterfall != 0)
            outputUV = CalculateUVRotateForLegacyWaterfalls(inputUV, 0u);
        else
            outputUV = CalculateUVRotate(inputUV, 0u);
    }

    return outputUV;
}

vec3 ConvertAnimNormal(vec3 inputNormal)
{
    vec3 outputNormal = inputNormal;

    if (Animated != 0u && Type == uint(ANIM_TYPE_VIDEO))
        outputNormal = vec3(0.5, 0.5, 1.0);

    return outputNormal;
}

vec4 ConvertAnimOSRH(vec4 inputVal)
{
    vec4 outputVal = inputVal;

    if (Animated != 0u && Type == uint(ANIM_TYPE_VIDEO))
        outputVal = vec4(1.0, 0.0, 0.0, 1.0);

    return outputVal;
}

#endif
