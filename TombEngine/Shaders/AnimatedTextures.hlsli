#ifndef ANIMATEDTEXTURESSHADER
#define ANIMATEDTEXTURESSHADER

#include "./CBAnimatedTexture.hlsli"

#define ANIM_TYPE_NORMAL   0
#define ANIM_TYPE_UVROTATE 1
#define ANIM_TYPE_VIDEO    2

float2 CalculateUVRotate(float2 uv, unsigned int frame)
{
    if (UVRotateSpeed <= 0.0f)
        return uv;

    const float epsilon = 0.001f;

    AnimatedFrameUV f = AnimFrames[frame];

    float2 minUV = min(min(f.TopLeft, f.TopRight), min(f.BottomLeft, f.BottomRight));
    float2 maxUV = max(max(f.TopLeft, f.TopRight), max(f.BottomLeft, f.BottomRight));
    float2 uvSize = maxUV - minUV;

    float2 localUV = (uv - minUV) / uvSize;
	
    float relPos = InterpolatedFrame * UVRotateSpeed / 30.0f;
	
    float theta = radians(UVRotateDirection + 90.0f);
    float2 dir = float2(cos(theta), sin(theta));
	
    float2 scrolledUV = uv + (-dir * relPos * uvSize);
	
    scrolledUV = frac(scrolledUV);
    scrolledUV = clamp(scrolledUV, epsilon, 1.0f - epsilon);

    return scrolledUV;
}

float2 CalculateUVRotateForLegacyWaterfalls(float2 uv, unsigned int frame)
{
    if (FPS == 0)
    {
        return uv;
    }
    else
    {
        float step = uv.y - AnimFrames[frame].TopLeft.y;
        float vert = AnimFrames[frame].TopLeft.y + (step / 2);
		
        float height = (AnimFrames[frame].BottomLeft.y - AnimFrames[frame].TopLeft.y) / 2;
        float relPos = 1.0f - (InterpolatedFrame % FPS) / (float) FPS;
        float newUV = vert + height * relPos;
        
        return float2(uv.x, newUV);
    }
}

float2 GetFrame(unsigned int index, unsigned int offset)
{
    float speed = (float)FPS / 30.0f;
    int frame = int(Frame * speed + offset) % NumAnimFrames;

	float2 result = 0;

	switch (index)
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

float2 GetUVPossiblyAnimated(float2 input, int index, int frame)
{
    float2 output = input;

    if (Animated && Type != ANIM_TYPE_UVROTATE)
        output = GetFrame(index, frame);
		
    return output;
}

float2 ConvertAnimUV(float2 input)
{
    float2 output = input;

    if (Animated && Type == ANIM_TYPE_UVROTATE)
    {
        if (IsWaterfall)
            output = CalculateUVRotateForLegacyWaterfalls(input, 0);
        else
            output = CalculateUVRotate(input, 0);
    }

    return output;
}

float3 ConvertAnimNormal(float3 input)
{
    float3 output = input;
	
    if (Animated && Type == ANIM_TYPE_VIDEO)
        output = float3(0.5f, 0.5f, 1.0f);
	
    return output;
}

float4 ConvertAnimOSRH(float4 input)
{
    float4 output = input;

    if (Animated && Type == ANIM_TYPE_VIDEO)
        output = float4(1.0f, 0.0f, 0.0f, 1.0f);
	
    return output;
}

#endif