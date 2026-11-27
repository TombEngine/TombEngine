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
		// Slot index equals the item number; the slot persists across frames, so its Prev* state survives between collections.
		int ItemNumber = NO_VALUE;
		int ObjectID   = NO_VALUE;

		Vector3 Position	= Vector3::Zero;
		int		RoomNumber	= NO_VALUE;
		Matrix	World		= Matrix::Identity;
		// Bone transforms of the current game tick, copied from the pose cache by FetchItemPose.
		Matrix	AnimationTransforms[BONE_COUNT_MAX] = {};

		Vector4 Color        = NEUTRAL_COLOR;
		Vector4 AmbientLight = NEUTRAL_COLOR;

		// Mesh index per bone and index of the skin mesh of the item, taken from its model.
		int				 SkinIndex	 = NO_VALUE;	
		std::vector<int> MeshIndex	 = {};

		std::vector<RendererLight*> LightsToDraw = {};
		float						LightFade	 = 0.0f;

		// IsCollected is cleared by ResetItems each frame and set when the item pose is fetched; it tells whether the slot is valid this frame.
		// DisableInterpolation is true until the slot is first collected, so a new item does not interpolate from stale data.
		bool IsCollected		  = false;
		bool DisableInterpolation = true;

		// Values blended between the Prev* and current ones by the interpolation factor; these are what the scene renderer draws.
		Vector3 InterpolatedPosition                       = Vector3::Zero;
		Matrix  InterpolatedWorld                          = Matrix::Identity;
		Matrix  InterpolatedAnimationTransforms[BONE_COUNT_MAX] = {};

		// Previous game tick state. PrevRoomNumber and LightFade drive the ambient light blend after a room change.
		Vector3 PrevPosition                       = Vector3::Zero;
		int     PrevRoomNumber                     = NO_VALUE;
		Matrix  PrevWorld                          = Matrix::Identity;
		Matrix  PrevAnimationTransforms[BONE_COUNT_MAX] = {};
	};
}
