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
		// Compute relative translation and curve alpha.
		float alpha = (float)frameNumber / (float)std::max(EndFrameNumber, 1);
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
		if (!HasRootTranslation() && !HasRootRotation())
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
		if (HasRootTranslation())
		{
			const auto& rootPos = Frames[frameNumber].RootPosition;
			const auto& prevRootPos = Frames[frameNumber - 1].RootPosition;
			auto rootTranslation = rootPos - prevRootPos;

			translation = Vector3(
				(Flags & (int)AnimFlags::RootMotionTranslationX) ? rootTranslation.x : 0.0f,
				(Flags & (int)AnimFlags::RootMotionTranslationY) ? rootTranslation.y : 0.0f,
				(Flags & (int)AnimFlags::RootMotionTranslationZ) ? rootTranslation.z : 0.0f);
		}

		// Compute relative rotation.
		auto rot = EulerAngles::Identity;
		if (HasRootRotation())
		{
			const auto& rootOrient = EulerAngles(Frames[frameNumber].BoneOrientations.front());
			const auto& prevRootOrient = EulerAngles(Frames[frameNumber - 1].BoneOrientations.front());
			auto rootRot = rootOrient - prevRootOrient;

			rot = EulerAngles(
				(Flags & (int)AnimFlags::RootMotionRotationX) ? rootRot.x : ANGLE(0.0f),
				(Flags & (int)AnimFlags::RootMotionRotationY) ? rootRot.y : ANGLE(0.0f),
				(Flags & (int)AnimFlags::RootMotionRotationZ) ? rootRot.z : ANGLE(0.0f));
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
		if (!HasRootTranslation() && !HasRootRotation())
			return {};

		// Handle frame 0.
		if (frameNumber == 0)
			return {};

		// Get relative translation counteraction.
		auto translation = Vector3::Zero;
		if (HasRootTranslation())
		{
			const auto& rootPos = Frames[frameNumber].RootPosition;
			const auto& baseRootPos = Frames.front().RootPosition;
			auto rootTranslation = baseRootPos - rootPos;

			translation = Vector3(
				(Flags & (int)AnimFlags::RootMotionTranslationX) ? rootTranslation.x : 0.0f,
				(Flags & (int)AnimFlags::RootMotionTranslationY) ? rootTranslation.y : 0.0f,
				(Flags & (int)AnimFlags::RootMotionTranslationZ) ? rootTranslation.z : 0.0f);
		}

		// Get relative rotation counteraction.
		auto rot = EulerAngles::Identity;
		if (HasRootRotation())
		{
			auto rootOrient = EulerAngles(Frames[frameNumber].BoneOrientations.front());
			auto baseRootOrient = EulerAngles(Frames.front().BoneOrientations.front());
			auto rootRot = baseRootOrient - rootOrient;

			rot = EulerAngles(
				(Flags & (int)AnimFlags::RootMotionRotationX) ? rootRot.x : ANGLE(0.0f),
				(Flags & (int)AnimFlags::RootMotionRotationY) ? rootRot.y : ANGLE(0.0f),
				(Flags & (int)AnimFlags::RootMotionRotationZ) ? rootRot.z : ANGLE(0.0f));
		}

		// Return root motion counteraction.
		return RootMotionData
		{
			translation,
			rot
		};
	}

	bool AnimData::HasRootTranslation() const
	{
		return (Flags & ((int)AnimFlags::RootMotionTranslationX | (int)AnimFlags::RootMotionTranslationY | (int)AnimFlags::RootMotionTranslationZ));
	}

	bool AnimData::HasRootRotation() const
	{
		return (Flags & ((int)AnimFlags::RootMotionRotationX | (int)AnimFlags::RootMotionRotationY | (int)AnimFlags::RootMotionRotationZ));
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

	bool IsSoundEffectCommandActive(const ItemInfo& item, int soundId)
	{
		const auto& animData = GetAnimData(item);
		int currentFrame = item.Animation.FrameNumber;

		for (const auto& cmdPtr : animData.Commands)
		{
			auto soundCmd = std::dynamic_pointer_cast<const TEN::Animation::SoundEffectCommand>(cmdPtr);
			if (soundCmd)
			{
				if (soundCmd->GetFrameNumber() == currentFrame && soundCmd->GetSoundID() == soundId)
					return true;
			}
		}
		return false;
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
			// Clamp to end frame number to remain in bounds.
			item.Animation.FrameNumber = anim->EndFrameNumber;
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

		// TODO: Better handling of player airborne status.
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
			// Compute interpolation parameters.
			auto prevVel = Vector3::Zero;
			float blendAlpha = 1.0f;
			if (item.Animation.Blend.IsEnabled())
			{
				prevVel = item.Animation.Blend.Velocity;
				blendAlpha = item.Animation.Blend.GetAlpha();
			}

			// TODO: Also get Y velocity from animation. Needs changes in WT.
			auto vel = Vector3::Lerp(prevVel, fixedMotion.Translation + rootMotion.Translation, blendAlpha);
			if (item.IsLara())
			{
				const auto& player = GetLaraInfo(item);

				bool isInSwamp = (player.Control.WaterStatus == WaterStatus::Wade && TestEnvironment(ENV_FLAG_SWAMP, &item));
				item.Animation.Velocity.x = vel.x * (isInSwamp ? 0.5f : 1.0f);
				item.Animation.Velocity.z = vel.z * (isInSwamp ? 0.5f : 1.0f);
			}
			else
			{
				item.Animation.Velocity.x = vel.x;
				item.Animation.Velocity.z = vel.z;
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

		// Draw debug.
		if (item.IsLara() && g_Renderer.GetDebugPage() == RendererDebugPage::PlayerStats)
		{
			PrintDebugMessage("Fixed motion: %s", (fixedMotion.Translation != Vector3::Zero) ? "Enabled" : "Disabled");
			PrintDebugMessage("Root motion: %s", (rootMotion.Translation != Vector3::Zero || rootMotion.Rotation != EulerAngles::Identity) ? "Enabled" : "Disabled");
			PrintDebugMessage("Blend: %s", item.Animation.Blend.IsEnabled() ? "Enabled" : "Disabled");
			PrintDebugMessage("Blend frame number: %d", item.Animation.Blend.FrameNumber);
			PrintDebugMessage("Blend frame count: %d", item.Animation.Blend.FrameCount);
			PrintDebugMessage("Blend curve:");
			PrintDebugMessage("    Start: %.3f, %.3f", item.Animation.Blend.Curve.GetStart().x, item.Animation.Blend.Curve.GetStart().y);
			PrintDebugMessage("    End: %.3f, %.3f", item.Animation.Blend.Curve.GetEnd().x, item.Animation.Blend.Curve.GetEnd().y);
			PrintDebugMessage("    Start handle: %.3f, %.3f", item.Animation.Blend.Curve.GetStartHandle().x, item.Animation.Blend.Curve.GetStartHandle().y);
			PrintDebugMessage("    End handle: %.3f, %.3f", item.Animation.Blend.Curve.GetEndHandle().x, item.Animation.Blend.Curve.GetEndHandle().y);
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

		// Check for animation number mismatch.
		if (item.Animation.AnimNumber != animNumber)
			return false;

		// Clamp to real end frame.
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
		if (object.Animations.size() > animNumber)
			return object.Animations[animNumber];

		TENLog("Attempted to access invalid animation.", LogLevel::Error);
		return (object.Animations.empty() ? Objects[0].Animations[0] : object.Animations[0]);
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

		frameNumber = std::clamp(frameNumber, 0, (int)anim.Frames.size() - 1);
		return anim.Frames[frameNumber];
	}

	const FrameData& GetFrame(const ItemInfo& item, int animNumber, int frameNumber)
	{
		return GetFrame(item.Animation.AnimObjectID, animNumber, frameNumber);
	}

	const FrameData& GetFrame(const ItemInfo& item)
	{
		return GetFrame(item.Animation.AnimObjectID, item.Animation.AnimNumber, item.Animation.FrameNumber);
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

	int GetSystemBlendDuration()
	{
		return std::max(0, g_GameFlow->GetSettings()->Animations.SystemBlendDuration);
	}

	Vector3i GetJointPosition(const ItemInfo& item, int boneID, const Vector3i& relOffset)
	{
		bool incorrectBone = false;
		if (boneID < 0 || boneID >= Objects[item.ObjectNumber].nmeshes)
		{
			TENLog(fmt::format("Unknown bone ID specified for object {}.", item.ObjectNumber), LogLevel::Warning, LogConfig::All);
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

	void SetAnimationFromSlot(ItemInfo& item, GAME_OBJECT_ID animObjectID, int animNumber, int frameNumber, int blendFrameCount, const BezierCurve2& blendCurve)
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
			bool isBorrowedAnim = animObjectID != item.ObjectNumber;
			TENLog(
				fmt::format(
					"Attempted to set missing animation {}{} for moveable of object type {}.",
					animNumber, isBorrowedAnim ? fmt::format(" (borrowed from object {})", GetObjectName(animObjectID)) : "", GetObjectName(item.ObjectNumber)),
				LogLevel::Warning);

			return;
		}

		const auto& anim = GetAnimData(animObject, animNumber);

		// Check if frame is missing.
		if (frameNumber < 0 || frameNumber >= anim.Frames.size())
		{
			bool isBorrowedAnim = animObjectID != item.ObjectNumber;
			TENLog(
				fmt::format(
					"Attempted to set missing frame {} from animation {}{} for moveable of object type {}.",
					frameNumber, animNumber, isBorrowedAnim ? fmt::format(" (borrowed from object {})", GetObjectName(animObjectID)) : "", GetObjectName(item.ObjectNumber)),
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

	void SetAnimation(ItemInfo& item, int animNumber, int frameNumber, int blendFrameCount, const BezierCurve2& blendCurve)
	{
		SetAnimationFromSlot(item, item.ObjectNumber, animNumber, frameNumber, blendFrameCount, blendCurve);
	}

	void SetAnimation(ItemInfo* item, int animNumber, int frameNumber, int blendFrameCount, const BezierCurve2& blendCurve)
	{
		SetAnimationFromSlot(*item, item->ObjectNumber, animNumber, frameNumber, blendFrameCount, blendCurve);
	}

	void SetExtendedAnimation(ItemInfo& item, int rawAnimNumber)
	{
		constexpr int ANIM_NUMBER_MAX = 1000;

		int nativeAnimCount = (int)Objects[item.ObjectNumber].Animations.size();
		rawAnimNumber = std::abs(rawAnimNumber);

		if (rawAnimNumber < nativeAnimCount)
		{
			SetAnimation(item, rawAnimNumber);
			return;
		}

		int relativeAnimNumber = rawAnimNumber - nativeAnimCount;

		if (Objects[GAME_OBJECT_ID::ID_LARA_EXTRA_ANIMS].loaded && Objects[GAME_OBJECT_ID::ID_LARA_EXTRA_ANIMS].Animations.size() > relativeAnimNumber)
		{
			SetAnimation(item, GAME_OBJECT_ID::ID_LARA_EXTRA_ANIMS, relativeAnimNumber);
			return;
		}

		if (rawAnimNumber >= ANIM_NUMBER_MAX)
		{
			int objectNumber = rawAnimNumber / ANIM_NUMBER_MAX;
			int animNumber = rawAnimNumber % ANIM_NUMBER_MAX;

			if (objectNumber < GAME_OBJECT_ID::ID_NUMBER_OBJECTS && Objects[objectNumber].loaded && Objects[objectNumber].Animations.size() > animNumber)
			{
				SetAnimation(item, (GAME_OBJECT_ID)objectNumber, animNumber);
				return;
			}
		}

		TENLog(fmt::format("Can't set extended animation {} for {} from the external slot: animation data missing.", rawAnimNumber, item.Name), LogLevel::Warning);
		SetAnimation(item, 0);
		return;
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
