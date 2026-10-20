#pragma once
#include "Math/SimpleMath.h"
#include "Renderer/RendererEnums.h"

namespace TEN::Renderer::Structures
{
	using namespace TEN::Math::Library;

	struct RendererLight
	{
		Vector3 Position;
		LightType Type;
		Vector3 Color;
		float Intensity;
		Vector3 Direction;
		float In;
		float Out;
		float InRange;
		float OutRange;
		
		BoundingSphere BoundingSphere;
		int RoomNumber;
		bool AffectNeighbourRooms;
		bool CastShadows;
		float Luma;

		Vector3 PrevPosition;
		Vector3 PrevDirection;

		int Hash = 0;
	};

	// Light candidate during light collection; dynamic lights rank first, then by local intensity.
	struct RendererLightNode
	{
		RendererLight* Light;
		float LocalIntensity;
		float Distance;
		int Dynamic;
	};
}
