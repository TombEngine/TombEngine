#include "framework.h"
#include "Game/Animation/Animation.h"

#include "Game/camera.h"
#include "Game/collision/collide_room.h"
#include "Game/control/box.h"
#include "Game/control/flipeffect.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Objects/Generic/Object/rope.h"
#include "Renderer/Renderer.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Entities::Generic;
using namespace TEN::Math;
using TEN::Renderer::g_Renderer;

namespace TEN::Animation
{
	// TODO: Arm anim object in savegame.

	FixedMotionData AnimData::GetFixedMotion(int frameNumber) const
	{
		// Ensure non-zero frame count. Animations with zero frames are considered valid.
		unsigned int frameCount = std::max(1, EndFrameNumber);

		// Compute relative translation and curve alpha.
		float alpha = (float)frameNumber / (float)frameCount;
		auto translation = Vector3(FixedMotionCurveX.GetY(alpha), FixedMotionCurveY.GetY(alpha), FixedMotionCurveZ.GetY(alpha));

		// Return fixed motion.
		return FixedMotionData
		{
			translation,
			alpha
		};
	}

	RootMotionData AnimData::GetRootMotion(int frameNumber) const
	{
		// Test for root motion flags.
		bool hasTranslation = Flags & ((int)AnimFlags::RootMotionTranslationX | (int)AnimFlags::RootMotionTranslationY | (int)AnimFlags::RootMotionTranslationZ);
		bool hasRot = Flags & ((int)AnimFlags::RootMotionRotationX | (int)AnimFlags::RootMotionRotationY | (int)AnimFlags::RootMotionRotationZ);
		if (!hasTranslation && !hasRot)
			return {};

		// Handle frame 0.
		if (frameNumber == 0)
		{
			// Derive root motion from first two frames if animation is cycled.
			if (Flags & (int)AnimFlags::RootMotionCycle)
			{
				if (Frames.size() > 1)
				{
					frameNumber = 1;
				}
				else
				{
					TENLog("Attempted to get anim root motion for frame 0 from cycle with 1 frame.", LogLevel::Warning);
					return {};
				}
			}
			else
			{
				return {};
			}
		}

		// Compute relative translation.
		auto translation = Vector3::Zero;
		if (hasTranslation)
		{
			const auto& rootPos = Frames[frameNumber].RootPosition;
			const auto& prevRootPos = Frames[frameNumber - 1].RootPosition;
			auto rootTranslation = rootPos - prevRootPos;

			if (Flags & (int)AnimFlags::RootMotionTranslationX)
				translation.x = rootTranslation.x;
			if (Flags & (int)AnimFlags::RootMotionTranslationY)
				translation.y = rootTranslation.y;
			if (Flags & (int)AnimFlags::RootMotionTranslationZ)
				translation.z = rootTranslation.z;
		}

		// Compute relative rotation.
		auto rot = EulerAngles::Identity;
		if (hasRot)
		{
			const auto& rootOrient = Frames[frameNumber].BoneOrientations.front();
			const auto& prevRootOrient = Frames[frameNumber - 1].BoneOrientations.front();
			auto rootRot = rootOrient - prevRootOrient;

			if (Flags & (int)AnimFlags::RootMotionRotationX)
				rot.x = rootRot.x;
			if (Flags & (int)AnimFlags::RootMotionRotationY)
				rot.y = rootRot.y;
			if (Flags & (int)AnimFlags::RootMotionRotationZ)
				rot.z = rootRot.z;
		}

		// Return root motion.
		return RootMotionData
		{
			translation,
			rot
		};
	}

	RootMotionData AnimData::GetRootMotionCounteraction(int frameNumber) const
	{
		// Test for root motion flags.
		bool hasTranslation = Flags & ((int)AnimFlags::RootMotionTranslationX | (int)AnimFlags::RootMotionTranslationY | (int)AnimFlags::RootMotionTranslationZ);
		bool hasRot = Flags & ((int)AnimFlags::RootMotionRotationX | (int)AnimFlags::RootMotionRotationY | (int)AnimFlags::RootMotionRotationZ);
		if (!hasTranslation && !hasRot)
			return {};

		// Handle frame 0.
		if (frameNumber == 0)
			return {};

		// Get relative translation counteraction.
		auto translation = Vector3::Zero;
		if (hasTranslation)
		{
			const auto& rootPos = Frames[frameNumber].RootPosition;
			const auto& baseRootPos = Frames.front().RootPosition;
			auto rootTranslation = baseRootPos - rootPos;

			if (Flags & (int)AnimFlags::RootMotionTranslationX)
				translation.x = rootTranslation.x;
			if (Flags & (int)AnimFlags::RootMotionTranslationY)
				translation.y = rootTranslation.y;
			if (Flags & (int)AnimFlags::RootMotionTranslationZ)
				translation.z = rootTranslation.z;
		}

		// Get relative rotation counteraction.
		auto rot = EulerAngles::Identity;
		if (hasRot)
		{
			auto rootOrient = EulerAngles(Frames[frameNumber].BoneOrientations.front());
			auto baseRootOrient = EulerAngles(Frames.front().BoneOrientations.front());
			auto rootRot = baseRootOrient - rootOrient;

			if (Flags & (int)AnimFlags::RootMotionRotationX)
				rot.x = rootRot.x;
			if (Flags & (int)AnimFlags::RootMotionRotationY)
				rot.y = rootRot.y;
			if (Flags & (int)AnimFlags::RootMotionRotationZ)
				rot.z = rootRot.z;
		}

		// Return root motion counteraction.
		return RootMotionData
		{
			translation,
			rot
		};
	}

	bool BoneMutator::IsEmpty() const
	{
		return (Offset == Vector3::Zero &&
				Rotation == EulerAngles::Identity &&
				Scale == Vector3::One);
	};

	static void ExecuteAnimCommands(ItemInfo& item, bool isFrameBased)
	{
		const auto& anim = GetAnimData(item);
		for (const auto& command : anim.Commands)
			command->Execute(item, isFrameBased);
	}

	void AnimateItem(ItemInfo& item)
	{
		if (!item.IsLara())
		{
			item.TouchBits.ClearAll();
			item.HitStatus = false;
		}

		ExecuteAnimCommands(item, true);
		item.Animation.FrameNumber++;

		const auto* anim = &GetAnimData(item);

		// Handle dispatch.
		const auto* dispatch = GetStateDispatch(item);
		if (dispatch != nullptr)
		{
			SetStateDispatch(item, *dispatch);
			anim = &GetAnimData(item);

			item.Animation.ActiveState = anim->StateID;

			if (!item.IsLara())
			{
				// Reset RequiredState if already reached.
				if (item.Animation.RequiredState == item.Animation.ActiveState)
					item.Animation.RequiredState = NO_VALUE;
			}
		}

		// Handle link.
		if (item.Animation.FrameNumber > anim->EndFrameNumber)
		{
			ExecuteAnimCommands(item, false);

			item.SetAnimBlend(anim->BlendFrameCount, anim->BlendCurve);
			item.Animation.AnimNumber = anim->NextAnimNumber;
			item.Animation.FrameNumber = anim->NextFrameNumber;

			anim = &GetAnimData(item);

			if (item.Animation.ActiveState != anim->StateID)
			{
				item.Animation.ActiveState =
				item.Animation.TargetState = anim->StateID;
			}

			if (!item.IsLara())
			{
				// Reset RequiredState if already reached.
				if (item.Animation.RequiredState == item.Animation.ActiveState)
					item.Animation.RequiredState = NO_VALUE;
			}
		}
		
		// Update blend.
		if (item.Animation.Blend.IsEnabled())
		{
			item.Animation.Blend.FrameNumber++;
			if (item.Animation.Blend.FrameNumber > item.Animation.Blend.FrameCount)
				item.DisableAnimBlend();
		}

		// Get fixed motion and root motion.
		auto fixedMotion = anim->GetFixedMotion(item.Animation.FrameNumber);
		auto rootMotion = anim->GetRootMotion(item.Animation.FrameNumber);

		// TODO: Better handling.
		// Apply motion translation and gravity.
		if (item.Animation.IsAirborne)
		{
			if (item.IsLara())
			{
				if (TestEnvironment(ENV_FLAG_SWAMP, &item))
				{
					item.Animation.Velocity.z -= item.Animation.Velocity.z / 8;
					if (abs(item.Animation.Velocity.z) < 8.0f)
					{
						item.Animation.IsAirborne = false;
						item.Animation.Velocity.z = 0.0f;
					}

					if (item.Animation.Velocity.y > VERTICAL_VELOCITY_GRAVITY_THRESHOLD)
						item.Animation.Velocity.y /= 2;
					item.Animation.Velocity.y -= item.Animation.Velocity.y / 4;

					if (item.Animation.Velocity.y < 4.0f)
						item.Animation.Velocity.y = 4.0f;
					item.Pose.Position.y += item.Animation.Velocity.y;
				}
				else
				{
					if (item.Animation.ActiveState != LS_FLY_CHEAT)
					{
						item.Animation.Velocity.y += GetEffectiveGravity(item.Animation.Velocity.y);
						item.Pose.Position.y += item.Animation.Velocity.y;
					}
				}
			}
			else
			{
				item.Animation.Velocity.y += GetEffectiveGravity(item.Animation.Velocity.y);
				item.Pose.Position.y += item.Animation.Velocity.y;
			}
		}
		else
		{
			// TODO: Y velocity.
			if (item.IsLara())
			{
				const auto& player = GetLaraInfo(item);

				bool isInSwamp = (player.Control.WaterStatus == WaterStatus::Wade && TestEnvironment(ENV_FLAG_SWAMP, &item));
				item.Animation.Velocity.x = (fixedMotion.Translation.x + rootMotion.Translation.x) * (isInSwamp ? 0.5f : 1.0f);
				item.Animation.Velocity.z = (fixedMotion.Translation.z + rootMotion.Translation.z) * (isInSwamp ? 0.5f : 1.0f);
			}
			else
			{
				item.Animation.Velocity.x = fixedMotion.Translation.x + rootMotion.Translation.x;
				item.Animation.Velocity.z = fixedMotion.Translation.z + rootMotion.Translation.z;
			}
		}

		// Update animation.
		if (item.IsLara())
		{
			const auto& player = GetLaraInfo(item);

			if (player.Control.Rope.Ptr != NO_VALUE)
				DelAlignLaraToRope(&item);

			if (!player.Control.IsMoving)
			{
				item.Pose.Translate(player.Control.MoveAngle, item.Animation.Velocity.z, 0.0f, item.Animation.Velocity.x);
				item.Pose.Orientation += rootMotion.Rotation;
			}

			g_Renderer.UpdateLaraAnimations(true);
		}
		else
		{
			item.Pose.Translate(item.Pose.Orientation.y, item.Animation.Velocity.z, 0.0f, item.Animation.Velocity.x);
			item.Pose.Orientation += rootMotion.Rotation;
			g_Renderer.UpdateItemAnimations(item.Index, true);
		}

		// Debug.
		if (item.IsLara())
		{
			PrintDebugMessage(std::string(std::string("Fixed motion: ") + ((fixedMotion.Translation != Vector3::Zero) ? "Yes" : "No")).c_str());
			PrintDebugMessage(std::string(std::string("Root motion: ") + ((rootMotion.Translation != Vector3::Zero && rootMotion.Rotation != EulerAngles::Identity) ? "Yes" : "No")).c_str());
			PrintDebugMessage(std::string(std::string("Blend enabled: ") + (item.Animation.Blend.IsEnabled() ? "Yes" : "No")).c_str());
			PrintDebugMessage("Frame number: %d", item.Animation.Blend.FrameNumber);
			PrintDebugMessage("Frame count: %d", item.Animation.Blend.FrameCount);
		}
	}

	void AnimateItem(ItemInfo* item)
	{
		AnimateItem(*item);
	}

	bool TestStateDispatch(const ItemInfo& item, int targetStateID)
	{
		const auto* dispatch = GetStateDispatch(item, targetStateID);
		return (dispatch != nullptr);
	}

	bool TestLastFrame(const ItemInfo& item, int animNumber)
	{
		if (animNumber == NO_VALUE)
			animNumber = item.Animation.AnimNumber;

		// Check for animation number mismatch; return early.
		if (item.Animation.AnimNumber != animNumber)
			return false;

		// FAILSAFE: Account for frames past real end frame.
		const auto& anim = GetAnimData(item.Animation.AnimObjectID, animNumber);
		return (item.Animation.FrameNumber >= anim.EndFrameNumber);
	}

	bool TestAnimFrameRange(const ItemInfo& item, int frameNumberLow, int frameNumberHigh)
	{
		return (item.Animation.FrameNumber >= frameNumberLow &&
				item.Animation.FrameNumber <= frameNumberHigh);
	}

	const AnimData& GetAnimData(const ObjectInfo& object, int animNumber)
	{
		if (object.Animations.empty())
		{
			TENLog("Attempted to access invalid animation.", LogLevel::Error);
			return Objects[0].Animations[0];
		}

		return object.Animations[animNumber];
	}

	const AnimData& GetAnimData(GAME_OBJECT_ID objectID, int animNumber)
	{
		const auto& object = Objects[objectID];
		return GetAnimData(object, animNumber);
	}

	const AnimData& GetAnimData(const ItemInfo& item, int animNumber)
	{
		if (animNumber == NO_VALUE)
			animNumber = item.Animation.AnimNumber;

		return GetAnimData(item.Animation.AnimObjectID, animNumber);
	}

	const FrameData& GetFrame(GAME_OBJECT_ID objectID, int animNumber, int frameNumber)
	{
		const auto& anim = GetAnimData(objectID, animNumber);
		return anim.Frames[frameNumber];
	}

	const FrameData& GetFrame(const ItemInfo& item, int animNumber, int frameNumber)
	{
		return GetFrame(item.ObjectNumber, animNumber, frameNumber);
	}

	const FrameData& GetFrame(const ItemInfo& item)
	{
		return GetFrame(item.ObjectNumber, item.Animation.AnimNumber, item.Animation.FrameNumber);
	}

	const FrameData& GetFirstFrame(GAME_OBJECT_ID objectID, int animNumber)
	{
		return GetFrame(objectID, animNumber, 0);
	}

	const FrameData& GetLastFrame(GAME_OBJECT_ID objectID, int animNumber)
	{
		return GetFrame(objectID, animNumber, INT_MAX);
	}

	const StateDispatchData* GetStateDispatch(const ItemInfo& item, int targetStateID)
	{
		targetStateID = (targetStateID == NO_VALUE) ? item.Animation.TargetState : targetStateID;

		// Prevent dispatch to same state ID.
		if (item.Animation.ActiveState == targetStateID)
			return nullptr;

		// Run through state dispatches.
		const auto& anim = GetAnimData(item);
		for (const auto& dispatch : anim.Dispatches)
		{
			// Check for state ID mismatch.
			if (dispatch.StateID != targetStateID)
				continue;

			// Check if current frame is within dispatch range.
			if (TestAnimFrameRange(item, dispatch.FrameNumberLow, dispatch.FrameNumberHigh))
				return &dispatch;
		}

		return nullptr;
	}

	int GetNextAnimState(const ItemInfo& item)
	{
		return GetNextAnimState(item.Animation.AnimObjectID, item.Animation.AnimNumber);
	}

	int GetNextAnimState(GAME_OBJECT_ID objectID, int animNumber)
	{
		const auto& anim = GetAnimData(objectID, animNumber);
		const auto& nextAnim = GetAnimData(objectID, anim.NextAnimNumber);

		return nextAnim.StateID;
	}

	int GetFrameCount(GAME_OBJECT_ID objectID, int animNumber)
	{
		const auto& anim = GetAnimData(objectID, animNumber);
		return anim.EndFrameNumber;
	}

	int GetFrameCount(const ItemInfo& item)
	{
		return GetFrameCount(item.Animation.AnimObjectID, item.Animation.AnimNumber);
	}

	float GetEffectiveGravity(float verticalVel)
	{
		return ((verticalVel >= VERTICAL_VELOCITY_GRAVITY_THRESHOLD) ? 1.0f : g_GameFlow->GetSettings()->Physics.Gravity);
	}

	Vector3i GetJointPosition(const ItemInfo& item, int boneID, const Vector3i& relOffset)
	{
		bool incorrectBone = false;
		if (boneID < 0 || boneID >= Objects[item.ObjectNumber].nmeshes)
		{
			TENLog("Unknown bone ID specified for object " + GetObjectName(item.ObjectNumber) + ".", LogLevel::Warning, LogConfig::All);
			incorrectBone = true;
		}

		// Always return object's root position if it's invisible. Joint position can't be predicted otherwise since it's not animated.
		if (incorrectBone || Objects[item.ObjectNumber].Hidden || item.Status == ITEM_INVISIBLE)
			return Geometry::TranslatePoint(item.Pose.Position, item.Pose.Orientation, relOffset);

		// Use matrices done in renderer to transform relative offset.
		return Vector3i(g_Renderer.GetMoveableBonePosition(item.Index, boneID, relOffset.ToVector3()));
	}

	Vector3i GetJointPosition(ItemInfo* item, int boneID, const Vector3i& relOffset)
	{
		return GetJointPosition(*item, boneID, relOffset);
	}

	Vector3i GetJointPosition(ItemInfo* item, const CreatureBiteInfo& bite)
	{
		return GetJointPosition(item, bite.BoneID, bite.Position);
	}

	Vector3i GetJointPosition(const ItemInfo& item, const CreatureBiteInfo& bite)
	{
		return GetJointPosition(item, bite.BoneID, bite.Position);
	}

	Vector3 GetJointOffset(GAME_OBJECT_ID objectID, int boneID, bool discardZSign)
	{
		const auto& object = Objects[objectID];
		int boneIndex = object.boneIndex + (boneID * 4);

		if (g_Level.Bones.size() <= boneIndex)
			return Vector3::Zero;

		int* bone = &g_Level.Bones[boneIndex];
		auto offset = Vector3(*(bone + 1), *(bone + 2), *(bone + 3));

		if (discardZSign)
			offset.z = abs(offset.z);

		return offset;
	}

	Quaternion GetBoneOrientation(const ItemInfo& item, int boneID)
	{
		return g_Renderer.GetMoveableBoneOrientation(item.Index, boneID);
	}

	void SetAnimation(ItemInfo& item, GAME_OBJECT_ID animObjectID, int animNumber, int frameNumber, int blendFrameCount, const BezierCurve2D& blendCurve)
	{
		// Check if animation is already set.
		if (item.Animation.AnimObjectID == animObjectID &&
			item.Animation.AnimNumber == animNumber &&
			item.Animation.FrameNumber  == frameNumber)
		{
			return;
		}

		const auto& animObject = Objects[animObjectID];

		// Check if animation is missing.
		if (animNumber < 0 || animNumber >= animObject.Animations.size())
		{
			TENLog(fmt::format(
					"Attempted to set missing animation {} {}{} for object {}.",
					animNumber, (animObjectID == item.ObjectNumber) ? "" : fmt::format(" from moveable {}", GetObjectName(animObjectID)), GetObjectName(item.ObjectNumber)),
				LogLevel::Warning);

			return;
		}

		const auto& anim = GetAnimData(animObject, animNumber);

		
		// Check if frame is missing.
		if (frameNumber < 0 || frameNumber > anim.EndFrameNumber)
		{
			TENLog(fmt::format(
					"Attempted to set missing frame {} from animation {}{} for object {}.",
					frameNumber, animNumber, (animObjectID == item.ObjectNumber) ? "" : fmt::format(" from moveable {}", GetObjectName(animObjectID)), GetObjectName(item.ObjectNumber)),
				LogLevel::Warning);

			return;
		}

		item.SetAnimBlend(blendFrameCount, blendCurve);
		item.Animation.AnimObjectID = animObjectID;
		item.Animation.AnimNumber = animNumber;
		item.Animation.FrameNumber = frameNumber;
		item.Animation.ActiveState =
		item.Animation.TargetState = anim.StateID;
	}

	void SetAnimation(ItemInfo& item, int animNumber, int frameNumber, int blendFrameCount, const BezierCurve2D& blendCurve)
	{
		SetAnimation(item, item.ObjectNumber, animNumber, frameNumber, blendFrameCount, blendCurve);
	}

	void SetAnimation(ItemInfo* item, int animNumber, int frameNumber, int blendFrameCount, const BezierCurve2D& blendCurve)
	{
		SetAnimation(*item, item->ObjectNumber, animNumber, frameNumber, blendFrameCount, blendCurve);
	}

	void SetStateDispatch(ItemInfo& item, const StateDispatchData& dispatch)
	{
		float frameNumber = Remap(
			item.Animation.FrameNumber,
			dispatch.FrameNumberLow, dispatch.FrameNumberHigh,
			dispatch.NextFrameNumberLow, dispatch.NextFrameNumberHigh);

		item.SetAnimBlend(dispatch.BlendFrameCount, dispatch.BlendCurve);
		item.Animation.AnimNumber = dispatch.NextAnimNumber;
		item.Animation.FrameNumber = (int)round(frameNumber);
	}

	void ClampRotation(Pose& outPose, short angle, short rot)
	{
		if (angle <= rot)
		{
			outPose.Orientation.y += (angle >= -rot) ? angle : -rot;
		}
		else
		{
			outPose.Orientation.y += rot;
		}
	}
}
