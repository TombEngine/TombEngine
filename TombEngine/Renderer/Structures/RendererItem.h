#pragma once

#include "Game/room.h"
#include "Renderer/RendererEnums.h"
#include "Renderer/Structures/RendererLight.h"

namespace TEN::Renderer::Structures
{
	// Renderer state of an item: world and bone transforms of the current and previous game frame and their
	// interpolated values, mesh swaps and lights.
	struct RendererItem
	{
		int ItemNumber = NO_VALUE;
		int ObjectID   = NO_VALUE;

		Vector3 Position	= Vector3::Zero;
		int		RoomNumber	= NO_VALUE;
		Matrix	World		= Matrix::Identity;
		Matrix	AnimationTransforms[BONE_COUNT_MAX] = {};

		Quaternion BoneOrientations[BONE_COUNT_MAX] = {};

		Vector4 Color        = NEUTRAL_COLOR;
		Vector4 AmbientLight = NEUTRAL_COLOR;

		int				 SkinIndex	 = NO_VALUE;	
		std::vector<int> MeshIndex	 = {};

		std::vector<RendererLight*> LightsToDraw = {};
		float						LightFade	 = 0.0f;

		bool DoneAnimations		  = false;
		bool DisableInterpolation = true;

		Vector3 InterpolatedPosition                       = Vector3::Zero;
		Matrix  InterpolatedWorld                          = Matrix::Identity;
		Matrix  InterpolatedAnimationTransforms[BONE_COUNT_MAX] = {};

		Vector3 PrevPosition                       = Vector3::Zero;
		int     PrevRoomNumber                     = NO_VALUE;
		Matrix  PrevWorld                          = Matrix::Identity;
		Matrix  PrevAnimationTransforms[BONE_COUNT_MAX] = {};
	};
}
