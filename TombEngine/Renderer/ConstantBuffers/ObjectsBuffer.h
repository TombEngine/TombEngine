#pragma once

#include <SimpleMath.h>
#include "Renderer/ConstantBuffers/ObjectData.h"

namespace TEN::Renderer::ConstantBuffers
{
	using namespace DirectX::SimpleMath;

	constexpr int INSTANCED_STATIC_MESH_BUCKET_SIZE = 100;

	// Single object constant buffer. Items draw with instance_count=1 and populate Objects[0]
	// + the bone array at the top; instanced statics fill Objects[0..N-1] and leave
	// Skinned=0 (Static) so the bone-blend path is collapsed by the uniform branch in the
	// shader. Layout matches the HLSL CBObjects cbuffer one-to-one.
	struct alignas(16) CObjectsBuffer
	{
		Matrix Bones[BONE_COUNT_MAX];
		//--
		int    BoneLightModes[BONE_COUNT_MAX];
		//--
		int    Skinned;            // SkinningMode value (0=Static, 1=None, 2=Full, 3=Classic)
		int    ObjectsBuffer_Padding0;
		int    ObjectsBuffer_Padding1;
		int    ObjectsBuffer_Padding2;
		//--
		ObjectData Objects[INSTANCED_STATIC_MESH_BUCKET_SIZE];
	};

	// Byte size of everything preceding the Objects array. Objects is the last member, so a
	// draw that only uses N instances needs to upload just this prefix plus N entries instead
	// of the full 64 KB buffer. See Renderer::UpdateObjectsBuffer.
	constexpr int OBJECTS_BUFFER_HEADER_SIZE =
		(int)(sizeof(CObjectsBuffer) - (sizeof(ObjectData) * INSTANCED_STATIC_MESH_BUCKET_SIZE));

	static_assert(OBJECTS_BUFFER_HEADER_SIZE % 16 == 0, "Objects array must stay 16-byte aligned.");
}
