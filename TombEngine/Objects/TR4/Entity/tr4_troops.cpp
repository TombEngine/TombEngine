#include "framework.h"
#include "Objects/TR4/Entity/tr4_troops.h"

#include "Game/Animation/Animation.h"
#include "Game/control/box.h"
#include "Game/control/lot.h"
#include "Game/control/control.h"
#include "Game/items.h"
#include "Game/effects/effects.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_fire.h"
#include "Game/people.h"
#include "Game/itemdata/creature_info.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Math;

namespace TEN::Entities::TR4
{
	const auto TroopsBite1 = CreatureBiteInfo(Vector3(0, 270, 40), 7);

	enum TroopState
	{
		// No state 0.
		TROOP_STATE_IDLE = 1,
		TROOP_STATE_WALK = 2,
		TROOP_STATE_RUN = 3,
		TROOP_STATE_GUARD = 4,
		TROOP_STATE_ATTACK_1 = 5,
		TROOP_STATE_ATTACK_2 = 6,
		TROOP_STATE_DEATH = 7,
		TROOP_STATE_AIM_1 = 8,
		TROOP_STATE_AIM_2 = 9,
		TROOP_STATE_AIM_3 = 10,
		TROOP_STATE_ATTACK_3 = 11,
		TROOP_STATE_KILLED_BY_SCORPION = 15,
		TROOP_STATE_ATTACKED_BY_SCORPION = 16,
		TROOP_STATE_FLASHED = 17
	};

	// TODO
	enum TroopAnim
	{

	};

	void InitializeTroops(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);

		if (item->TriggerFlags == 1)
		{
			item->Animation.TargetState = item->Animation.ActiveState = TROOP_STATE_ATTACKED_BY_SCORPION;
			item->Animation.AnimNumber = 27;
		}
		else
		{
			item->Animation.TargetState = item->Animation.ActiveState = TROOP_STATE_IDLE;
			item->Animation.AnimNumber = 12;
		}

		item->Animation.FrameNumber = 0;
	}

	void TroopsControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* object = &Objects[item->ObjectNumber];
		auto* creature = GetCreatureInfo(item);

		short angle = 0;
		short tilt = 0;
		short rot = 0;
		short joint0 = 0;
		short joint1 = 0;
		short joint2 = 0;

		int dx = 0;
		int dy = 0;
		int dz = 0;

		if (creature->MuzzleFlash[0].Delay != 0)
			creature->MuzzleFlash[0].Delay--;

		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState != TROOP_STATE_DEATH &&
				item->Animation.ActiveState != TROOP_STATE_KILLED_BY_SCORPION)
			{
				if (creature->Enemy &&
					creature->Enemy->ObjectNumber == ID_BIG_SCORPION &&
					item->ItemFlags[0] < 80)
				{
					if (creature->Enemy->Animation.AnimNumber == 6)
					{
						item->Animation.AnimNumber = 23;

						if (item->Animation.ActiveState == TROOP_STATE_ATTACKED_BY_SCORPION)
							item->Animation.FrameNumber = 37;
						else
							item->Animation.FrameNumber = 0;

						item->Animation.ActiveState = TROOP_STATE_KILLED_BY_SCORPION;
						item->Animation.TargetState = TROOP_STATE_KILLED_BY_SCORPION;

						angle = 0;

						item->Pose.Position = creature->Enemy->Pose.Position;
						item->Pose.Orientation = creature->Enemy->Pose.Orientation;

						creature->Enemy->TriggerFlags = 99;
					}
					else
					{
						item->ItemFlags[0]++;
						angle = 0;
					}
				}
				else
				{
					item->Animation.AnimNumber = 19;
					item->Animation.ActiveState = TROOP_STATE_DEATH;
					item->Animation.FrameNumber = 0;
				}
			}
		}
		else
		{
			if (item->AIBits)
				GetAITarget(creature);
			else
			{
				// Search for active troops.
				creature->Enemy = nullptr;

				float minDistance = FLT_MAX;

				for (auto& currentCreature : ActiveCreatures)
				{
					if (currentCreature->ItemNumber != NO_VALUE && currentCreature->ItemNumber != itemNumber)
					{
						auto* currentItem = &g_Level.Items[currentCreature->ItemNumber];
						if (currentItem->ObjectNumber != ID_LARA)
						{
							if (currentItem->ObjectNumber != ID_TROOPS &&
								(!currentItem->IsLara() || creature->HurtByLara))
							{
								float distance = Vector3i::Distance(item->Pose.Position, currentItem->Pose.Position);
								if (distance < minDistance)
								{
									minDistance = distance;
									creature->Enemy = currentItem;
								}
							}
						}
					}
				}
			}

			if (creature->HurtByLara && item->Animation.ActiveState != TROOP_STATE_ATTACKED_BY_SCORPION)
				creature->Enemy = LaraItem;

			AI_INFO AI;
			CreatureAIInfo(item, &AI);

			int distance = 0;
			if (creature->Enemy->IsLara())
			{
				distance = AI.distance;
				rot = AI.angle;
			}
			else
			{
				dx = LaraItem->Pose.Position.x - item->Pose.Position.x;
				dz = LaraItem->Pose.Position.z - item->Pose.Position.z;
				distance = pow(dx, 2) + pow(dz, 2);
				rot = phd_atan(dz, dx) - item->Pose.Orientation.y;
			}

			if (!creature->HurtByLara && creature->Enemy->IsLara())
				creature->Enemy = nullptr;

			GetCreatureMood(item, &AI, false);
			CreatureMood(item, &AI, false);

			// Vehicle handling
			if (Lara.Context.Vehicle != NO_VALUE && AI.bite)
				creature->Mood = MoodType::Escape;

			angle = CreatureTurn(item, creature->MaxTurn);

			if (item->HitStatus)
				AlertAllGuards(itemNumber);

			switch (item->Animation.ActiveState)
			{
			case TROOP_STATE_IDLE:
				creature->MaxTurn = 0;
				creature->Flags = 0;
				joint2 = rot;

				if (item->Animation.AnimNumber == 17)
				{
					if (abs(AI.angle) >= ANGLE(10.0f))
					{
						if (AI.angle >= 0)
							item->Pose.Orientation.y += ANGLE(10.0f);
						else
							item->Pose.Orientation.y -= ANGLE(10.0f);
					}
					else
						item->Pose.Orientation.y += AI.angle;
				}

				if (item->AIBits & GUARD)
				{
					joint2 = AIGuard(creature);

					// TODO: Use Random::TestProbability().
					if (!GetRandomControl())
					{
						if (item->Animation.ActiveState == TROOP_STATE_IDLE)
							item->Animation.TargetState = TROOP_STATE_GUARD;
						else
							item->Animation.TargetState = TROOP_STATE_IDLE;
					}
				}
				else if (item->AIBits & PATROL1)
				{
					item->Animation.TargetState = TROOP_STATE_WALK;
					joint2 = 0;
				}
				else if (creature->Mood == MoodType::Escape)
					item->Animation.TargetState = TROOP_STATE_RUN;
				else if (Targetable(item, &AI))
				{
					if (AI.distance < pow(BLOCK(3), 2) || AI.zoneNumber != AI.enemyZone)
					{
						if (Random::TestProbability(1 / 2.0f))
							item->Animation.TargetState = TROOP_STATE_AIM_3;
						else
							item->Animation.TargetState = TROOP_STATE_AIM_1;
					}
					else
						item->Animation.TargetState = TROOP_STATE_WALK;
				}
				else
				{
					if ((creature->Alerted || creature->Mood != MoodType::Bored) &&
						(!(item->AIBits & FOLLOW) || !(item->AIBits & MODIFY) && distance <= pow(BLOCK(2), 2)))
					{
						if (creature->Mood == MoodType::Bored || AI.distance <= pow(BLOCK(2), 2))
						{
							item->Animation.TargetState = TROOP_STATE_WALK;
							break;
						}

						item->Animation.TargetState = TROOP_STATE_RUN;
					}
					else
						item->Animation.TargetState = TROOP_STATE_IDLE;
				}

				break;

			case TROOP_STATE_WALK:
				creature->MaxTurn = ANGLE(5.0f);
				creature->Flags = 0;
				joint2 = rot;

				if (item->AIBits & PATROL1)
					item->Animation.TargetState = TROOP_STATE_WALK;
				else if (creature->Mood == MoodType::Escape)
					item->Animation.TargetState = TROOP_STATE_RUN;
				else
				{
					if (item->AIBits & GUARD ||
						item->AIBits & FOLLOW &&
						(creature->ReachedGoal ||
							distance > pow(BLOCK(2), 2)))
					{
						item->Animation.TargetState = TROOP_STATE_IDLE;
					}
					else if (Targetable(item, &AI))
					{
						if (AI.distance < pow(BLOCK(3), 2) || AI.enemyZone != AI.zoneNumber)
							item->Animation.TargetState = TROOP_STATE_IDLE;
						else
							item->Animation.TargetState = TROOP_STATE_AIM_2;
					}
					else if (creature->Mood != MoodType::Bored)
					{
						if (AI.distance > pow(BLOCK(2), 2))
							item->Animation.TargetState = TROOP_STATE_RUN;
					}
					else if (AI.ahead)
						item->Animation.TargetState = TROOP_STATE_IDLE;
				}

				break;

			case TROOP_STATE_RUN:
				creature->MaxTurn = ANGLE(10.0f);
				tilt = angle / 2;

				if (AI.ahead)
					joint2 = AI.angle;

				if (item->AIBits & GUARD ||
					item->AIBits & FOLLOW &&
					(creature->ReachedGoal ||
						distance > pow(BLOCK(2), 2)))
				{
					item->Animation.TargetState = TROOP_STATE_WALK;
				}
				else if (creature->Mood != MoodType::Escape)
				{
					if (Targetable(item, &AI))
						item->Animation.TargetState = TROOP_STATE_WALK;
					else if (creature->Mood == MoodType::Bored ||
						creature->Mood == MoodType::Stalk &&
						!(item->AIBits & FOLLOW) &&
						AI.distance < pow(BLOCK(2), 2))
					{
						item->Animation.TargetState = TROOP_STATE_WALK;
					}
				}

				break;

			case TROOP_STATE_GUARD:
				creature->MaxTurn = 0;
				creature->Flags = 0;
				joint2 = rot;

				if (item->AIBits & GUARD)
				{
					// TODO: Use Random::TestProbability().
					joint2 = AIGuard(creature);
					if (!GetRandomControl())
						item->Animation.TargetState = TROOP_STATE_IDLE;
				}
				else if (Targetable(item, &AI))
					item->Animation.TargetState = TROOP_STATE_ATTACK_1;
				else if (creature->Mood != MoodType::Bored || !AI.ahead)
					item->Animation.TargetState = TROOP_STATE_IDLE;

				break;

			case TROOP_STATE_ATTACK_1:
			case TROOP_STATE_ATTACK_2:
				if (AI.ahead)
				{
					joint0 = AI.angle;
					joint1 = AI.xAngle;
				}

				if (creature->Flags)
				{
					creature->Flags--;
				}
				else
				{
					ShotLara(item, &AI, TroopsBite1, joint0, 23);
					creature->MuzzleFlash[0].Bite = TroopsBite1;
					creature->MuzzleFlash[0].Delay = 2;
					creature->Flags = 5;
				}

				break;

			case TROOP_STATE_AIM_1:
			case TROOP_STATE_AIM_3:
				creature->Flags = 0;

				if (AI.ahead)
				{
					joint1 = AI.xAngle;
					joint0 = AI.angle;

					if (Targetable(item, &AI))
						item->Animation.TargetState = item->Animation.ActiveState != TROOP_STATE_AIM_1 ? TROOP_STATE_ATTACK_3 : TROOP_STATE_ATTACK_1;
					else
						item->Animation.TargetState = TROOP_STATE_IDLE;
				}

				break;

			case TROOP_STATE_AIM_2:
				creature->Flags = 0;

				if (AI.ahead)
				{
					joint1 = AI.xAngle;
					joint0 = AI.angle;

					if (Targetable(item, &AI))
						item->Animation.TargetState = TROOP_STATE_ATTACK_2;
					else
						item->Animation.TargetState = TROOP_STATE_WALK;
				}

				break;

			case TROOP_STATE_ATTACK_3:
				if (item->Animation.TargetState != TROOP_STATE_IDLE &&
					(creature->Mood == MoodType::Escape ||
						AI.distance > pow(BLOCK(3), 2) ||
						!Targetable(item, &AI)))
				{
					item->Animation.TargetState = TROOP_STATE_IDLE;
				}

				if (AI.ahead)
				{
					joint0 = AI.angle;
					joint1 = AI.xAngle;
				}

				if (creature->Flags)
				{
					creature->Flags--;
				}
				else
				{
					ShotLara(item, &AI, TroopsBite1, joint0, 23);
					creature->MuzzleFlash[0].Bite = TroopsBite1;
					creature->MuzzleFlash[0].Delay = 2;
					creature->Flags = 5;
				}

				break;

			case TROOP_STATE_ATTACKED_BY_SCORPION:
				creature->MaxTurn = 0;
				break;

			case TROOP_STATE_FLASHED:
				if (!FlashGrenadeAftershockTimer && Random::TestProbability(1 / 128.0f))
					item->Animation.TargetState = TROOP_STATE_GUARD;

				break;

			default:
				break;
			}

			if (FlashGrenadeAftershockTimer > 100)
			{
				if (item->Animation.ActiveState != TROOP_STATE_FLASHED &&
					item->Animation.ActiveState != TROOP_STATE_ATTACKED_BY_SCORPION)
				{
					item->Animation.AnimNumber = 28;
					item->Animation.ActiveState = TROOP_STATE_FLASHED;
					item->Animation.FrameNumber = (GetRandomControl() & 7);
					creature->MaxTurn = 0;
				}
			}
		}

		CreatureTilt(item, tilt);
		CreatureJoint(item, 0, joint0);
		CreatureJoint(item, 1, joint1);
		CreatureJoint(item, 2, joint2);
		CreatureAnimation(itemNumber, angle, 0);
	}
}
