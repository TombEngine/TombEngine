#pragma once
#include <SimpleMath.h>
#include "Renderer/ConstantBuffers/ShaderLight.h"
#include "Renderer/RendererEnums.h"

namespace TEN::Renderer::ConstantBuffers
{
	using namespace DirectX::SimpleMath;

	struct alignas(16) CRoomBuffer
	{
		int Water;
		int Caustics;
		int NumRoomLights;
		int Padding;
		//--
		Vector2 CausticsStartUV;
		Vector2 CausticsSize;
		//--
		Vector4 AmbientColor;
		//--
		ShaderLight RoomLights[MAX_LIGHTS_PER_ROOM];
	};
}