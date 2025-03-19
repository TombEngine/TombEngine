#include "framework.h"
#include "Objects/TR4/Entity/tr4_mummy.h"

#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Specific/level.h"

using namespace TEN::Math;

namespace TEN::Entities::TR4
{
	constexpr auto MUMMY_SWIPE_ATTACK_DAMAGE = 100;

	constexpr auto MUMMY_IDLE_SWIPE_ATTACK_RANGE = SQUARE(BLOCK(1 / 2.0f));
	constexpr auto MUMMY_WALK_SWIPE_ATTACK_RANGE = SQUARE(BLOCK(2 / 3.0f));
	constexpr auto MUMMY_ACTIVATE_RANGE			 = SQUARE(BLOCK(1));
	constexpr auto MUMMY_RECOIL_RANGE			 = SQUARE(BLOCK(3));
	constexpr auto MUMMY_ARMS_UP_RANGE			 = SQUARE(BLOCK(3));
	constexpr auto MUMMY_AWARE_RANGE			 = SQUARE(BLOCK(7));

	constexpr auto MUMMY_WALK_TURN_RATE_MAX	  = ANGLE(7.0f);
	constexpr auto MUMMY_ATTACK_TURN_RATE_MAX = ANGLE(7.0f);

	const auto MummyBite1 = CreatureBiteInfo(Vector3::Zero, 11);
	const auto MummyBite2 = CreatureBiteInfo(Vector3::Zero, 14);
	const auto MummySwipeAttackJoints = std::vector<unsigned int>{ 11, 14 };

	enum MummyState
	{
		MUMMY_STATE_INACTIVE_ARMS_CROSSED = 0,
		MUMMY_STATE_IDLE = 1,
		MUMMY_STATE_WALK_FORWARD = 2,
		MUMMY_STATE_WALK_FORWARD_ARMS_UP = 3,
		MUMMY_STATE_WALK_FORWARD_SWIPE_ATTACK = 4,
		MUMMY_STATE_RECOIL = 5,
		MUMMY_STATE_ARMS_UP_RECOIL = 6,
		MUMMY_STATE_COLLAPSE = 7,
		MUMMY_STATE_INACTIVE_LYING_DOWN = 8,
		MUMMY_STATE_COLLAPSED_TO_IDLE = 9,
		MUMMY_STATE_IDLE_SWIPE_ATTACK = 10
	};

	enum MummyAnim
	{
		MUMMY_ANIM_STAND = 0,
		MUMMY_ANIM_WALK_FORWARD = 1,
		MUMMY_ANIM_WALK_FORWARD_ARMS_UP = 2,
		MUMMY_ANIM_RECOIL = 3,
		MUMMY_ANIM_WALK_FORWARD_TO_WALK_FORWARD_ARMS_UP_RIGHT = 4,
		MUMMY_ANIM_WALK_FORWARD_ARMS_UP_TO_WALK_FORWARD_LEFT = 5,
		MUMMY_ANIM_WALK_FORWARD_ARMS_UP_TO_IDLE = 6,
		MUMMY_ANIM_IDLE_TO_WALK_FORWARD_ARMS_UP = 7,
		MUMMY_ANIM_IDLE_TO_WALK_FORWARD = 8,
		MUMMY_ANIM_WALK_FORWARD_TO_IDLE = 9,
		MUMMY_ANIM_COLLAPSE_START = 10,
		MUMMY_ANIM_COLLAPSE_CONTINUE = 11,
		MUMMY_ANIM_COLLAPSE_END = 12,
		MUMMY_ANIM_COLLAPSED_TO_IDLE = 13,
		MUMMY_ANIM_IDLE_SWIPE_ATTACK_RIGHT = 14,
		MUMMY_ANIM_IDLE_SWIPE_ATTACK_LEFT = 15,
		MUMMY_ANIM_WALK_FORWARD_SWIPE_ATTACK = 16,
		MUMMY_ANIM_ARMS_CROSSED_TO_WALK_FORWARD_START = 17,
		MUMMY_ANIM_ARMS_CROSSED_TO_WALK_FORWARD_END = 18,
		MUMMY_ANIM_ARMS_CROSSED = 19,
		MUMMY_ANIM_ARMS_UP_RECOIL = 20
	};

	void InitializeMummy(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);

		if (item->TriggerFlags == 2)
		{
			SetAnimation(*item, MUMMY_ANIM_COLLAPSE_END);
			item->Status = ITEM_NOT_ACTIVE;
		}
		else
		{
			SetAnimation(*item, MUMMY_ANIM_ARMS_CROSSED);
		}
	}

	void MummyControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short headingAngle = 0;
		short joint0 = 0;
		short joint1 = 0;
		short joint2 = 0;

		if (item->AIBits)
			GetAITarget(creature);
		else if (creature->HurtByLara)
			creature->Enemy = LaraItem;

		AI_INFO AI;
		CreatureAIInfo(item, &AI);

		if (item->HitStatus)
		{
			if (AI.distance < MUMMY_RECOIL_RANGE)
			{
				if (item->Animation.ActiveState != MUMMY_ANIM_IDLE_TO_WALK_FORWARD_ARMS_UP &&
					item->Animation.ActiveState != MUMMY_ANIM_WALK_FORWARD_ARMS_UP_TO_WALK_FORWARD_LEFT &&
					item->Animation.ActiveState != MUMMY_ANIM_IDLE_TO_WALK_FORWARD)
				{
					if (Random::TestProbability(0.75f) ||
						Lara.Control.Weapon.GunType != LaraWeaponType::Shotgun &&
						Lara.Control.Weapon.GunType != LaraWeaponType::HK &&
						Lara.Control.Weapon.GunType != LaraWeaponType::Revolver)
					{
						if (Random::TestProbability(1 / 8.0f) ||
							Lara.Control.Weapon.GunType == LaraWeaponType::Shotgun ||
							Lara.Control.Weapon.GunType == LaraWeaponType::HK ||
							Lara.Control.Weapon.GunType == LaraWeaponType::Revolver)
						{
							if (item->Animation.ActiveState == MUMMY_STATE_WALK_FORWARD_ARMS_UP ||
								item->Animation.ActiveState == MUMMY_STATE_WALK_FORWARD_SWIPE_ATTACK)
							{
								item->Animation.ActiveState = MUMMY_STATE_ARMS_UP_RECOIL;
								item->Animation.AnimNumber = MUMMY_ANIM_ARMS_UP_RECOIL;
							}
							else
							{
								item->Animation.ActiveState = MUMMY_STATE_RECOIL;
								item->Animation.AnimNumber = MUMMY_ANIM_RECOIL;
							}

							item->Animation.FrameNumber = 0;
							item->Pose.Orientation.y += AI.angle;
						}
					}
					else
					{
						SetAnimation(*item, MUMMY_ANIM_COLLAPSE_START);
						item->Pose.Orientation.y += AI.angle;
						creature->MaxTurn = 0;
					}
				}
			}
		}
		else
		{
			GetCreatureMood(item, &AI, true);
			CreatureMood(item, &AI, true);

			headingAngle = CreatureTurn(item, creature->MaxTurn);

			if (AI.ahead)
			{
				joint0 = AI.angle / 2;
				joint1 = AI.angle / 2;
				joint2 = AI.xAngle;
			}

			switch (item->Animation.ActiveState)
			{
			case MUMMY_STATE_IDLE:
				creature->MaxTurn = 0;
				creature->Flags = 0;

				if (AI.distance <= MUMMY_IDLE_SWIPE_ATTACK_RANGE ||
					AI.distance >= MUMMY_AWARE_RANGE)
				{
					if (AI.distance - MUMMY_IDLE_SWIPE_ATTACK_RANGE <= 0)
						item->Animation.TargetState = MUMMY_STATE_IDLE_SWIPE_ATTACK;
					else
					{
						item->Animation.TargetState = MUMMY_STATE_IDLE;
						joint0 = 0;
						joint1 = 0;
						joint2 = 0;

						if (item->TriggerFlags > -100 && item->TriggerFlags < 0)
							item->TriggerFlags++;
					}
				}
				else
					item->Animation.TargetState = MUMMY_STATE_WALK_FORWARD;

				break;

			case MUMMY_STATE_WALK_FORWARD:
				if (item->TriggerFlags == 1)
				{
					creature->MaxTurn = 0;

					if (TestLastFrame(*item))
						item->TriggerFlags = 0;
				}
				else
				{
					creature->MaxTurn = MUMMY_WALK_TURN_RATE_MAX;

					if (AI.distance >= MUMMY_ARMS_UP_RANGE)
					{
						if (AI.distance > MUMMY_AWARE_RANGE)
							item->Animation.TargetState = MUMMY_STATE_IDLE;
					}
					else
						item->Animation.TargetState = MUMMY_STATE_WALK_FORWARD_ARMS_UP;
				}

				break;

			case MUMMY_STATE_WALK_FORWARD_ARMS_UP:
				creature->MaxTurn = MUMMY_WALK_TURN_RATE_MAX;
				creature->Flags = 0;

				if (AI.distance < MUMMY_IDLE_SWIPE_ATTACK_RANGE)
				{
					item->Animation.TargetState = MUMMY_STATE_IDLE;
					break;
				}

				if (AI.distance > MUMMY_ARMS_UP_RANGE && AI.distance < MUMMY_AWARE_RANGE)
				{
					item->Animation.TargetState = MUMMY_STATE_WALK_FORWARD;
					break;
				}

				if (AI.distance <= MUMMY_WALK_SWIPE_ATTACK_RANGE)
					item->Animation.TargetState = MUMMY_STATE_WALK_FORWARD_SWIPE_ATTACK;
				else if (AI.distance > MUMMY_AWARE_RANGE)
					item->Animation.TargetState = MUMMY_STATE_IDLE;

				break;

			case MUMMY_STATE_INACTIVE_ARMS_CROSSED:
				creature->MaxTurn = 0;

				if (AI.distance < MUMMY_ACTIVATE_RANGE || item->TriggerFlags > -1)
					item->Animation.TargetState = MUMMY_STATE_WALK_FORWARD;

				break;

			case MUMMY_STATE_INACTIVE_LYING_DOWN:
				item->HitPoints = 0;
				creature->MaxTurn = 0;
				joint0 = 0;
				joint1 = 0;
				joint2 = 0;

				if (AI.distance < MUMMY_ACTIVATE_RANGE || Random::TestProbability(1 / 128.0f))
				{
					item->Animation.TargetState = MUMMY_STATE_COLLAPSED_TO_IDLE;
					item->HitPoints = Objects[item->ObjectNumber].HitPoints;
				}

				break;

			case MUMMY_STATE_WALK_FORWARD_SWIPE_ATTACK:
			case MUMMY_STATE_IDLE_SWIPE_ATTACK:
				creature->MaxTurn = 0;

				if (abs(AI.angle) >= MUMMY_ATTACK_TURN_RATE_MAX)
				{
					if (AI.angle >= 0)
						item->Pose.Orientation.y += MUMMY_ATTACK_TURN_RATE_MAX;
					else
						item->Pose.Orientation.y -= MUMMY_ATTACK_TURN_RATE_MAX;
				}
				else
					item->Pose.Orientation.y += AI.angle;

				if (!creature->Flags)
				{
					if (item->TouchBits.Test(MummySwipeAttackJoints))
					{
						if (item->Animation.FrameNumber > 0 && !TestLastFrame(*item))
						{
							DoDamage(creature->Enemy, MUMMY_SWIPE_ATTACK_DAMAGE);

							if (item->Animation.AnimNumber == MUMMY_ANIM_IDLE_SWIPE_ATTACK_LEFT)
								CreatureEffect2(item, MummyBite1, 5, -1, DoBloodSplat);
							else
								CreatureEffect2(item, MummyBite2, 5, -1, DoBloodSplat);

							creature->Flags = 1;
						}
					}
				}

				break;

			default:
				break;
			}
		}

		CreatureTilt(item, 0);
		CreatureJoint(item, 0, joint0);
		CreatureJoint(item, 1, joint1);
		CreatureJoint(item, 2, joint2);
		CreatureAnimation(itemNumber, headingAngle, 0);
	}
}
