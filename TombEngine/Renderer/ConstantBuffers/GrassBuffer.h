#pragma once
#include <SimpleMath.h>

namespace TEN::Renderer::ConstantBuffers
{
	using namespace DirectX::SimpleMath;

	constexpr int GRASS_INSTANCE_BUCKET_SIZE = 256;
	constexpr int MAX_GRASS_INFLUENCE_SPHERES = 8;

	struct alignas(16) GrassInfluenceSphere
	{
		Vector3 Position;
		float Radius;
		//--
		float Intensity;
		float Timestamp;       // Last refresh time (used for decay).
		float BirthTimestamp;  // Creation time (used for rise).
		float Padding1;
	};

	struct alignas(16) GrassInstance
	{
		Vector3 Position;
		float Scale;
		//--
		Vector3 Normal;
		float Seed;
		//--
		Vector4 Color;
		//--
		Vector2 UVOffset;
		Vector2 UVScale;
		//--
		Vector3 AmbientLight;
		float WindEnabled; // 1.0 = blade in outdoor (skybox) room; 0.0 = indoors, no wind.
		//--
		Vector3 SunDirection;
		float SunIntensity;
		//--
		Vector3 SunColor;
		float SunPad0;
	};

	struct alignas(16) CGrassSettingsBuffer
	{
		float MaxDrawDistance;
		float FadeStartDistance;
		float WindStrength;
		float WindFrequency;
		//--
		Vector3 WindDirection;
		float Time;
		//--
		float BendRiseSpeed;
		float BendDecaySpeed;
		float BendMaxAngle;
		int NumInfluences;
		//--
		float BladeWidth;
		float BladeHeight;
		int DebugMode;
		float Padding0;
		//--
		GrassInfluenceSphere Influences[MAX_GRASS_INFLUENCE_SPHERES];
	};

	struct alignas(16) CGrassInstanceBuffer
	{
		GrassInstance Instances[GRASS_INSTANCE_BUCKET_SIZE];
	};
}
