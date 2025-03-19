#include "framework.h"
#include "Objects/TR2/Entity/tr2_knife_thrower.h"

#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/floordata.h"
#include "Game/control/box.h"
#include "Game/effects/effects.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/missile.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR2
{
	constexpr auto KNIFE_PROJECTILE_DAMAGE = 50;

	// TODO: Ranges.

	const auto KnifeBiteLeft  = CreatureBiteInfo(Vector3::Zero, 5);
	const auto KnifeBiteRight = CreatureBiteInfo(Vector3::Zero, 8);

	enum KnifeThrowerState
	{
		// No state 0.
		KTHROWER_STATE_IDLE = 1,
		KTHROWER_STATE_WALK_FORWARD = 2,
		KTHROWER_STATE_RUN_FORWARD = 3,
		KTHROWER_STATE_WALK_KNIFE_ATTACK_LEFT_START = 4,
		KTHROWER_STATE_WALK_KNIFE_ATTACK_LEFT_CONTINUE = 5,
		KTHROWER_STATE_WALK_KNIFE_ATTACK_RIGHT_START = 6,
		KTHROWER_STATE_WALK_KNIFE_ATTACK_RIGHT_CONTINUE = 7,
		KTHROWER_STATE_IDLE_KNIFE_ATTACK_START = 8,
		KTHROWER_STATE_IDLE_KNIFE_ATTACK_CONTINUE = 9,
		KTHROWER_STATE_DEATH = 10
	};

	enum KnifeThrowerAnim
	{
		KTHROWER_ANIM_IDLE = 0,
		KTHROWER_ANIM_WALK_KNIFE_ATTACK_LEFT_START = 1,
		KTHROWER_ANIM_WALK_KNIFE_ATTACK_LEFT_CANCEL = 2,
		KTHROWER_ANIM_WALK_KNIFE_ATTACK_RIGHT_START = 3,
		KTHROWER_ANIM_WALK_KNIFE_ATTACK_RIGHT_CANCEL = 4,
		KTHROWER_ANIM_IDLE_KNIFE_ATTACK_START = 5,
		KTHROWER_ANIM_RUN_FORWARD_TO_IDLE = 6,
		KTHROWER_ANIM_RUN_FORWARD_TO_WALK_FORWARD = 7,
		KTHROWER_ANIM_RUN_FORWARD = 8,
		KTHROWER_ANIM_WALK_KNIFE_ATTACK_LEFT_CONTINUE = 9,
		KTHROWER_ANIM_WALK_KNIFE_ATTACK_RIGHT_CONTINUE = 10,
		KTHROWER_ANIM_IDLE_KNIFE_ATTACK_CONTINUE = 11,
		KTHROWER_ANIM_WALK_KNIFE_ATTACK_LEFT_END = 12,
		KTHROWER_ANIM_IDLE_KNIFE_ATTACK_END_TO_IDLE = 13,
		KTHROWER_ANIM_IDLE_KNIFE_ATTACK_END_TO_START = 14,
		KTHROWER_ANIM_IDLE_TO_RUN_FORWARD = 15,
		KTHROWER_ANIM_IDLE_TO_WALK_FORWARD = 16,
		KTHROWER_ANIM_WALK_FORWARD = 17,
		KTHROWER_ANIM_WALK_FORWARD_TO_RUN_FORWARD = 18,
		KTHROWER_ANIM_WALK_FORWARD_TO_IDLE_START = 19,
		KTHROWER_ANIM_WALK_FORWARD_TO_IDLE_END = 20,
		KTHROWER_ANIM_WALK_KNIFE_ATTACK_RIGHT_END = 21,
		KTHROWER_ANIM_IDLE_KNIFE_ATTACK_CANCEL = 22,
		KTHROWER_ANIM_DEATH = 23
	};

	short ThrowKnife(int x, int y, int z, float vel, short yRot, int roomNumber)
	{
		int fxNumber = CreateNewEffect(roomNumber);
		if (fxNumber == NO_VALUE)
			return fxNumber;

		auto& fx = EffectList[fxNumber];

		fx.objectNumber = ID_KNIFETHROWER_KNIFE;
		fx.pos.Position.x = x;
		fx.pos.Position.y = y;
		fx.pos.Position.z = z;
		fx.speed = vel;
		fx.pos.Orientation.y = yRot;
		fx.fallspeed = 0;
		fx.flag2 = KNIFE_PROJECTILE_DAMAGE;
		fx.color = Vector4::One;
		ShootAtLara(fx);

		return fxNumber;
	}

	void KnifeThrowerControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short headingAngle = 0;
		short tiltAngle = 0;
		short extraTorsoRot = 0;
		short extraHeadRot = 0;

		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState != KTHROWER_STATE_DEATH)
				SetAnimation(*item, KTHROWER_ANIM_DEATH);
		}
		else
		{
			AI_INFO ai;
			CreatureAIInfo(item, &ai);

			GetCreatureMood(item, &ai, true);
			CreatureMood(item, &ai, true);

			headingAngle = CreatureTurn(item, creature->MaxTurn);

			switch (item->Animation.ActiveState)
			{
			case KTHROWER_STATE_IDLE:
				creature->MaxTurn = 0;

				if (ai.ahead)
					extraHeadRot = ai.angle;

				if (creature->Mood == MoodType::Escape)
				{
					item->Animation.TargetState = KTHROWER_STATE_RUN_FORWARD;
				}
				else if (Targetable(item, &ai))
				{
					item->Animation.TargetState = KTHROWER_STATE_IDLE_KNIFE_ATTACK_START;
				}
				else if (creature->Mood == MoodType::Bored)
				{
					if (!ai.ahead || ai.distance > pow(BLOCK(6), 2))
						item->Animation.TargetState = KTHROWER_STATE_WALK_FORWARD;
				}
				else if (ai.ahead && ai.distance < pow(BLOCK(4), 2))
				{
					item->Animation.TargetState = KTHROWER_STATE_WALK_FORWARD;
				}
				else
				{
					item->Animation.TargetState = KTHROWER_STATE_RUN_FORWARD;
				}

				break;

			case KTHROWER_STATE_WALK_FORWARD:
				creature->MaxTurn = ANGLE(3.0f);

				if (ai.ahead)
					extraHeadRot = ai.angle;

				if (creature->Mood == MoodType::Escape)
				{
					item->Animation.TargetState = KTHROWER_STATE_RUN_FORWARD;
				}
				else if (Targetable(item, &ai))
				{
					if (ai.distance < pow(BLOCK(2.5f), 2) || ai.zoneNumber != ai.enemyZone)
					{
						item->Animation.TargetState = KTHROWER_STATE_IDLE;
					}
					else if (Random::TestProbability(1 / 2.0f))
					{
						item->Animation.TargetState = KTHROWER_STATE_WALK_KNIFE_ATTACK_LEFT_START;
					}
					else
					{
						item->Animation.TargetState = KTHROWER_STATE_WALK_KNIFE_ATTACK_RIGHT_START;
					}
				}
				else if (creature->Mood == MoodType::Bored)
				{
					if (ai.ahead && ai.distance < pow(BLOCK(6), 2))
						item->Animation.TargetState = KTHROWER_STATE_IDLE;
				}
				else if (!ai.ahead || ai.distance > pow(BLOCK(4), 2))
				{
					item->Animation.TargetState = KTHROWER_STATE_RUN_FORWARD;
				}

				break;

			case KTHROWER_STATE_RUN_FORWARD:
				creature->MaxTurn = ANGLE(6.0f);
				tiltAngle = headingAngle / 3;

				if (ai.ahead)
					extraHeadRot = ai.angle;

				if (Targetable(item, &ai))
				{
					item->Animation.TargetState = KTHROWER_STATE_WALK_FORWARD;
				}
				else if (creature->Mood == MoodType::Bored)
				{
					if (ai.ahead && ai.distance < pow(BLOCK(6), 2))
					{
						item->Animation.TargetState = KTHROWER_STATE_IDLE;
					}
					else
					{
						item->Animation.TargetState = KTHROWER_STATE_WALK_FORWARD;
					}
				}
				else if (ai.ahead && ai.distance < pow(BLOCK(4), 2))
				{
					item->Animation.TargetState = KTHROWER_STATE_WALK_FORWARD;
				}

				break;

			case KTHROWER_STATE_WALK_KNIFE_ATTACK_LEFT_START:
				creature->Flags = 0;

				if (ai.ahead)
					extraTorsoRot = ai.angle;

				if (Targetable(item, &ai))
				{
					item->Animation.TargetState = KTHROWER_STATE_WALK_KNIFE_ATTACK_LEFT_CONTINUE;
				}
				else
				{
					item->Animation.TargetState = KTHROWER_STATE_WALK_FORWARD;
				}

				break;

			case KTHROWER_STATE_WALK_KNIFE_ATTACK_RIGHT_START:
				creature->Flags = 0;

				if (ai.ahead)
					extraTorsoRot = ai.angle;

				if (Targetable(item, &ai))
				{
					item->Animation.TargetState = KTHROWER_STATE_WALK_KNIFE_ATTACK_RIGHT_CONTINUE;
				}
				else
				{
					item->Animation.TargetState = KTHROWER_STATE_WALK_FORWARD;
				}

				break;

			case KTHROWER_STATE_IDLE_KNIFE_ATTACK_START:
				creature->Flags = 0;

				if (ai.ahead)
					extraTorsoRot = ai.angle;

				if (Targetable(item, &ai))
				{
					item->Animation.TargetState = KTHROWER_STATE_IDLE_KNIFE_ATTACK_CONTINUE;
				}
				else
				{
					item->Animation.TargetState = KTHROWER_STATE_IDLE;
				}

				break;

			case KTHROWER_STATE_WALK_KNIFE_ATTACK_LEFT_CONTINUE:
				if (ai.ahead)
					extraTorsoRot = ai.angle;

				if (!creature->Flags)
				{
					CreatureEffect2(item, KnifeBiteLeft, 100, extraTorsoRot, ThrowKnife);
					creature->Flags = 1;
				}

				break;

			case KTHROWER_STATE_WALK_KNIFE_ATTACK_RIGHT_CONTINUE:
				if (ai.ahead)
					extraTorsoRot = ai.angle;

				if (!creature->Flags)
				{
					CreatureEffect2(item, KnifeBiteRight, 100, extraTorsoRot, ThrowKnife);
					creature->Flags = 1;
				}

				break;

			case KTHROWER_STATE_IDLE_KNIFE_ATTACK_CONTINUE:
				if (ai.ahead)
					extraTorsoRot = ai.angle;

				if (!creature->Flags)
				{
					CreatureEffect2(item, KnifeBiteLeft, 100, extraTorsoRot, ThrowKnife);
					CreatureEffect2(item, KnifeBiteRight, 100, extraTorsoRot, ThrowKnife);
					creature->Flags = 1;
				}

				break;
			}
		}

		CreatureTilt(item, tiltAngle);
		CreatureJoint(item, 0, extraTorsoRot);
		CreatureJoint(item, 1, extraHeadRot);
		CreatureAnimation(itemNumber, headingAngle, tiltAngle);
	}
}
