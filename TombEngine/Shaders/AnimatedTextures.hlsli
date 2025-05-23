struct AnimatedFrameUV
{
	float2 TopLeft;
	float2 TopRight;
	float2 BottomRight;
	float2 BottomLeft;
};

cbuffer AnimatedBuffer : register(b6)
{
	AnimatedFrameUV AnimFrames[256];
	unsigned int NumAnimFrames;
	unsigned int FPS;
	unsigned int Type;
	unsigned int padding2;
}

float2 CalculateUVRotate(float2 uv, unsigned int frame)
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
		float relPos = 1.0f - (Frame % FPS) / (float)FPS;

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
