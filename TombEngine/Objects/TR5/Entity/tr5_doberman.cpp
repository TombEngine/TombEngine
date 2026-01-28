#include "framework.h"
#include "Objects/TR5/Entity/tr5_doberman.h"

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

namespace TEN::Entities::Creatures::TR5
{
	const auto DobermanBite = CreatureBiteInfo(Vector3(0, 30, 141), 20);

	enum DobermanState
	{
		// No state 0.
		DOBERMAN_STATE_WALK_FORWARD = 1,
		DOBERMAN_STATE_RUN_FORWARD = 2,
		DOBERMAN_STATE_STOP = 3,
		DOBERMAN_STATE_STAND_LOW_BITE_ATTACK = 4,
		DOBERMAN_STATE_SIT_IDLE = 5,
		DOBERMAN_STATE_STAND_IDLE = 6,
		DOBERMAN_STATE_STAND_HIGH_BITE_ATTACK = 7,
		DOBERMAN_STATE_JUMP_BITE_ATTACK = 8,
		DOBERMAN_STATE_LEAP_BITE_ATTACK = 9,
		DOBERMAN_STATE_DEATH = 10,
	};

	enum DobermanAnim
	{
		DOBERMAN_ANIM_START_WALK = 0,
		DOBERMAN_ANIM_WALKING = 1,
		DOBERMAN_ANIM_LOW_BITE_ATTACK_START = 2,
		DOBERMAN_ANIM_LOW_BITE_ATTACK = 3,
		DOBERMAN_ANIM_LOW_BITE_ATTACK_END = 4,
		DOBERMAN_ANIM_SIT_START = 5,
		DOBERMAN_ANIM_SIT = 6,
		DOBERMAN_ANIM_SIT_TO_STAND = 7,
		DOBERMAN_ANIM_RUN_START = 8,
		DOBERMAN_ANIM_RUNNING = 9,
		DOBERMAN_ANIM_STAND_IDLE = 10,
		DOBERMAN_ANIM_IDLE = 11,
		DOBERMAN_ANIM_WALK_STOP = 12,
		DOBERMAN_ANIM_DEATH = 13,
		DOBERMAN_ANIM_JUMP_BITE = 14,
		DOBERMAN_ANIM_LEAP_BITE = 15,
		DOBERMAN_ANIM_JUMP_BITE_START = 16,
		DOBERMAN_ANIM_HIGH_BITE_START = 17,
		DOBERMAN_ANIM_HIGH_BITE = 18,
		DOBERMAN_ANIM_STOP_HIGH_BITE = 19,
		DOBERMAN_ANIM_RUN_STOP = 20,
	};

	void InitializeDoberman(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		if (item->TriggerFlags)
		{
			item->Animation.AnimNumber = DOBERMAN_ANIM_SIT;
			item->Animation.ActiveState = DOBERMAN_STATE_SIT_IDLE;
			// TODO: item->flags2 ^= (item->flags2 ^ ((item->flags2 & 0xFE) + 2)) & 6;
		}
		else
		{
			item->Animation.AnimNumber = DOBERMAN_ANIM_STAND_IDLE;
			item->Animation.ActiveState = DOBERMAN_STATE_STAND_IDLE;
		}

		item->Animation.FrameNumber = 0;
	}

	void DobermanControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short angle = 0;
		short tilt = 0;
		short joint = 0;

		if (item->HitPoints > 0)
		{
			AI_INFO AI;
			CreatureAIInfo(item, &AI);

			if (AI.ahead)
				joint = AI.angle;

			GetCreatureMood(item, &AI, false);
			CreatureMood(item, &AI, false);

			angle = CreatureTurn(item, creature->MaxTurn);

			switch (item->Animation.ActiveState)
			{
			case DOBERMAN_STATE_WALK_FORWARD:
				creature->MaxTurn = ANGLE(3.0f);

				if (creature->Mood != MoodType::Bored)
					item->Animation.TargetState = DOBERMAN_STATE_RUN_FORWARD;
				else
				{
					if (Random::TestProbability(0.025f))
					{
						item->Animation.RequiredState = DOBERMAN_STATE_STAND_LOW_BITE_ATTACK;
						item->Animation.TargetState = DOBERMAN_STATE_STOP;
						break;
					}

					if (Random::TestProbability(0.045f))
					{
						item->Animation.RequiredState = DOBERMAN_STATE_SIT_IDLE;
						item->Animation.TargetState = DOBERMAN_STATE_STOP;
						break;
					}

					if (Random::TestProbability(0.085f))
					{
						item->Animation.TargetState = DOBERMAN_STATE_STOP;
						break;
					}
				}

				break;

			case DOBERMAN_STATE_RUN_FORWARD:
				creature->MaxTurn = ANGLE(6.0f);
				tilt = angle;

				if (creature->Mood == MoodType::Bored)
				{
					item->Animation.TargetState = DOBERMAN_STATE_STOP;
					break;
				}

				if (AI.distance < pow(768, 2))
					item->Animation.TargetState = DOBERMAN_STATE_JUMP_BITE_ATTACK;

				break;

			case DOBERMAN_STATE_STOP:
				creature->MaxTurn = 0;
				creature->Flags = 0;

				if (creature->Mood != MoodType::Bored)
				{
					if (creature->Mood != MoodType::Escape &&
						AI.distance < pow(341, 2) &&
						AI.ahead)
					{
						item->Animation.TargetState = DOBERMAN_STATE_STAND_HIGH_BITE_ATTACK;
					}
					else
						item->Animation.TargetState = DOBERMAN_STATE_RUN_FORWARD;
				}
				else
				{
					if (item->Animation.RequiredState != NO_VALUE)
						item->Animation.TargetState = item->Animation.RequiredState;
					else
					{
						if (Random::TestProbability(0.975f))
						{
							if (Random::TestProbability(0.95f))
							{
								if (Random::TestProbability(0.3f))
									item->Animation.TargetState = DOBERMAN_STATE_WALK_FORWARD;
							}
							else
								item->Animation.TargetState = DOBERMAN_STATE_SIT_IDLE;
						}
						else
							item->Animation.TargetState = DOBERMAN_STATE_STAND_LOW_BITE_ATTACK;
					}
				}

				break;

			case DOBERMAN_STATE_STAND_LOW_BITE_ATTACK:
				if (creature->Mood != MoodType::Bored || Random::TestProbability(0.04f))
					item->Animation.TargetState = DOBERMAN_STATE_STOP;

				break;

			case DOBERMAN_STATE_SIT_IDLE:
				if (creature->Mood != MoodType::Bored || Random::TestProbability(1 / 128.0f))
					item->Animation.TargetState = DOBERMAN_STATE_STOP;

				break;

			case DOBERMAN_STATE_STAND_IDLE:
				if (creature->Mood != MoodType::Bored || Random::TestProbability(1 / 64.0f))
					item->Animation.TargetState = DOBERMAN_STATE_STOP;

				break;

			case DOBERMAN_STATE_STAND_HIGH_BITE_ATTACK:
				creature->MaxTurn = ANGLE(0.5f);

				if (creature->Flags != 1 && AI.ahead &&
					item->TouchBits & 0x122000)
				{
					DoDamage(creature->Enemy, 30);
					CreatureEffect(item, DobermanBite, DoBloodSplat);
					creature->Flags = 1;
				}

				if (AI.distance <= pow(341, 2) || AI.distance >= pow(682, 2))
					item->Animation.TargetState = DOBERMAN_STATE_STOP;
				else
					item->Animation.TargetState = DOBERMAN_STATE_LEAP_BITE_ATTACK;

				break;

			case DOBERMAN_STATE_JUMP_BITE_ATTACK:
				if (creature->Flags != 2 && item->TouchBits & 0x122000)
				{
					DoDamage(creature->Enemy, 80);
					CreatureEffect(item, DobermanBite, DoBloodSplat);
					creature->Flags = 2;
				}

				if (AI.distance >= pow(341, 2))
				{
					if (AI.distance < pow(682, 2))
						item->Animation.TargetState = DOBERMAN_STATE_LEAP_BITE_ATTACK;
				}
				else
					item->Animation.TargetState = DOBERMAN_STATE_STAND_HIGH_BITE_ATTACK;

				break;

			case DOBERMAN_STATE_LEAP_BITE_ATTACK:
				creature->MaxTurn = ANGLE(6.0f);

				if (creature->Flags != 3 && item->TouchBits & 0x122000)
				{
					DoDamage(creature->Enemy, 50);
					CreatureEffect(item, DobermanBite, DoBloodSplat);
					creature->Flags = 3;
				}

				if (AI.distance < pow(341, 2))
					item->Animation.TargetState = DOBERMAN_STATE_STAND_HIGH_BITE_ATTACK;

				break;

			default:
				break;
			}
		}
		else if (item->Animation.ActiveState != DOBERMAN_STATE_DEATH)
		{
			item->Animation.AnimNumber = DOBERMAN_ANIM_DEATH;
			item->Animation.FrameNumber = 0;
			item->Animation.ActiveState = DOBERMAN_STATE_DEATH;
		}

		CreatureTilt(item, tilt);
		CreatureJoint(item, 0, 0);
		CreatureJoint(item, 1, joint);
		CreatureJoint(item, 2, 0);
		CreatureAnimation(itemNumber, angle, tilt);
	}
}
