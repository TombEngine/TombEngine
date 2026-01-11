#pragma once

#include "Game/room.h"
#include "Renderer/RendererEnums.h"
#include "Renderer/Structures/RendererLight.h"

namespace TEN::Renderer::Structures
{
	struct RendererItem
	{
		int ItemNumber = NO_VALUE;
		int ObjectID   = NO_VALUE;

		Vector3 Position	= Vector3::Zero;
		int		RoomNumber	= NO_VALUE;
		Matrix	World		= Matrix::Identity;
		Matrix	Translation = Matrix::Identity;
		Matrix	Rotation	= Matrix::Identity;
		Matrix	Scale		= Matrix::Identity;
		Matrix	AnimTransforms[MAX_BONES] = {};

		Quaternion BoneOrientations[MAX_BONES];

		Vector4 Color = Vector4::One;
		Vector4 AmbientLight = Vector4::One;

		int				 SkinIndex	 = NO_VALUE;	
		std::vector<int> MeshIndex	 = {};

		std::vector<RendererLight*> LightsToDraw = {};
		float LightFade = 0.0f;

		bool DoneAnimations = false;
		bool DisableInterpolation = true;

		Vector3 InterpolatedPosition	= Vector3::Zero;
		Matrix	InterpolatedWorld		= Matrix::Identity;
		Matrix	InterpolatedTranslation = Matrix::Identity;
		Matrix	InterpolatedRotation	= Matrix::Identity;
		Matrix	InterpolatedScale		= Matrix::Identity;
		Matrix	InterpolatedAnimTransforms[MAX_BONES];

		Vector3 PrevPosition	= Vector3::Zero;
		int		PrevRoomNumber	= NO_VALUE;
		Matrix	PrevWorld		= Matrix::Identity;
		Matrix	PrevTranslation = Matrix::Identity;
		Matrix	PrevRotation	= Matrix::Identity;
		Matrix	PrevScale		= Matrix::Identity;
		Matrix	PrevAnimTransforms[MAX_BONES];
	};
}
