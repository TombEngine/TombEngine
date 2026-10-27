#pragma once

#include <cstddef>
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

	// Byte count of the CB prefix covering bones, metadata and the first `objectCount` entries
	// of Objects[]. Draws which populate only a few instances (e.g. sorted faces always use
	// Objects[0] with instance count 1) pass this as partial upload size to avoid re-uploading
	// the whole ~64KB buffer on every batch. Relies on Objects[] being the last field.
	constexpr int GetObjectsBufferPrefixSize(int objectCount)
	{
		return (int)(offsetof(CObjectsBuffer, Objects) + sizeof(ObjectData) * objectCount);
	}
}
