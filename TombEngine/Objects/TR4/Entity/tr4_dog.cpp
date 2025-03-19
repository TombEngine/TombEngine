#include "framework.h"
#include "Objects/TR4/Entity/tr4_dog.h"

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
	constexpr auto DOG_BITE_ATTACK_DAMAGE = 10;
	constexpr auto DOG_JUMP_ATTACK_DAMAGE = 20;

	constexpr auto DOG_BITE_ATTACK_RANGE = SQUARE(BLOCK(0.55));
	constexpr auto DOG_JUMP_ATTACK_RANGE = SQUARE(BLOCK(1));
	
	const auto DogBite = CreatureBiteInfo(Vector3(0, 0, 100), 3);
	const auto DogJumpAttackJoints = std::vector<unsigned int>{ 3, 6, 9, 10, 13, 14 };
	const auto DogBiteAttackJoints = std::vector<unsigned int>{ 3, 6 };

	enum DogState
	{
		DOG_STATE_NONE = 0, // TODO: Check what this is actually used for an rename accordingling.
		DOG_STATE_IDLE = 1,
		DOG_STATE_WALK_FORWARD = 2,
		DOG_STATE_RUN_FORWARD = 3,
		DOG_STATE_STALK = 5,
		DOG_STATE_JUMP_ATTACK = 6,
		DOG_STATE_HOWL = 7,
		DOG_STATE_SLEEP = 8,
		DOG_STATE_STALK_IDLE = 9,
		DOG_STATE_RUN_FORWARD_RIGHT = 10, // Unused.
		DOG_STATE_DEATH = 11,
		DOG_STATE_BITE_ATTACK = 12
	};

	enum DogAnim
	{
		DOG_ANIM_SLEEP = 0,
		DOG_ANIM_AWAKEN = 1,
		DOG_ANIM_IDLE_TO_WALK_FORWARD = 2,
		DOG_ANIM_WALK_FORWARD = 3,
		DOG_ANIM_WALK_FORWARD_TO_STALK = 4,
		DOG_ANIM_STALK_FORWARD = 5,
		DOG_ANIM_STALK_FORWARD_TO_RUN_FORWARD = 6,
		DOG_ANIM_RUN_FORWARD = 7,
		DOG_ANIM_IDLE = 8,
		DOG_ANIM_JUMP_ATTACK = 9,
		DOG_ANIM_STALK_IDLE_TO_STALK_FORWARD = 10,
		DOG_ANIM_STALK_FORWARD_TO_STALK_IDLE_START = 11,
		DOG_ANIM_STALK_FORWARD_TO_STALK_IDLE_END = 12,
		DOG_ANIM_STALK_IDLE_TO_RUN_FORWARD = 13,
		DOG_ANIM_STALK_IDLE = 14,
		DOG_ANIM_RUN_FORWARD_TO_STALK_IDLE = 15,
		DOG_ANIM_HOWL = 16,
		DOG_ANIM_IDLE_TO_STALK_IDLE = 17,
		DOG_ANIM_RUN_FORWARD_RIGHT = 18, // Unused.
		DOG_ANIM_WALK_FORWARD_TO_IDLE = 19,
		DOG_ANIM_DEATH_1 = 20,
		DOG_ANIM_DEATH_2 = 21,
		DOG_ANIM_DEATH_3 = 22,
		DOG_ANIM_BITE_ATTACK = 23,
		DOG_ANIM_STALK_IDLE_TO_IDLE = 24
	};

	const std::array DogDeathAnims = { DOG_ANIM_DEATH_1, DOG_ANIM_DEATH_2, DOG_ANIM_DEATH_3 };

	void InitializeTr4Dog(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		// OCB 1 makes the dog sitting down until fired
		if (item->TriggerFlags)
		{
			SetAnimation(*item, DOG_ANIM_AWAKEN);
			item->Status = ITEM_NOT_ACTIVE;
		}
		else
			SetAnimation(*item, DOG_ANIM_IDLE);
	}

	void Tr4DogControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);
		auto* object = &Objects[item->ObjectNumber];

		short angle = 0;
		short joint2 = 0;
		short joint1 = 0;
		short joint0 = 0;

		if (item->HitPoints <= 0)
		{
			if (item->Animation.AnimNumber == 1)
				item->HitPoints = object->HitPoints;
			else if (item->Animation.ActiveState != DOG_STATE_DEATH)
				SetAnimation(*item, DogDeathAnims[Random::GenerateInt(0, (int)DogDeathAnims.size() - 1)]);
		}
		else
		{
			if (item->AIBits)
				GetAITarget(creature);
			else
				creature->Enemy = LaraItem;

			AI_INFO AI;
			CreatureAIInfo(item, &AI);

			int distance;
			if (creature->Enemy->IsLara())
			{
				distance = AI.distance;
			}
			else
			{
				int dx = LaraItem->Pose.Position.x - item->Pose.Position.x;
				int dz = LaraItem->Pose.Position.z - item->Pose.Position.z;
				phd_atan(dz, dx);
				distance = pow(dx, 2) + pow(dz, 2);
			}

			if (AI.ahead)
			{
				joint2 = AI.xAngle; // TODO: Maybe swapped
				joint1 = AI.angle;
			}

			GetCreatureMood(item, &AI, true);
			CreatureMood(item, &AI, true);

			if (creature->Mood == MoodType::Bored)
				creature->MaxTurn /= 2;

			angle = CreatureTurn(item, creature->MaxTurn);
			joint0 = angle * 4;

			if (creature->HurtByLara || distance < pow(BLOCK(3), 2) && !(item->AIBits & MODIFY))
			{
				AlertAllGuards(itemNumber);
				item->AIBits &= ~MODIFY;
			}

			int frame = item->Animation.FrameNumber;

			switch (item->Animation.ActiveState)
			{
			case DOG_STATE_NONE:
			case DOG_STATE_SLEEP:
				joint1 = 0;
				joint2 = 0;

				if (creature->Mood != MoodType::Bored && item->AIBits != MODIFY)
					item->Animation.TargetState = DOG_STATE_IDLE;
				else
				{
					creature->Flags++;
					creature->MaxTurn = 0;

					if (creature->Flags > 300 && Random::TestProbability(1.0f / 256))
						item->Animation.TargetState = DOG_STATE_IDLE;
				}

				break;

			case DOG_STATE_IDLE:
			case DOG_STATE_STALK_IDLE:
				creature->MaxTurn = 0;

				if (item->Animation.ActiveState == DOG_STATE_STALK_IDLE &&
					item->Animation.RequiredState != NO_VALUE)
				{
					item->Animation.TargetState = item->Animation.RequiredState;
					break;
				}

				if (item->AIBits & GUARD)
				{
					joint1 = AIGuard(creature);

					if (Random::TestProbability(0.996f))
						break;

					if (item->Animation.ActiveState == DOG_STATE_IDLE)
					{
						item->Animation.TargetState = DOG_STATE_STALK_IDLE;
						break;
					}
				}
				else
				{
					if (item->Animation.ActiveState == DOG_STATE_STALK_IDLE &&
						Random::TestProbability(1.0f / 256))
					{
						item->Animation.TargetState = DOG_STATE_IDLE;
						break;
					}

					if (item->AIBits & PATROL1)
					{
						if (item->Animation.ActiveState == DOG_STATE_IDLE)
							item->Animation.TargetState = DOG_STATE_WALK_FORWARD;
						else
							item->Animation.TargetState = DOG_STATE_IDLE;

						break;
					}

					if (creature->Mood == MoodType::Escape)
					{
						if (Lara.TargetEntity == item || !AI.ahead || item->HitStatus)
						{
							item->Animation.TargetState = DOG_STATE_STALK_IDLE;
							item->Animation.RequiredState = DOG_STATE_RUN_FORWARD;
						}
						else
							item->Animation.TargetState = DOG_STATE_IDLE;

						break;
					}

					if (creature->Mood != MoodType::Bored)
					{
						if (item->Animation.ActiveState == DOG_STATE_IDLE)
							item->Animation.TargetState = DOG_STATE_STALK_IDLE;

						item->Animation.RequiredState = DOG_STATE_RUN_FORWARD;
						break;
					}

					creature->MaxTurn = ANGLE(1.0f);
					creature->Flags = 0;

					if (Random::TestProbability(1 / 128.0f))
					{
						if (item->AIBits & MODIFY)
						{
							if (item->Animation.ActiveState == DOG_STATE_IDLE)
							{
								item->Animation.TargetState = DOG_STATE_SLEEP;
								creature->Flags = 0;
								break;
							}
						}
					}

					if (Random::TestProbability(0.875f))
					{
						if (Random::TestProbability(1 / 30.0f))
							item->Animation.TargetState = DOG_STATE_HOWL;

						break;
					}

					if (item->Animation.ActiveState == DOG_STATE_IDLE)
					{
						item->Animation.TargetState = DOG_STATE_WALK_FORWARD;
						break;
					}
				}

				item->Animation.TargetState = DOG_STATE_IDLE;
				break;

			case DOG_STATE_WALK_FORWARD:
				creature->MaxTurn = ANGLE(3.0f);

				if (item->AIBits & PATROL1)
					item->Animation.TargetState = DOG_STATE_WALK_FORWARD;
				else if (creature->Mood == MoodType::Bored && Random::TestProbability(1 / 128.0f))
					item->Animation.TargetState = DOG_STATE_IDLE;
				else
					item->Animation.TargetState = DOG_STATE_STALK;

				break;

			case DOG_STATE_RUN_FORWARD:
				creature->MaxTurn = ANGLE(6.0f);

				if (creature->Mood == MoodType::Escape)
				{
					if (Lara.TargetEntity != item && AI.ahead)
						item->Animation.TargetState = DOG_STATE_STALK_IDLE;
				}
				else if (creature->Mood != MoodType::Bored)
				{
					if (AI.bite && AI.distance < DOG_JUMP_ATTACK_RANGE)
						item->Animation.TargetState = DOG_STATE_JUMP_ATTACK;
					else if (AI.distance < pow(BLOCK(1.5f), 2))
					{
						item->Animation.TargetState = DOG_STATE_STALK_IDLE;
						item->Animation.RequiredState = DOG_STATE_STALK;
					}
				}
				else
					item->Animation.TargetState = DOG_STATE_STALK_IDLE;

				break;

			case DOG_STATE_STALK:
				creature->MaxTurn = ANGLE(3.0f);

				if (creature->Mood != MoodType::Bored)
				{
					if (creature->Mood == MoodType::Escape)
						item->Animation.TargetState = DOG_STATE_RUN_FORWARD;
					else if (AI.bite && AI.distance < DOG_BITE_ATTACK_RANGE)
					{
						item->Animation.TargetState = DOG_STATE_BITE_ATTACK;
						item->Animation.RequiredState = DOG_STATE_STALK;
					}
					else if (AI.distance > pow(BLOCK(1.5f), 2) || item->HitStatus)
						item->Animation.TargetState = DOG_STATE_RUN_FORWARD;
				}
				else
					item->Animation.TargetState = DOG_STATE_STALK_IDLE;

				break;

			case DOG_STATE_JUMP_ATTACK:
				if (AI.bite && item->TouchBits.Test(DogJumpAttackJoints) &&
					frame >= 4 && frame <= 14)
				{
					DoDamage(creature->Enemy, DOG_JUMP_ATTACK_DAMAGE);
					CreatureEffect2(item, DogBite, 2, -1, DoBloodSplat);
				}

				item->Animation.TargetState = DOG_STATE_RUN_FORWARD;
				break;

			case DOG_STATE_HOWL:
				joint1 = 0;
				joint2 = 0;
				break;

			case DOG_STATE_BITE_ATTACK:
				if (AI.bite && item->TouchBits.Test(DogBiteAttackJoints) &&
					((frame >= 9 && frame <= 12) || (frame >= 22 && frame <= 25)))
				{
					DoDamage(creature->Enemy, DOG_BITE_ATTACK_DAMAGE);
					CreatureEffect2(item, DogBite, 2, -1, DoBloodSplat);
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
		CreatureAnimation(itemNumber, angle, 0);
	}
}
