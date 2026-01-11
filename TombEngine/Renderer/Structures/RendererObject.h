#pragma once
#include <vector>
#include <SimpleMath.h>
#include "Renderer/Structures/RendererBone.h"
#include "Renderer/Structures/RendererMesh.h"
#include "Renderer/RendererEnums.h"

namespace TEN::Renderer::Structures
{
	using namespace DirectX::SimpleMath;

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
