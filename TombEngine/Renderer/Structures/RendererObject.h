#pragma once
#include <vector>
#include "Math/SimpleMath.h"
#include "Renderer/Structures/RendererBone.h"
#include "Renderer/Structures/RendererMesh.h"
#include "Renderer/RendererEnums.h"

namespace TEN::Renderer::Structures
{
	using namespace TEN::Math::Library;

	// Moveable (Type 0) or static (Type 1) object: meshes, skeleton, animation and bind pose transforms.
	struct RendererObject
	{
		int Id;
		int Type;
		std::vector<RendererMesh*> ObjectMeshes;
		RendererBone* Skeleton;
		std::vector<Matrix> AnimationTransforms;
		std::vector<Matrix> BindPoseTransforms;
		std::vector<RendererBone*> LinearizedBones;
		bool Hidden;
		ShadowMode ShadowType;

		~RendererObject()
		{
			for (int i = 0; i < LinearizedBones.size(); i++)
				delete LinearizedBones[i];
		}
	};
}
