#include "framework.h"
#include "Game/animation.h"

#include "Game/camera.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
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

using namespace TEN::Collision::Point;
using namespace TEN::Entities::Generic;
using namespace TEN::Math;
using TEN::Renderer::g_Renderer;

constexpr auto VERTICAL_VELOCITY_GRAVITY_THRESHOLD = CLICK(0.5f);

// NOTE: 0 frames counts as 1.
static unsigned int GetNonZeroFrameCount(const AnimData& anim)
{
	unsigned int frameCount = anim.frameEnd - anim.frameBase;
	return ((frameCount > 0) ? frameCount : 1);
}

static void PerformAnimCommands(ItemInfo& item, bool isFrameBased)
{
	const auto& anim = GetAnimData(item);

	// No commands; return early.
	if (anim.NumCommands == 0)
		return;

	// Get command data pointer.
	int* commandDataPtr = &g_Level.Commands[anim.CommandIndex];

	for (int i = anim.NumCommands; i > 0; i--)
	{
		auto animCommand = (AnimCommandType)commandDataPtr[0];
		commandDataPtr++;

		switch (animCommand)
		{
		case AnimCommandType::MoveOrigin:
			if (!isFrameBased)
			{
				TranslateItem(&item, item.Pose.Orientation.y, commandDataPtr[2], commandDataPtr[1], commandDataPtr[0]);

				if (item.IsLara())
				{
					auto bounds = GameBoundingBox(&item);
					UpdateLaraRoom(&item, -bounds.GetHeight() / 2, -commandDataPtr[0], -commandDataPtr[2]);
				}
				else
				{
					UpdateItemRoom(item.Index);
				}

				item.DisableInterpolation = true;
			}

			commandDataPtr += 3;
			break;

		case AnimCommandType::JumpVelocity:
			if (!isFrameBased)
			{
				item.Animation.IsAirborne = true;
				item.Animation.Velocity.y = commandDataPtr[0];
				item.Animation.Velocity.z = commandDataPtr[1];

				if (item.IsLara())
				{
					auto& player = GetLaraInfo(item);

					if (player.Context.CalcJumpVelocity != 0)
					{
						item.Animation.Velocity.y = player.Context.CalcJumpVelocity;
						player.Context.CalcJumpVelocity = 0;
					}
				}
			}

			commandDataPtr += 2;
			break;

		case AnimCommandType::Deactivate:
			if (!isFrameBased)
			{
				if (Objects[item.ObjectNumber].intelligent && !item.AfterDeath)
					item.AfterDeath = 1;

				item.Status = ITEM_DEACTIVATED;
			}

			break;

		case AnimCommandType::AttackReady:
			if (!isFrameBased && item.IsLara())
			{
				auto& player = GetLaraInfo(item);

				if (player.Control.HandStatus != HandStatus::Special)
					player.Control.HandStatus = HandStatus::Free;
			}

			break;

		case AnimCommandType::SoundEffect:
		{
			int frameNumber = commandDataPtr[0];
			if (isFrameBased && item.Animation.FrameNumber == frameNumber)
			{
				// Get sound ID and sound environment flag.
				int soundID = commandDataPtr[1];
				auto requiredSoundEnv = (SoundEnvironment)commandDataPtr[2];

				int roomNumberAtPos = (item.RoomNumber == NO_VALUE) ? Camera.pos.RoomNumber : GetPointCollision(item).GetRoomNumber();
				bool isWater = TestEnvironment(ENV_FLAG_WATER, roomNumberAtPos);
				bool isSwamp = TestEnvironment(ENV_FLAG_SWAMP, roomNumberAtPos);

				// Get sound environment for sound effect.
				auto soundEnv = std::optional<SoundEnvironment>();
				switch (requiredSoundEnv)
				{
				case SoundEnvironment::Always:
					soundEnv = SoundEnvironment::Always;
					break;

				case SoundEnvironment::Land:
					if (!isWater && !isSwamp)
						soundEnv = SoundEnvironment::Land;

					break;

				case SoundEnvironment::ShallowWater:
					if (isWater)
					{
						// HACK: Must update assets before removing this exception for water creatures.
						const auto& object = Objects[item.ObjectNumber];
						soundEnv = object.waterCreature ? SoundEnvironment::Underwater : SoundEnvironment::ShallowWater;
					}

					break;

				case SoundEnvironment::Swamp:
					if (isSwamp)
						soundEnv = SoundEnvironment::Swamp;

					break;

				case SoundEnvironment::Underwater:
					if (isWater || isSwamp)
						soundEnv = SoundEnvironment::Underwater;

					break;
				}

				if (soundEnv.has_value())
					SoundEffect(soundID, &item.Pose, *soundEnv);
			}

			commandDataPtr += 3;
			break;
		}

		case AnimCommandType::Flipeffect:
			if (isFrameBased && item.Animation.FrameNumber == commandDataPtr[0])
				DoFlipEffect(commandDataPtr[1], &item);

			commandDataPtr += 2;
			break;

		case AnimCommandType::DisableInterpolation:
			if (isFrameBased && item.Animation.FrameNumber == commandDataPtr[0])
				item.DisableInterpolation = true;

			commandDataPtr += 1;
			break;

		default:
			break;
		}
	}
}

void AnimateItem(ItemInfo* item)
{
	if (!item->IsLara())
	{
		item->TouchBits.ClearAll();
		item->HitStatus = false;
	}

	PerformAnimCommands(*item, true);
	item->Animation.FrameNumber++;

	const auto* animPtr = &GetAnimData(*item);

	if (animPtr->NumStateDispatches > 0 && GetStateDispatch(item, *animPtr))
	{
		animPtr = &GetAnimData(*item);

		item->Animation.ActiveState = animPtr->ActiveState;

		if (!item->IsLara())
		{
			if (item->Animation.RequiredState == item->Animation.ActiveState)
				item->Animation.RequiredState = NO_VALUE;
		}
	}

	if (item->Animation.FrameNumber > animPtr->frameEnd)
	{
		PerformAnimCommands(*item, false);

		item->Animation.AnimNumber = animPtr->JumpAnimNum;
		item->Animation.FrameNumber = animPtr->JumpFrameNum;

		animPtr = &GetAnimData(*item);

		if (!item->IsLara())
		{
			if (item->Animation.ActiveState != animPtr->ActiveState)
			{
				item->Animation.ActiveState = animPtr->ActiveState;
				item->Animation.TargetState = animPtr->ActiveState;
			}

			if (item->Animation.RequiredState == item->Animation.ActiveState)
				item->Animation.RequiredState = NO_VALUE;
		}
		else
		{
			item->Animation.ActiveState = animPtr->ActiveState;
		}

		// TODO: Theoretically this is better than above block, but it must be checked. -- Sezz 2023.03.31
		/*if (item->Animation.ActiveState != animPtr->ActiveState)
		{
			item->Animation.ActiveState =
			item->Animation.TargetState = animPtr->ActiveState;
		}

		if (!item->IsLara())
		{
			if (item->Animation.RequiredState == item->Animation.ActiveState)
				item->Animation.RequiredState = NO_VALUE;
		}*/
	}

	unsigned int frameCount = GetNonZeroFrameCount(*animPtr);
	int currentFrame = item->Animation.FrameNumber - animPtr->frameBase;

	auto animAccel = (animPtr->VelocityEnd - animPtr->VelocityStart) / frameCount;
	auto animVel = animPtr->VelocityStart + (animAccel * currentFrame);

	if (item->Animation.IsAirborne)
	{
		if (item->IsLara())
		{
			if (TestEnvironment(ENV_FLAG_SWAMP, item))
			{
				item->Animation.Velocity.z -= item->Animation.Velocity.z / 8;
				if (abs(item->Animation.Velocity.z) < 8.0f)
				{
					item->Animation.IsAirborne = false;
					item->Animation.Velocity.z = 0.0f;
				}

				if (item->Animation.Velocity.y > VERTICAL_VELOCITY_GRAVITY_THRESHOLD)
					item->Animation.Velocity.y /= 2;
				item->Animation.Velocity.y -= item->Animation.Velocity.y / 4;

				if (item->Animation.Velocity.y < 4.0f)
					item->Animation.Velocity.y = 4.0f;
				item->Pose.Position.y += item->Animation.Velocity.y;
			}
			else
			{
				if (item->Animation.ActiveState != LS_FLY_CHEAT)
				{
					item->Animation.Velocity.y += GetEffectiveGravity(item->Animation.Velocity.y);
					item->Animation.Velocity.z += animAccel.z;

					item->Pose.Position.y += item->Animation.Velocity.y;
				}
			}
		}
		else
		{
			item->Animation.Velocity.y += GetEffectiveGravity(item->Animation.Velocity.y);
			item->Pose.Position.y += item->Animation.Velocity.y;
		}
	}
	else
	{
		if (item->IsLara())
		{
			const auto& player = GetLaraInfo(*item);

			bool isInSwamp = (player.Control.WaterStatus == WaterStatus::Wade && TestEnvironment(ENV_FLAG_SWAMP, item));
			item->Animation.Velocity.z = isInSwamp ? (animVel.z / 2) : animVel.z;
		}
		else
		{
			item->Animation.Velocity.x = animVel.x;
			item->Animation.Velocity.z = animVel.z;
		}
	}
	
	if (item->IsLara())
	{
		const auto& player = GetLaraInfo(*item);

		item->Animation.Velocity.x = animVel.x;

		if (player.Control.Rope.Ptr != -1)
			DelAlignLaraToRope(item);

		if (!player.Control.IsMoving)
			TranslateItem(item, player.Control.MoveAngle, item->Animation.Velocity.z, 0.0f, item->Animation.Velocity.x);

		// Update matrices.
		g_Renderer.UpdateLaraAnimations(true);
	}
	else
	{
		TranslateItem(item, item->Pose.Orientation.y, item->Animation.Velocity.z, 0.0f, item->Animation.Velocity.x);

		// Update matrices.
		g_Renderer.UpdateItemAnimations(item->Index, true);
	}
}

bool HasStateDispatch(const ItemInfo* item, int targetState)
{
	const auto& anim = GetAnimData(*item);

	// No dispatches; return early.
	if (anim.NumStateDispatches <= 0)
		return false;

	if (targetState == NO_VALUE)
		targetState = item->Animation.TargetState;

	// Iterate over animation's state dispatches.
	for (int i = 0; i < anim.NumStateDispatches; i++)
	{
		const auto& dispatch = g_Level.Changes[anim.StateDispatchIndex + i];
		
		if (dispatch.TargetState != targetState)
			continue;

		// Iterate over dispatch frame ranges.
		for (int j = 0; j < dispatch.NumberRanges; j++)
		{
			const auto& range = g_Level.Ranges[dispatch.RangeIndex + j];

			// Check if current frame is within dispatch range.
			if (item->Animation.FrameNumber >= range.StartFrame &&
				item->Animation.FrameNumber <= range.EndFrame)
			{
				return true;
			}
		}
	}

	return false;
}

bool TestAnimNumber(const ItemInfo& item, int animNumber)
{
	const auto& object = Objects[item.Animation.AnimObjectID];
	return (item.Animation.AnimNumber == (object.animIndex + animNumber));
}

bool TestLastFrame(ItemInfo* item, int animNumber)
{
	const auto& object = Objects[item->Animation.AnimObjectID];

	if (animNumber == NO_VALUE)
		animNumber = item->Animation.AnimNumber - object.animIndex;

	// Animation to test doesn't match; return early.
	int animIndex = object.animIndex + animNumber;
	if (item->Animation.AnimNumber != animIndex)
		return false;

	const auto& anim = GetAnimData(object, animNumber);
	return (item->Animation.FrameNumber >= anim.frameEnd);
}

bool TestAnimFrame(const ItemInfo& item, int frameStart)
{
	const auto& anim = GetAnimData(item);
	return (item.Animation.FrameNumber == (anim.frameBase + frameStart));
}

bool TestAnimFrameRange(const ItemInfo& item, int frameStart, int frameEnd)
{
	const auto& anim = GetAnimData(item);
	return (item.Animation.FrameNumber >= (anim.frameBase + frameStart) &&
			item.Animation.FrameNumber <= (anim.frameBase + frameEnd));
}

void TranslateItem(ItemInfo* item, short headingAngle, float forward, float down, float right)
{
	item->Pose.Translate(headingAngle, forward, down, right);
}

void TranslateItem(ItemInfo* item, const EulerAngles& orient, float distance)
{
	item->Pose.Translate(orient, distance);
}

void TranslateItem(ItemInfo* item, const Vector3& direction, float distance)
{
	item->Pose.Translate(direction, distance);
}

void SetAnimation(ItemInfo& item, GAME_OBJECT_ID animObjectID, int animNumber, int frameNumber)
{
	const auto& animObject = Objects[animObjectID];
	int animIndex = animObject.animIndex + animNumber;

	// Animation is missing; return early.
	if (animIndex < 0 || animIndex >= g_Level.Anims.size())
	{
		TENLog(
			"Attempted to set missing animation " + std::to_string(animNumber) +
			((animObjectID == item.ObjectNumber) ? "" : (" from object " + GetObjectName(animObjectID))) +
			" for object " + GetObjectName(item.ObjectNumber),
			LogLevel::Warning);

		return;
	}

	const auto& anim = GetAnimData(animObject, animNumber);
	int frameIndex = anim.frameBase + frameNumber;

	// Animation already set; return early.
	if (item.Animation.AnimObjectID == animObjectID &&
		item.Animation.AnimNumber == animIndex &&
		item.Animation.FrameNumber == frameIndex)
	{
		return;
	}

	item.Animation.AnimObjectID = animObjectID;
	item.Animation.AnimNumber = animIndex;
	item.Animation.FrameNumber = frameIndex;
	item.Animation.ActiveState =
	item.Animation.TargetState = anim.ActiveState;
}

void SetAnimation(ItemInfo& item, int animNumber, int frameNumber)
{
	SetAnimation(item, item.ObjectNumber, animNumber, frameNumber);
}

void SetAnimation(ItemInfo* item, int animNumber, int frameNumber)
{
	SetAnimation(*item, item->ObjectNumber, animNumber, frameNumber);
}

const AnimData& GetAnimData(int animIndex)
{
	if (animIndex < 0 || animIndex >= g_Level.Anims.size())
	{
		TENLog("GetAnimData() attempted to access incorrect animation.", LogLevel::Error);
		return g_Level.Anims[0];
	}

	return g_Level.Anims[animIndex];
}

const AnimData& GetAnimData(GAME_OBJECT_ID objectID, int animNumber)
{
	const auto& object = Objects[objectID];
	return GetAnimData(object, animNumber);
}

const AnimData& GetAnimData(const ObjectInfo& object, int animNumber)
{
	return g_Level.Anims[object.animIndex + animNumber];
}

const AnimData& GetAnimData(const ItemInfo& item, int animNumber)
{
	if (animNumber == NO_VALUE)
	{
		if (item.Animation.AnimNumber == NO_VALUE)
		{
			TENLog("Object " + GetObjectName(item.ObjectNumber) + " has no animations.", LogLevel::Error);
			return GetAnimData(0);
		}

		return GetAnimData(item.Animation.AnimNumber);
	}

	const auto& object = Objects[item.Animation.AnimObjectID];
	return GetAnimData(object, animNumber);
}

const AnimData& GetAnimData(const ItemInfo* item, int animNumber)
{
	return (GetAnimData(*item, animNumber));
}

AnimFrameInterpData GetFrameInterpData(const ItemInfo& item)
{
	const auto& anim = GetAnimData(item);

	// Normalize animation's current frame number into keyframe range.
	int frameNumber = GetFrameNumber(item);
	float frameNumberNorm = frameNumber / (float)anim.Interpolation;

	// Calculate keyframe numbers defining interpolated frame and get pointers to them.
	int frame0 = (int)floor(frameNumberNorm);
	int frame1 = (int)ceil(frameNumberNorm);
	const auto* framePtr0 = &g_Level.Frames[anim.FramePtr + frame0];
	const auto* framePtr1 = &g_Level.Frames[anim.FramePtr + frame1];

	// Calculate interpolation alpha between keyframes.
	float alpha = (1.0f / anim.Interpolation) * (frameNumber % anim.Interpolation);

	// Return frame interpolation data.
	return AnimFrameInterpData{ framePtr0, framePtr1, alpha };
}

const AnimFrame& GetAnimFrame(const ItemInfo& item, int animNumber, int frameNumber)
{
	return *GetFrame(item.ObjectNumber, animNumber, frameNumber);
}

const AnimFrame* GetFrame(GAME_OBJECT_ID objectID, int animNumber, int frameNumber)
{
	const auto& object = Objects[objectID];

	int animIndex = object.animIndex + animNumber;
	TENAssert(animIndex < g_Level.Anims.size(), "GetFrame() attempted to access missing animation.");

	const auto& anim = GetAnimData(object, animNumber);

	// Get and clamp frame count.
	unsigned int frameCount = anim.frameEnd - anim.frameBase;
	if (frameNumber > frameCount)
		frameNumber = frameCount;

	// Interpolate and return frame pointer.
	const auto* framePtr = &g_Level.Frames[anim.FramePtr];
	framePtr += frameNumber / anim.Interpolation;
	return framePtr;
}

const AnimFrame* GetFirstFrame(GAME_OBJECT_ID objectID, int animNumber)
{
	return GetFrame(objectID, animNumber, 0);
}

const AnimFrame* GetLastFrame(GAME_OBJECT_ID objectID, int animNumber)
{
	return GetFrame(objectID, animNumber, INT_MAX);
}

const AnimFrame& GetBestFrame(const ItemInfo& item)
{
	auto frameData = GetFrameInterpData(item);
	return ((frameData.Alpha <= 0.5f) ? *frameData.FramePtr0 : *frameData.FramePtr1);
}

float GetEffectiveGravity(float verticalVel)
{
	return ((verticalVel >= VERTICAL_VELOCITY_GRAVITY_THRESHOLD) ? 1.0f : g_GameFlow->GetSettings()->Physics.Gravity);
}

int GetAnimNumber(const ItemInfo& item)
{
	const auto& object = Objects[item.Animation.AnimObjectID];
	return (item.Animation.AnimNumber - object.animIndex);
}

int GetAnimIndex(const ItemInfo& item, int animNumber)
{
	const auto& object = Objects[item.Animation.AnimObjectID];
	return (object.animIndex + animNumber);
}

int GetFrameNumber(const ItemInfo& item)
{
	const auto& anim = GetAnimData(item);
	return (item.Animation.FrameNumber - anim.frameBase);
}

int GetFrameNumber(ItemInfo* item)
{
	return GetFrameNumber(*item);
}

int GetFrameIndex(ItemInfo* item, int frameNumber)
{
	int animNumber = item->Animation.AnimNumber - Objects[item->Animation.AnimObjectID].animIndex;
	return GetFrameIndex(item->Animation.AnimObjectID, animNumber, frameNumber);
}

int GetFrameIndex(GAME_OBJECT_ID objectID, int animNumber, int frameNumber)
{
	const auto& object = Objects[objectID];
	const auto& anim = GetAnimData(object, animNumber);

	return (anim.frameBase + frameNumber);
}

int GetFrameCount(int animIndex)
{
	if (animIndex < 0 || g_Level.Anims.size() <= animIndex)
		return 0;

	const auto& anim = GetAnimData(animIndex);
	return (anim.frameEnd - anim.frameBase);
}

int GetNextAnimState(ItemInfo* item)
{
	return GetNextAnimState(item->Animation.AnimObjectID, item->Animation.AnimNumber);
}

int GetNextAnimState(int objectID, int animNumber)
{
	const auto& object = Objects[objectID];
	const auto& anim = GetAnimData(object, animNumber);

	const auto& nextAnim = GetAnimData(anim.JumpAnimNum);
	return nextAnim.ActiveState;
}

bool GetStateDispatch(ItemInfo* item, const AnimData& anim)
{
	// Active and target states already match; return early.
	if (item->Animation.ActiveState == item->Animation.TargetState)
		return false;

	// No dispatches; return early.
	if (anim.NumStateDispatches <= 0)
		return false;

	// Iterate over animation's state dispatches.
	for (int i = 0; i < anim.NumStateDispatches; i++)
	{
		const auto& dispatch = g_Level.Changes[anim.StateDispatchIndex + i];

		if (dispatch.TargetState != item->Animation.TargetState)
			continue;

		// Iterate over dispatch frame ranges.
		for (int j = 0; j < dispatch.NumberRanges; j++)
		{
			const auto& range = g_Level.Ranges[dispatch.RangeIndex + j];

			// Set new animation if current frame is within dispatch range.
			if (item->Animation.FrameNumber >= range.StartFrame &&
				item->Animation.FrameNumber <= range.EndFrame)
			{
				item->Animation.AnimNumber = range.LinkAnimNum;
				item->Animation.FrameNumber = range.LinkFrameNum;
				return true;
			}
		}
	}

	return false;
}

void DrawAnimatingItem(ItemInfo* item)
{
	// TODO: to refactor
	// Empty stub because actually we disable items drawing when drawRoutine pointer is nullptr in ObjectInfo
}

void ClampRotation(Pose& outPose, short angle, short rotation)
{
	if (angle <= rotation)
	{
		if (angle >= -rotation)
			outPose.Orientation.y += angle;
		else
			outPose.Orientation.y -= rotation;
	}
	else
	{
		outPose.Orientation.y += rotation;
	}
}

Vector3i GetJointPosition(const ItemInfo& item, int jointIndex, const Vector3i& relOffset)
{
	bool incorrectJoint = false;
	if (jointIndex < 0 || jointIndex >= Objects[item.ObjectNumber].nmeshes)
	{
		TENLog("Unknown joint ID specified for object " + GetObjectName(item.ObjectNumber), LogLevel::Warning, LogConfig::All);
		incorrectJoint = true;
	}

	// Always return object's root position if it's invisible, because we can't predict its
	// joint position otherwise, since it's not animated.
	if (incorrectJoint || Objects[item.ObjectNumber].drawRoutine == nullptr || item.Status == ITEM_INVISIBLE)
		return Geometry::TranslatePoint(item.Pose.Position, item.Pose.Orientation, relOffset);

	// Use matrices done in renderer to transform relative offset.
	return Vector3i(g_Renderer.GetMoveableBonePosition(item.Index, jointIndex, relOffset.ToVector3()));
}

Vector3i GetJointPosition(ItemInfo* item, int jointIndex, const Vector3i& relOffset)
{
	return GetJointPosition(*item, jointIndex, relOffset);
}

Vector3i GetJointPosition(ItemInfo* item, const CreatureBiteInfo& bite)
{
	return GetJointPosition(item, bite.BoneID, bite.Position);
}

Vector3i GetJointPosition(const ItemInfo& item, const CreatureBiteInfo& bite)
{
	return GetJointPosition(item, bite.BoneID, bite.Position);
}

Vector3 GetJointOffset(GAME_OBJECT_ID objectID, int jointIndex)
{
	const auto& object = Objects[objectID];
	int boneIndex = object.boneIndex + (jointIndex * 4);

	if (g_Level.Bones.size() <= boneIndex)
		return Vector3::Zero;

	int* bonePtr = &g_Level.Bones[object.boneIndex + (jointIndex * 4)];
	return Vector3(*(bonePtr + 1), *(bonePtr + 2), *(bonePtr + 3));
}

Quaternion GetBoneOrientation(const ItemInfo& item, int boneID)
{
	return g_Renderer.GetMoveableBoneOrientation(item.Index, boneID);
}

// NOTE: Will not work for bones at ends of hierarchies.
float GetBoneLength(GAME_OBJECT_ID objectID, int boneIndex)
{
	const auto& object = Objects[objectID];

	if (object.nmeshes == boneIndex)
		return 0.0f;

	auto nextBoneOffset = GetJointOffset(objectID, boneIndex + 1);
	return nextBoneOffset.Length();
}
