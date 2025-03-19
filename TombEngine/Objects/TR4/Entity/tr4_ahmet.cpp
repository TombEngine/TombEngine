#include "framework.h"
#include "Objects/TR4/Entity/tr4_ahmet.h"

#include "Game/collision/Sphere.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/control/lot.h"
#include "Game/effects/effects.h"
#include "Game/effects/weather.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Collision::Sphere;
using namespace TEN::Effects::Environment;
using namespace TEN::Math;

namespace TEN::Entities::TR4
{
	constexpr auto AHMET_SWIPE_ATTACK_DAMAGE = 80;
	constexpr auto AHMET_BITE_ATTACK_DAMAGE	 = 120;

	constexpr auto AHMET_ATTACK_RANGE = SQUARE(BLOCK(0.67f));
	constexpr auto AHMET_AWARE_RANGE  = SQUARE(BLOCK(1));
	constexpr auto AHMET_IDLE_RANGE   = SQUARE(BLOCK(1.25f));
	constexpr auto AHMET_RUN_RANGE    = SQUARE(BLOCK(2.5f));

	constexpr auto AHMET_WALK_FORWARD_TURN_ANGLE = ANGLE(5.0f);
	constexpr auto AHMET_RUN_FORWARD_TURN_ANGLE	 = ANGLE(8.0f);
	constexpr auto AHMET_VIEW_ANGLE				 = ANGLE(45.0f);
	constexpr auto AHMET_ENEMY_ANGLE			 = ANGLE(90.0f);
	
	const auto AhmetBiteLeft  = CreatureBiteInfo(Vector3::Zero, 16);
	const auto AhmetBiteRight = CreatureBiteInfo(Vector3::Zero, 22);
	const auto AhmetBiteJaw	  = CreatureBiteInfo(Vector3::Zero, 11);
	const auto AhmetSwipeAttackLeftJoints  = std::vector<unsigned int>{ 14, 15, 16, 17 };
	const auto AhmetSwipeAttackRightJoints = std::vector<unsigned int>{ 20, 21, 22, 23 };

	enum AhmetState
	{
		// No state 0.
		AHMET_STATE_IDLE = 1,
		AHMET_STATE_WALK_FORWARD = 2,
		AHMET_STATE_RUN_FORWARD = 3,
		AHMET_STATE_SWIPE_ATTACK = 4,
		AHMET_STATE_JUMP_BITE_ATTACK = 5,
		AHMET_STATE_JUMP_SWIPE_ATTACK = 6,
		AHMET_STATE_DEATH = 7
	};

	enum AhmetAnim
	{
		AHMET_ANIM_IDLE = 0,
		AHMET_ANIM_RUN_FORWARD = 1,
		AHMET_ANIM_SWIPE_ATTACK = 2,
		AHMET_ANIM_JUMP_BITE_ATTACK_START = 3,
		AHMET_ANIM_JUMP_BITE_ATTACK_CONTINUE = 4,
		AHMET_ANIM_JUMP_BITE_ATTACK_END = 5,
		AHMET_ANIM_WALK_FORWARD = 6,
		AHMET_ANIM_JUMP_SWIPE_ATTACK_START = 7,
		AHMET_ANIM_JUMP_SWIPE_ATTACK_CONTINUE = 8,
		AHMET_ANIM_JUMP_SWIPE_ATTACK_END = 9,
		AHMET_ANIM_DEATH = 10,
		AHMET_ANIM_IDLE_TO_WALK_FORWARD = 11,
		AHMET_ANIM_WALK_FORWARD_TO_IDLE = 12,
		AHMET_ANIM_IDLE_TO_RUN_FORWARD = 13,
		AHMET_ANIM_RUN_FORWARD_TO_IDLE = 14,
	};

	// TODO
	enum AhmetFlags
	{

	};

	static void TriggerAhmetDeathEffect(ItemInfo* item)
	{
		if (!(Wibble & 7))
		{
			auto spheres = item->GetSpheres();
			const auto* spherePtr = &spheres[(Wibble / 8) & 1];

			// TODO
			for (int i = (int)spheres.size(); i > 0; i--, spherePtr += 2)
				TriggerFireFlame(spherePtr->Center.x, spherePtr->Center.y, spherePtr->Center.z, FlameType::Medium);
		}

		SpawnDynamicLight(
			item->Pose.Position.x,
			item->Pose.Position.y - CLICK(1),
			item->Pose.Position.z,
			13, (GetRandomControl() & 0x3F) - 64, (GetRandomControl() & 0x1F) + 96, 0);
		SoundEffect(SFX_TR4_LOOP_FOR_SMALL_FIRES, &item->Pose);
	}

	void InitializeAhmet(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(*item, AHMET_ANIM_IDLE);
		item->ItemFlags[0] = item->Pose.Position.x / BLOCK(1);
		item->ItemFlags[1] = (item->Pose.Position.y * 4) / BLOCK(1);
		item->ItemFlags[2] = item->Pose.Position.z / BLOCK(1);
	}

	void AhmetControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];

		if (item->TriggerFlags == 1)
		{
			item->TriggerFlags = 0;
			return;
		}

		auto* creature = GetCreatureInfo(item);

		short angle = 0;
		auto extraHeadRot = EulerAngles::Identity;

		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState == AHMET_STATE_DEATH)
			{
				// Don't clear.
				if (TestLastFrame(*item))
				{
					item->Animation.FrameNumber = GetAnimData(*item).EndFrameNumber - 1;
					item->Collidable = false;
				}
			}
			else
			{
				SetAnimation(*item, AHMET_ANIM_DEATH);
				Lara.Context.InteractedItem = itemNumber; // TODO: Check if it's really required! -- TokyoSU 3/8/2022
			}
			
			TriggerAhmetDeathEffect(item);
		}
		else
		{
			if (item->AIBits != 0) // Does this entity have AI object? NOTE: Previous one checked "& ALL_AIOBJ" -- TokyoSU 3/8/2022
				GetAITarget(creature);

			AI_INFO AI, laraAI;
			CreatureAIInfo(item, &AI);

			if (creature->Enemy->IsLara())
			{
				laraAI.angle = AI.angle;
				laraAI.distance = AI.distance;
			}
			else
			{
				int dx = LaraItem->Pose.Position.x - item->Pose.Position.x; // TODO: Make ahmet to not use LaraItem global -- TokyoSU 3/8/2022
				int dz = LaraItem->Pose.Position.z - item->Pose.Position.z;
				laraAI.angle = short(phd_atan(dx, dz)) - item->Pose.Orientation.y;
				laraAI.distance = SQUARE(dx) + SQUARE(dz);
			}

			GetCreatureMood(item, &AI, true);
			CreatureMood(item, &AI, true);

			angle = CreatureTurn(item, creature->MaxTurn);
			//creature->Enemy = LaraItem; // No need since CreatureAIInfo() set it. -- TokyoSU

			if (laraAI.distance < AHMET_AWARE_RANGE ||
				item->HitStatus || TargetVisible(item, &laraAI))
			{
				AlertAllGuards(itemNumber);
			}

			if (AI.ahead)
				extraHeadRot.y = AI.angle;

			switch (item->Animation.ActiveState)
			{
			case AHMET_STATE_IDLE:
				creature->MaxTurn = 0;
				creature->Flags = 0;

				if (item->AIBits & GUARD)
				{
					item->Animation.TargetState = AHMET_STATE_IDLE;
					extraHeadRot.y = AIGuard(creature);
				}
				else if (item->AIBits & PATROL1)
				{
					item->Animation.TargetState = AHMET_STATE_WALK_FORWARD;
					extraHeadRot.y = 0;
				}
				else if (creature->Mood == MoodType::Bored || creature->Mood == MoodType::Escape)
				{
					if (Lara.TargetEntity == item || !AI.ahead) // TODO: Make ahmet not use LaraInfo global. -- TokyoSU 3/8/2022
						item->Animation.TargetState = AHMET_STATE_RUN_FORWARD;
					else
						item->Animation.TargetState = AHMET_STATE_IDLE;
				}
				else if (AI.bite && AI.distance < AHMET_ATTACK_RANGE)
					item->Animation.TargetState = AHMET_STATE_SWIPE_ATTACK;
				else if ((AI.angle >= AHMET_VIEW_ANGLE || AI.angle <= -AHMET_VIEW_ANGLE) ||
					AI.distance >= AHMET_IDLE_RANGE)
				{
					if (item->Animation.RequiredState != NO_VALUE)
						item->Animation.TargetState = item->Animation.RequiredState;
					else
					{
						if (!AI.ahead || AI.distance >= AHMET_RUN_RANGE)
							item->Animation.TargetState = AHMET_STATE_RUN_FORWARD;
						else
							item->Animation.TargetState = AHMET_STATE_WALK_FORWARD;
					}
				}
				else if (Random::TestProbability(1 / 2.0f))
					item->Animation.TargetState = AHMET_STATE_JUMP_BITE_ATTACK;
				else
					item->Animation.TargetState = AHMET_STATE_JUMP_SWIPE_ATTACK;

				break;

			case AHMET_STATE_WALK_FORWARD:
				creature->MaxTurn = AHMET_WALK_FORWARD_TURN_ANGLE;

				if (item->AIBits & PATROL1)
				{
					item->Animation.TargetState = AHMET_STATE_WALK_FORWARD;
					extraHeadRot.y = 0;
				}
				else if (AI.bite && AI.distance < AHMET_IDLE_RANGE)
					item->Animation.TargetState = AHMET_STATE_IDLE;
				else if (creature->Mood == MoodType::Escape ||
					AI.distance > AHMET_RUN_RANGE || !AI.ahead ||
					(AI.enemyFacing > -AHMET_ENEMY_ANGLE || AI.enemyFacing < AHMET_ENEMY_ANGLE))
				{
					item->Animation.TargetState = AHMET_STATE_RUN_FORWARD;
				}
				
				break;

			case AHMET_STATE_RUN_FORWARD:
				creature->MaxTurn = AHMET_RUN_FORWARD_TURN_ANGLE;
				creature->Flags = 0;

				if (item->AIBits & GUARD ||
					((creature->Mood == MoodType::Bored || creature->Mood == MoodType::Escape) &&
						(Lara.TargetEntity == item && AI.ahead)) ||
					(AI.bite && AI.distance < AHMET_IDLE_RANGE))
				{
					item->Animation.TargetState = AHMET_STATE_IDLE;
				}
				else if (AI.ahead && AI.distance < AHMET_RUN_RANGE &&
					(AI.enemyFacing < -AHMET_ENEMY_ANGLE || AI.enemyFacing > AHMET_ENEMY_ANGLE))
				{
					item->Animation.TargetState = AHMET_STATE_WALK_FORWARD;
				}

				break;

			case AHMET_STATE_SWIPE_ATTACK:
				creature->MaxTurn = 0;

				if (abs(AI.angle) >= ANGLE(5.0f))
				{
					if (AI.angle >= 0)
						item->Pose.Orientation.y += ANGLE(5.0f);
					else
						item->Pose.Orientation.y -= ANGLE(5.0f);
				}
				else
					item->Pose.Orientation.y += AI.angle;

				if (!(creature->Flags & 1) &&
					item->Animation.FrameNumber > 7 &&
					item->TouchBits.Test(AhmetSwipeAttackLeftJoints))
				{
					DoDamage(creature->Enemy, AHMET_SWIPE_ATTACK_DAMAGE);
					CreatureEffect2(item, AhmetBiteLeft, 10, -1, DoBloodSplat);
					creature->Flags |= 1;
				}
				else if (!(creature->Flags & 2) &&
					item->Animation.FrameNumber > 32 &&
					item->TouchBits.Test(AhmetSwipeAttackRightJoints))
				{
					DoDamage(creature->Enemy, AHMET_SWIPE_ATTACK_DAMAGE);
					CreatureEffect2(item, AhmetBiteRight, 10, -1, DoBloodSplat);
					creature->Flags |= 2;
				}

				break;

			case AHMET_STATE_JUMP_BITE_ATTACK:
				creature->MaxTurn = 0;

				if (item->Animation.AnimNumber == AHMET_ANIM_JUMP_SWIPE_ATTACK_START)
				{
					if (abs(AI.angle) >= ANGLE(5.0f))
					{
						if (AI.angle >= 0)
							item->Pose.Orientation.y += ANGLE(5.0f);
						else
							item->Pose.Orientation.y -= ANGLE(5.0f);
					}
					else
						item->Pose.Orientation.y += AI.angle;
				}
				else
				{
					if (!(creature->Flags & 1) &&
						item->Animation.AnimNumber == AHMET_ANIM_JUMP_BITE_ATTACK_CONTINUE)
					{
						if (item->Animation.FrameNumber > 11 &&
							item->TouchBits.Test(AhmetSwipeAttackLeftJoints))
						{
							DoDamage(creature->Enemy, AHMET_BITE_ATTACK_DAMAGE);
							CreatureEffect2(item, AhmetBiteJaw, 10, -1, DoBloodSplat);
							creature->Flags |= 1;
						}
					}
				}

				break;

			case AHMET_STATE_JUMP_SWIPE_ATTACK:
				creature->MaxTurn = 0;

				if (item->Animation.AnimNumber == AHMET_ANIM_JUMP_SWIPE_ATTACK_START)
				{
					if (abs(AI.angle) >= ANGLE(5.0f))
					{
						if (AI.angle >= 0)
							item->Pose.Orientation.y += ANGLE(5.0f);
						else
							item->Pose.Orientation.y -= ANGLE(5.0f);
					}
					else
					{
						item->Pose.Orientation.y += AI.angle;
					}
				}
				else
				{
					if (!(creature->Flags & 1) &&
						item->Animation.FrameNumber > 14 &&
						item->TouchBits.Test(AhmetSwipeAttackLeftJoints))
					{
						DoDamage(creature->Enemy, AHMET_SWIPE_ATTACK_DAMAGE);
						CreatureEffect2(item, AhmetBiteLeft, 10, -1, DoBloodSplat);
						creature->Flags |= 1;
					}
					else if (!(creature->Flags & 2) &&
						item->Animation.FrameNumber > 22 &&
						item->TouchBits.Test(AhmetSwipeAttackRightJoints))
					{
						DoDamage(creature->Enemy, AHMET_SWIPE_ATTACK_DAMAGE);
						CreatureEffect2(item, AhmetBiteRight, 10, -1, DoBloodSplat);
						creature->Flags |= 2;
					}
				}

				break;
			}
		}

		TestTriggers(item, true);
		CreatureTilt(item, 0);
		CreatureJoint(item, 0, extraHeadRot.y);
		CreatureAnimation(itemNumber, angle, 0);
	}

	bool RespawnAhmet(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		if (item->Animation.ActiveState != AHMET_STATE_DEATH || !TestLastFrame(*item))
			return false;

		Weather.Flash(255, 64, 0, 0.03f);

		item->Pose.Position.x = (item->ItemFlags[0] * BLOCK(1)) + CLICK(2);
		item->Pose.Position.y = (item->ItemFlags[1] * CLICK(1));
		item->Pose.Position.z = (item->ItemFlags[2] * BLOCK(1)) + CLICK(2);

		auto outsideRoom = IsRoomOutside(item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z);
		if (item->RoomNumber != outsideRoom)
			ItemNewRoom(itemNumber, outsideRoom);

		SetAnimation(*item, AHMET_ANIM_IDLE);
		item->HitPoints = Objects[item->ObjectNumber].HitPoints;
		AddActiveItem(itemNumber);

		item->Status = ITEM_ACTIVE;
		item->Collidable = true;
		item->AfterDeath = 0;
		item->Flags &= ~IFLAG_INVISIBLE;

		EnableEntityAI(itemNumber, true);
		item->TriggerFlags = 1;
		return true;
	}
}
