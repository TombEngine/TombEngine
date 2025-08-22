#pragma once
#include <SimpleMath.h>
#include "Game/effects/Decal.h"
#include "Renderer/ConstantBuffers/ShaderDecal.h"
#include "Renderer/RendererEnums.h"

namespace TEN::Renderer::ConstantBuffers
{
	using namespace DirectX::SimpleMath;
	using namespace TEN::Effects::Decal;

	struct alignas(16) CRoomBuffer
	{
		int Water;
		int Caustics;
		int NumRoomLights;
		int NumRoomDecals;
		//--
		Vector2 CausticsStartUV;
		Vector2 CausticsSize;
		//--
		Vector4 AmbientColor;
		//--
		ShaderLight RoomLights[MAX_LIGHTS_PER_ROOM];
		//--
		ShaderDecal RoomDecals[Decal::COUNT_MAX];
	};
}