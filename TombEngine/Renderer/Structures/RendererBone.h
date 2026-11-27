#pragma once
#include <vector>
#include "Math/SimpleMath.h"

namespace TEN::Renderer::Structures
{
	using namespace TEN::Math::Library;

	// Bone of the skeleton of an object, built once on level load. Translation is the offset from the parent in the
	// bind pose; the Global fields accumulate it down the hierarchy (see Renderer::BuildHierarchy).
	struct RendererBone
	{
		int Index = 0;

		Vector3	   GlobalTranslation = Vector3::Zero;
		Vector3	   Translation		 = Vector3::Zero;
		Matrix	   GlobalTransform	 = Matrix::Identity;
		Matrix	   Transform		 = Matrix::Identity;
		// ExtraRotation is applied on top of the animated rotation of the bone by UpdateAnimation.
		Quaternion ExtraRotation	 = Quaternion::Identity;

		RendererBone*			   Parent	= nullptr;
		std::vector<RendererBone*> Children = {};

		// Extra rotation flags read from the level bone data (opcode bits 2-4).
		unsigned char ExtraRotationFlags = 0;

		RendererBone(int index)
		{
			Index = index;
			Translation = Vector3::Zero;
			ExtraRotation = Quaternion::Identity;
			ExtraRotationFlags = 0;
		}
	};
}
