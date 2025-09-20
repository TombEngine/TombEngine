#pragma once
#include <SimpleMath.h>
#include "Renderer/ConstantBuffers/ShaderLight.h"
#include "Renderer/Renderer.h"

namespace TEN::Renderer::ConstantBuffers
{
	using namespace DirectX::SimpleMath;

	constexpr int INSTANCED_STATIC_MESH_BUCKET_SIZE = 100;
	
	struct alignas(16) InstancedStaticMesh
	{
		Matrix World;
		//--
		Vector4 Color;
		//--
		Vector4 Ambient;
		//--
		ShaderLight Lights[MAX_LIGHTS_PER_ITEM];
		//--
		int NumLights;
		int LightMode;
		int Padding1;
		int Padding2;
	};

	struct alignas(16) CInstancedStaticMeshBuffer
	{
		InstancedStaticMesh StaticMeshes[INSTANCED_STATIC_MESH_BUCKET_SIZE];
	};
}