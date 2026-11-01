#pragma once

#include "Math/Math.h"
#include "Renderer/RendererEnums.h"

enum GAME_OBJECT_ID : short;
struct ItemInfo;
struct MoveableAnimBlendData;

// Moveable skeletons posed from the game animation data: bone positions and orientations for gameplay queries and for
// the renderer, independent of what was drawn.

namespace TEN::Animation
{
	struct FrameData;
	struct RootMotionData;

	// Bone of a moveable hierarchy from the level bone data. Parents always precede their children.
	struct SkeletonBone
	{
		int			  Parent		= NO_VALUE; // NO_VALUE for the root and for bones the hierarchy does not reach.
		Vector3		  Offset		= Vector3::Zero;
		unsigned char RotationFlags = 0; // ROT_X, ROT_Y and ROT_Z axes of creature joint rotations.
	};

	// Bone transforms relative to the moveable world matrix, and the animated orientation of each bone.
	struct SkeletonPose
	{
		std::array<Matrix, BONE_COUNT_MAX>	   Transforms	= {};
		std::array<Quaternion, BONE_COUNT_MAX> Orientations = {};
	};

	using BoneRotations = std::array<Quaternion, BONE_COUNT_MAX>;

	void InitializeSkeletons();
	const std::vector<SkeletonBone>& GetSkeleton(GAME_OBJECT_ID objectID);

	void PoseBones(GAME_OBJECT_ID objectID, const FrameData& frame, int mask, const BoneRotations& extraRotations, SkeletonPose& pose,
				   bool useObjectWorldRotation = false, const MoveableAnimBlendData* blend = nullptr, const RootMotionData* rootMotion = nullptr,
				   const ItemInfo* mutatedItem = nullptr);

	// Current pose of an item. Wheels and propellers spin from their previous extra rotations, which the renderer keeps
	// per item; queries without them get the rotation of the current frame only.
	void GetItemPose(const ItemInfo& item, SkeletonPose& pose, BoneRotations* extraRotations = nullptr);

	Matrix GetItemWorldMatrix(const ItemInfo& item, Matrix* rotationMatrix = nullptr, Matrix* translationMatrix = nullptr);
}
