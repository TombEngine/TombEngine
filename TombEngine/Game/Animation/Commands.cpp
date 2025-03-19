#include "framework.h"
#include "Game/Animation/Commands.h"

#include "Game/camera.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/control/flipeffect.h"
#include "Game/items.h"
#include "Game/lara/lara.h"
#include "Game/lara/lara_helpers.h"
#include "Game/Setup.h"
#include "Sound/sound.h"

using namespace TEN::Collision::Point;

namespace TEN::Animation
{
	void MoveOriginCommand::Execute(ItemInfo& item, bool isFrameBased) const
	{
		if (isFrameBased)
			return;

		item.Pose.Translate(item.Pose.Orientation.y, _relOffset.z, _relOffset.y, _relOffset.x);

		if (item.IsLara())
		{
			// NOTE: GameBoundingBox constructor always clamps to last frame to avoid errors.
			auto bounds = GameBoundingBox(&item);
			UpdateLaraRoom(&item, -bounds.GetHeight() / 2, -_relOffset.x, -_relOffset.z);
		}
		else
		{
			UpdateItemRoom(item.Index);
		}
	}

	void JumpVelocityCommand::Execute(ItemInfo& item, bool isFrameBased) const
	{
		if (isFrameBased)
			return;

		item.Animation.IsAirborne = true;
		item.Animation.Velocity = _jumpVelocity;

		if (item.IsLara())
		{
			auto& player = GetLaraInfo(item);
			if (player.Context.CalcJumpVelocity != 0.0f)
			{
				item.Animation.Velocity.y = player.Context.CalcJumpVelocity;
				player.Context.CalcJumpVelocity = 0.0f;
			}
		}
	}

	void AttackReadyCommand::Execute(ItemInfo& item, bool isFrameBased) const
	{
		if (isFrameBased || !item.IsLara())
			return;

		auto& player = GetLaraInfo(item);
		if (player.Control.HandStatus != HandStatus::Special)
			player.Control.HandStatus = HandStatus::Free;
	}

	void DeactivateCommand::Execute(ItemInfo& item, bool isFrameBased) const
	{
		if (isFrameBased)
			return;

		const auto& object = Objects[item.ObjectNumber];
		if (object.intelligent && !item.AfterDeath)
			item.AfterDeath = 1;

		item.Status = ITEM_DEACTIVATED;
	}

	void SoundEffectCommand::Execute(ItemInfo& item, bool isFrameBased) const
	{
		if (!isFrameBased || item.Animation.FrameNumber != _frameNumber)
			return;
		
		// FAILSAFE.
		if (item.RoomNumber == NO_VALUE)
		{
			SoundEffect(_soundID, &item.Pose, SoundEnvironment::Always);
			return;
		}

		int roomNumberAtPos = GetPointCollision(item).GetRoomNumber();
		bool isWater = TestEnvironment(ENV_FLAG_WATER, roomNumberAtPos);
		bool isSwamp = TestEnvironment(ENV_FLAG_SWAMP, roomNumberAtPos);

		// Get sound environment for sound effect.
		auto soundEnv = std::optional<SoundEnvironment>();
		switch (_envCondition)
		{
		case SoundEffectEnvCondition::Always:
			soundEnv = SoundEnvironment::Always;
			break;

		case SoundEffectEnvCondition::Land:
			if (!isWater && !isSwamp)
				soundEnv = SoundEnvironment::Land;

			break;

		case SoundEffectEnvCondition::ShallowWater:
			if (isWater)
			{
				// HACK: Must update assets before removing this exception for water creatures.
				const auto& object = Objects[item.ObjectNumber];
				soundEnv = object.waterCreature ? SoundEnvironment::Underwater : SoundEnvironment::ShallowWater;
			}

			break;

		case SoundEffectEnvCondition::Quicksand:
			if (isSwamp)
				soundEnv = SoundEnvironment::Swamp;

			break;

		case SoundEffectEnvCondition::Underwater:
			if (isWater || isSwamp)
				soundEnv = SoundEnvironment::Underwater;

			break;
		}

		if (soundEnv.has_value())
			SoundEffect(_soundID, &item.Pose, *soundEnv);
	}

	void FlipEffectCommand::Execute(ItemInfo& item, bool isFrameBased) const
	{
		if (!isFrameBased || item.Animation.FrameNumber != _frameNumber)
			return;

		int flipEffectID = _flipEffectID & 0x3FFF; // Bits 1-14.
		DoFlipEffect(flipEffectID, &item);
	}

	void DisableInterpolationCommand::Execute(ItemInfo& item, bool isFrameBased) const
	{
		if (!isFrameBased || item.Animation.FrameNumber != _frameNumber)
			return;

		item.DisableInterpolation = true;
	}
}
