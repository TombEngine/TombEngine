#pragma once
#include <vector>

namespace TEN::Renderer::Structures
{
	using namespace TEN::Math::Library;

	struct RendererBone
	{
		int Index = 0;

		Vector3	   GlobalTranslation = Vector3::Zero;
		Vector3	   Translation		 = Vector3::Zero;
		Matrix	   GlobalTransform	 = Matrix::Identity;
		Matrix	   Transform		 = Matrix::Identity;
		Quaternion ExtraRotation	 = Quaternion::Identity;

		RendererBone*			   Parent	= nullptr;
		std::vector<RendererBone*> Children = {};

		byte ExtraRotationFlags = 0;

		RendererBone(int index)
		{
			Index = index;
			Translation = Vector3::Zero;
			ExtraRotation = Quaternion::Identity;
			ExtraRotationFlags = 0;
		}
	};
}
