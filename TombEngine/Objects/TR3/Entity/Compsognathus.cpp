#include "framework.h"
#include "Objects/TR3/Entity/Compsognathus.h"

#include "Game/collision/collide_room.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/control/lot.h"
#include "Game/effects/effects.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Objects/TR3/Object/Corpse.h"
#include "Specific/level.h"

using namespace TEN::Entities::TR3;
using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR3
{
	constexpr auto COMPY_ATTACK_DAMAGE = 90;

	constexpr auto COMPY_ATTACK_RANGE = SQUARE(BLOCK(0.4f));
	constexpr auto COMPY_ESCAPE_RANGE = SQUARE(BLOCK(5));

	constexpr auto COMPY_JUMP_ATTACK_CHANCE = 1 / 8.0f;
	constexpr auto COMPY_ATTACK_CHANCE		= 1 / 1024.0f;

	constexpr auto COMPY_IDLE_TURN_RATE_MAX	  = ANGLE(3.0f);
	constexpr auto COMPY_RUN_TURN_RATE_MAX	  = ANGLE(10.0f);
	constexpr auto COMPY_ATTACK_TURN_RATE_MAX = ANGLE(67.5f);

	constexpr auto COMPY_PLAYER_ALERT_VELOCITY = 15;
	constexpr auto COMPY_HIT_FLAG = 1;

	const auto CompyBite = CreatureBiteInfo(Vector3::Zero, 2);
	const auto CompyAttackJoints = std::vector<unsigned int>{ 1, 2 };

	enum CompyState
	{
		COMPY_STATE_IDLE = 0,
		COMPY_STATE_RUN_FORWARD = 1,
		COMPY_STATE_JUMP_ATTACK = 2,
		COMPY_STATE_ATTACK = 3,
		COMPY_STATE_DEATH = 4
	};

	enum CompyAnim
	{
		COMPY_ANIM_IDLE = 0,
		COMPY_ANIM_RUN_FORWARD = 1,
		COMPY_ANIM_JUMP_ATTACK_START = 2,
		COMPY_ANIM_JUMP_ATTACK_CONTINUE = 3,
		COMPY_ANIM_JUMP_ATTACK_END = 4,
		COMPY_ANIM_ATTACK = 5,
		COMPY_ANIM_DEATH = 6,
		COMPY_ANIM_IDLE_TO_RUN_FORWARD = 7,
		COMPY_ANIM_RUN_FORWARD_TO_IDLE = 8
	};

	enum CompyTarget
	{
		ATTACK_CADAVER = 0,
		ATTACK_PLAYER = 1
	};

	void InitializeCompsognathus(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		InitializeCreature(itemNumber);

		// Set friendly.
		item.ItemFlags[1] = ATTACK_CADAVER;		
	}

	void CompsognathusControl(short itemNumber)
	{
		constexpr auto INVALID_CADAVER_POSITION = Vector3(FLT_MAX);

		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short headingAngle = 0;
		short tiltAngle = 0;
		short headAngle = 0;
		short neckAngle = 0;

		int random = 0;
		int roomNumber = 0;
		int target = 0;

		auto cadaverPos = INVALID_CADAVER_POSITION;
		
		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState != COMPY_STATE_DEATH)
			{
				item->Animation.TargetState = COMPY_STATE_DEATH;
				SetAnimation(*item, COMPY_ANIM_DEATH);
			}
		}
		else
		{
			AI_INFO ai;
			CreatureAIInfo(item, &ai);

			if (ai.ahead)
				headAngle = ai.angle;

			GetCreatureMood(item, &ai, true);
			CreatureMood(item, &ai, true);

			if (cadaverPos == INVALID_CADAVER_POSITION)
			{
				float shortestDist = INFINITY;
				for (auto& targetItem : g_Level.Items)
				{
					if (!Objects.CheckID(targetItem.ObjectNumber) || targetItem.Index == itemNumber || targetItem.RoomNumber == NO_VALUE)
						continue;

					if (SameZone(creature, &targetItem))
					{
						float dist = Vector3i::Distance(item->Pose.Position, targetItem.Pose.Position);
						if (dist < shortestDist && targetItem.ObjectNumber == ID_CORPSE && targetItem.Active &&
							TriggerActive(&targetItem) && targetItem.ItemFlags[1] == (int)CorpseFlag::Grounded)
						{
							cadaverPos = targetItem.Pose.Position.ToVector3();
							shortestDist = dist;
							item->ItemFlags[1] = ATTACK_CADAVER;
						}
					}
				}
			}

			creature->Enemy = LaraItem;
			target = item->ItemFlags[1];

			if (creature->HurtByLara)
				AlertAllGuards(itemNumber);

			if (creature->Mood == MoodType::Bored && item->ItemFlags[1] == ATTACK_CADAVER)
			{
				int dx = cadaverPos.x - item->Pose.Position.x;
				int dz = cadaverPos.z - item->Pose.Position.z;
				ai.distance = SQUARE(dx) + SQUARE(dz);
				ai.angle = phd_atan(dz, dx) - item->Pose.Orientation.y;
				ai.ahead = (ai.angle > -FRONT_ARC && ai.angle < FRONT_ARC);
			}

			random = ((itemNumber & 0x7) * 0x200) - 0x700;
		
			if (!item->ItemFlags[3] &&
				!item->ItemFlags[1] == ATTACK_PLAYER &&
				((ai.enemyFacing < COMPY_ATTACK_TURN_RATE_MAX && ai.enemyFacing > -COMPY_ATTACK_TURN_RATE_MAX && LaraItem->Animation.Velocity.z > COMPY_PLAYER_ALERT_VELOCITY) ||
				LaraItem->Animation.ActiveState == LS_ROLL_180_FORWARD ||
				item->HitStatus))
			{
				if (ai.distance > (COMPY_ATTACK_RANGE * 4))
				{
					item->ItemFlags[0] = (random + 0x700) >> 7;

					// Spooked for less time the more compys there are. Adjust when there are many of them.
					item->ItemFlags[3] = LaraItem->Animation.Velocity.z * 2;
				}
			}
			else if (item->ItemFlags[3])
			{
				if (item->ItemFlags[0] > 0)
				{
					item->ItemFlags[0]--;
				}
				else
				{
					creature->Mood = MoodType::Escape;		
					item->ItemFlags[3]--;
				}

				if (Random::TestProbability(COMPY_ATTACK_CHANCE) && item->Timer > 180)
					item->ItemFlags[1] = ATTACK_PLAYER;
			}
			else if (ai.zoneNumber != ai.enemyZone)
			{
				creature->Mood = MoodType::Bored;						
			}
			else
			{
				creature->Mood = MoodType::Attack;
			}

			switch (creature->Mood)
			{
			case MoodType::Attack:
				creature->Target.x = creature->Enemy->Pose.Position.x + (BLOCK(1) * phd_sin(item->Pose.Orientation.y + random));
				creature->Target.y = creature->Enemy->Pose.Position.y;
				creature->Target.z = creature->Enemy->Pose.Position.z + (BLOCK(1) * phd_cos(item->Pose.Orientation.y + random));
				break;

			// Turn and run.
			case MoodType::Stalk:
			case MoodType::Escape:
				creature->Target.x = item->Pose.Position.x + (BLOCK(1) * phd_sin(ai.angle + ANGLE(180.0f) + random));
				creature->Target.z = item->Pose.Position.z + (BLOCK(1) * phd_cos(ai.angle + ANGLE(180.0f) + random));
				roomNumber = item->RoomNumber;

				if (ai.distance > COMPY_ESCAPE_RANGE || !item->ItemFlags[3])
				{
					creature->Mood = MoodType::Bored;
					item->ItemFlags[0] = item->ItemFlags[3];
				}

				break;

			case MoodType::Bored:
				if (cadaverPos != INVALID_CADAVER_POSITION && item->ItemFlags[1] == ATTACK_CADAVER)
				{
					creature->Target.x = cadaverPos.x;
					creature->Target.z = cadaverPos.z;
				}
				else
				{
					cadaverPos = INVALID_CADAVER_POSITION;
				}

				break;
			}

			if (item->AIBits)
				GetAITarget(creature);

			headingAngle = CreatureTurn(item, creature->MaxTurn);

			if (ai.ahead)
				headAngle = ai.angle;

			neckAngle = -(ai.angle / 4);

			if (item->Timer < 250)
				item->Timer++;

			if ((item->HitStatus && item->Timer > 200 && Random::TestProbability(COMPY_ATTACK_CHANCE * 100)) ||
				(creature->Alerted && ai.zoneNumber == ai.enemyZone))
			{
				item->ItemFlags[1] = ATTACK_PLAYER;
			}

			switch (item->Animation.ActiveState)
			{
			case COMPY_STATE_IDLE:
				creature->MaxTurn = COMPY_IDLE_TURN_RATE_MAX;
				creature->Flags &= ~COMPY_HIT_FLAG;

				if (creature->Mood == MoodType::Attack)
				{					
					if (ai.ahead && ai.distance < (COMPY_ATTACK_RANGE * 4))
					{
						if (item->ItemFlags[1] == ATTACK_PLAYER)
						{
							if (Random::TestProbability(1 / 2.0f))
							{
								item->Animation.TargetState = COMPY_STATE_ATTACK;
							}
							else
							{
								item->Animation.TargetState = COMPY_STATE_JUMP_ATTACK;
							}
						}
						else
						{
							item->Animation.TargetState = COMPY_STATE_IDLE;
						}
					}
					else if (ai.distance > (COMPY_ATTACK_RANGE * (9 - (target * 4))))
					{
						item->Animation.TargetState = COMPY_STATE_RUN_FORWARD;
					}
				}
				else if (creature->Mood == MoodType::Bored)
				{
					if (ai.ahead && ai.distance < (COMPY_ATTACK_RANGE * 3) && item->ItemFlags[1] == ATTACK_CADAVER)
					{
						if (Random::TestProbability(1 / 2.0f))
						{
							item->Animation.TargetState = COMPY_STATE_ATTACK;
						}
						else
						{
							item->Animation.TargetState = COMPY_STATE_JUMP_ATTACK;
						}
					}
					else if (ai.distance > (COMPY_ATTACK_RANGE * 3))
					{
						item->Animation.TargetState = COMPY_STATE_RUN_FORWARD;
					}
				}
				else
				{
					if (Random::TestProbability(COMPY_JUMP_ATTACK_CHANCE))
					{
						item->Animation.TargetState = COMPY_STATE_JUMP_ATTACK;
					}
					else
					{
						item->Animation.TargetState = COMPY_STATE_RUN_FORWARD;
					}
				}

				break;

			case COMPY_STATE_RUN_FORWARD:
				creature->MaxTurn = COMPY_RUN_TURN_RATE_MAX;
				creature->Flags &= ~COMPY_HIT_FLAG;

				if (ai.angle < COMPY_ATTACK_TURN_RATE_MAX && ai.angle > -COMPY_ATTACK_TURN_RATE_MAX &&
					ai.distance < (COMPY_ATTACK_RANGE * (9 - (target * 4))))
				{
					item->Animation.TargetState = COMPY_STATE_IDLE;
				}

				break;

			case COMPY_STATE_ATTACK:
			case COMPY_STATE_JUMP_ATTACK:
				creature->MaxTurn = COMPY_RUN_TURN_RATE_MAX;

				if (!(creature->Flags & COMPY_HIT_FLAG))
				{
					switch (item->ItemFlags[1])
					{
					case ATTACK_PLAYER:
						if (item->TouchBits.Test(CompyAttackJoints))
						{
							creature->Flags |= COMPY_HIT_FLAG;

							DoDamage(creature->Enemy, COMPY_ATTACK_DAMAGE);
							CreatureEffect(item, CompyBite, DoBloodSplat);
						}

						break;

					case ATTACK_CADAVER:
						if (ai.distance < COMPY_ATTACK_RANGE && ai.ahead)
						{
							creature->Flags |= COMPY_HIT_FLAG;
							CreatureEffect(item, CompyBite, DoBloodSplat);
						}

						break;
					}
				}

				break;
			}
		}

		CreatureTilt(item, headingAngle >> 1);
		CreatureJoint(item, 0, headAngle);
		CreatureJoint(item, 1, neckAngle);
		CreatureAnimation(itemNumber, headingAngle, tiltAngle);
	}
}
