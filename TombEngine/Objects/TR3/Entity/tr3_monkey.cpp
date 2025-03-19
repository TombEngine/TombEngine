#include "framework.h"
#include "Objects/TR3/Entity/tr3_monkey.h"

#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/control/lot.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/itemdata/creature_info.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Specific/level.h"

using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR3
{
	// TODO: Work out damage constants.
	constexpr auto MONKEY_SWIPE_ATTACK_PLAYER_DAMAGE = 40;
	constexpr auto MONKEY_SWIPE_ATTACK_CREATURE_DAMAGE = 20;

	// TODO: Range constants.

	const auto MonkeyBite = CreatureBiteInfo(Vector3(10, 10, 11), 13);
	const auto MonkeyAttackJoints = std::vector<unsigned int>{ 10, 13 };

	enum MonkeyState
	{
		// No states 0-1.
		MONKEY_STATE_WALK_FORWARD = 2,
		MONKEY_STATE_IDLE = 3,
		MONKEY_STATE_RUN_FORWARD = 4,
		MONKEY_STATE_BITE_ATTACK = 5, // Check.
		MONKEY_STATE_SIT = 6,
		MONKEY_STATE_SIT_EAT = 7,
		MONKEY_STATE_SIT_SCRATCH = 8,
		MONKEY_STATE_RUN_FORWARD_ROLL = 9,
		MONKEY_STATE_POUND_GROUND = 10,
		MONKEY_STATE_DEATH = 11,
		MONKEY_STATE_SWIPE_ATTACK = 12,
		MONKEY_STATE_JUMP_ATTACK = 13,
		MONKEY_STATE_HIGH_JUMP_ATTACK = 14,
		MONKEY_STATE_VAULT_UP_1_BLOCK = 15,
		MONKEY_STATE_VAULT_UP_0_POINT_3_BLOCKS = 16,
		MONKEY_STATE_VAULT_UP_0_POINT_2_BLOCKS = 17,
		MONKEY_STATE_VAULT_DOWN_1_BLOCK = 18,
		MONKEY_STATE_VAULT_DOWN_0_POINT_3_BLOCKS = 19,
		MONKEY_STATE_VAULT_DOWN_0_POINT_2_BLOCKS = 20
	};

	enum MonkeyAnim
	{
		MONKEY_ANIM_WALK_FORWARD = 0,
		MONKEY_ANIM_WALK_FORWARD_TO_SIT = 1,
		MONKEY_ANIM_SIT = 2,
		MONKEY_ANIM_SIT_TO_WALK_FORWARD = 3,
		MONKEY_ANIM_SIT_EAT = 4,
		MONKEY_ANIM_SIT_SCRATCH = 5,
		MONKEY_ANIM_RUN_FORWARD = 6,
		MONKEY_ANIM_RUN_FORWARD_ROLL = 7,
		MONKEY_ANIM_IDLE_POUND_GROUND = 8,
		MONKEY_ANIM_IDLE = 9,
		MONKEY_ANIM_IDLE_TO_RUN_FORWARD = 10,
		MONKEY_ANIM_WALK_FORWARD_TO_RUN_FORWARD = 11,
		MONKEY_ANIM_RUN_FORWARD_TO_IDLE = 12,
		MONKEY_ANIM_SIT_TO_IDLE = 13,
		MONKEY_ANIM_DEATH = 14,
		MONKEY_ANIM_RUN_FORWARD_TO_WALK_FORWARD = 15,
		MONKEY_ANIM_IDLE_TO_SIT = 16,
		MONKEY_ANIM_VAULT_UP_1_BLOCK = 17,
		MONKEY_ANIM_VAULT_UP_0_POINT_3_BLOCKS = 18,
		MONKEY_ANIM_VAULT_UP_0_POINT_2_BLOCKS = 19,
		MONKEY_ANIM_VAULT_DOWN_1_BLOCK = 20,
		MONKEY_ANIM_VAULT_DOWN_0_POINT_3_BLOCKS = 21,
		MONKEY_ANIM_VAULT_DOWN_0_POINT_2_BLOCKS = 22,
		MONKEY_ANIM_SWIPE_ATTACK = 23,
		MONKEY_ANIM_JUMP_ATTACK = 24,
		MONKEY_ANIM_BITE_ATTACK = 25,
		MONKEY_ANIM_HIGH_JUMP_ATTACK_START = 26,
		MONKEY_ANIM_HIGH_JUMP_ATTACK_CONTINUE = 27,
		MONKEY_ANIM_HIGH_JUMP_ATTACK_END = 28,
		MONKEY_ANIM_IDLE_TO_WALK_FORWARD = 29,
		MONKEY_ANIM_WALK_FORWARD_TO_IDLE = 30
	};

	void InitializeMonkey(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(*item, MONKEY_ANIM_SIT);
	}

	void MonkeyControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short angle = 0;
		short tilt = 0;
		auto extraHeadRot = EulerAngles::Identity;
		auto extraTorsoRot = EulerAngles::Identity;

		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState != MONKEY_STATE_DEATH)
			{
				SetAnimation(*item, MONKEY_ANIM_DEATH);
				item->MeshBits = ALL_JOINT_BITS;
			}
		}
		else
		{
			GetAITarget(creature);

			if (creature->HurtByLara)
				creature->Enemy = LaraItem;
			else
			{
				creature->Enemy = nullptr;
				int minDistance = INT_MAX;

				for (auto& currentCreature : ActiveCreatures)
				{
					if (currentCreature->ItemNumber == NO_VALUE || currentCreature->ItemNumber == itemNumber)
						continue;

					auto* target = &g_Level.Items[currentCreature->ItemNumber];
					if (target->ObjectNumber == ID_LARA || target->ObjectNumber == ID_MONKEY)
						continue;

					if (target->ObjectNumber == ID_SMALLMEDI_ITEM)
					{
						int x = target->Pose.Position.x - item->Pose.Position.x;
						int z = target->Pose.Position.z - item->Pose.Position.z;
						int distance = pow(x, 2) + pow(z, 2);

						if (distance < minDistance)
						{
							creature->Enemy = target;
							minDistance = distance;
						}
					}
				}
			}

			if (item->AIBits != MODIFY)
			{
				if (item->CarriedItem != NO_VALUE)
					item->MeshBits = 0xFFFFFEFF;
				else
					item->MeshBits = ALL_JOINT_BITS;
			}
			else
			{
				if (item->CarriedItem != NO_VALUE)
					item->MeshBits = 0xFFFF6E6F;
				else
					item->MeshBits = 0xFFFF6F6F;
			}

			AI_INFO AI;
			CreatureAIInfo(item, &AI);

			if (creature->Enemy != nullptr)
			{
				if (!creature->HurtByLara && creature->Enemy->IsLara())
					creature->Enemy = nullptr;
			}

			AI_INFO laraAI;
			if (creature->Enemy != nullptr && creature->Enemy->IsLara())
			{
				laraAI.angle = AI.angle;
				laraAI.distance = AI.distance;
			}
			else
			{
				int dx = LaraItem->Pose.Position.x - item->Pose.Position.x;
				int dz = LaraItem->Pose.Position.z - item->Pose.Position.z;

				laraAI.angle = phd_atan(dz, dz) - item->Pose.Orientation.y;
				laraAI.distance = pow(dx, 2) + pow(dz, 2);
			}

			GetCreatureMood(item, &AI, true);

			if (Lara.Context.Vehicle != NO_VALUE)
				creature->Mood = MoodType::Escape;

			CreatureMood(item, &AI, true);

			angle = CreatureTurn(item, creature->MaxTurn);

			auto* enemy = creature->Enemy;
			creature->Enemy = LaraItem;

			if (item->HitStatus)
				AlertAllGuards(itemNumber);

			creature->Enemy = enemy;

			switch (item->Animation.ActiveState)
			{
			case MONKEY_STATE_SIT:
				creature->MaxTurn = 0;
				creature->Flags = 0;
				extraTorsoRot.y = laraAI.angle;

				if (item->AIBits & GUARD)
				{
					extraTorsoRot.y = AIGuard(creature);
					if (Random::TestProbability(0.06f))
					{
						if (Random::TestProbability(1 / 2.0f))
							item->Animation.TargetState = MONKEY_STATE_SIT_EAT;
						else
							item->Animation.TargetState = MONKEY_STATE_SIT_SCRATCH;
					}

					break;
				}
				else if (item->AIBits & PATROL1)
					item->Animation.TargetState = MONKEY_STATE_WALK_FORWARD;
				else if (creature->Mood == MoodType::Escape)
					item->Animation.TargetState = MONKEY_STATE_IDLE;
				else if (creature->Mood == MoodType::Bored)
				{
					if (item->Animation.RequiredState != NO_VALUE)
						item->Animation.TargetState = item->Animation.RequiredState;
					else if (Random::TestProbability(0.06f))
						item->Animation.TargetState = MONKEY_STATE_WALK_FORWARD;
					else if (Random::TestProbability(0.06f))
					{
						if (Random::TestProbability(1 / 2.0f))
							item->Animation.TargetState = MONKEY_STATE_SIT_EAT;
						else
							item->Animation.TargetState = MONKEY_STATE_SIT_SCRATCH;
					}
				}
				else if ((item->AIBits & FOLLOW) &&
					(creature->ReachedGoal || laraAI.distance > pow(BLOCK(2), 2)))
				{
					if (item->Animation.RequiredState != NO_VALUE)
						item->Animation.TargetState = item->Animation.RequiredState;
					else if (AI.ahead)
						item->Animation.TargetState = MONKEY_STATE_SIT;
					else
						item->Animation.TargetState = MONKEY_STATE_IDLE;
				}
				else if (AI.bite && AI.distance < pow(682, 2))
					item->Animation.TargetState = MONKEY_STATE_IDLE;
				else if (AI.bite && AI.distance < pow(682, 2))
					item->Animation.TargetState = MONKEY_STATE_WALK_FORWARD;
				else
					item->Animation.TargetState = MONKEY_STATE_IDLE;

				break;

			case MONKEY_STATE_IDLE:
				creature->MaxTurn = 0;
				creature->Flags = 0;
				extraTorsoRot.y = laraAI.angle;

				if (item->AIBits & GUARD)
				{
					extraTorsoRot.y = AIGuard(creature);

					if (Random::TestProbability(0.06f))
					{
						if (Random::TestProbability(1 / 2.0f))
							item->Animation.TargetState = MONKEY_STATE_POUND_GROUND;
						else
							item->Animation.TargetState = MONKEY_STATE_SIT;
					}

					break;
				}
				else if (item->AIBits & PATROL1)
					item->Animation.TargetState = MONKEY_STATE_WALK_FORWARD;
				else if (creature->Mood == MoodType::Escape)
				{
					if (Lara.TargetEntity != item && AI.ahead)
						item->Animation.TargetState = MONKEY_STATE_IDLE;
					else
						item->Animation.TargetState = MONKEY_STATE_RUN_FORWARD;
				}
				else if (creature->Mood == MoodType::Bored)
				{
					if (item->Animation.RequiredState != NO_VALUE)
						item->Animation.TargetState = item->Animation.RequiredState;
					else if (Random::TestProbability(0.06f))
						item->Animation.TargetState = MONKEY_STATE_WALK_FORWARD;
					else if (Random::TestProbability(0.06f))
					{
						if (Random::TestProbability(1 / 2.0f))
							item->Animation.TargetState = MONKEY_STATE_POUND_GROUND;
						else
							item->Animation.TargetState = MONKEY_STATE_SIT;
					}
				}
				else if (item->AIBits & FOLLOW &&
					(creature->ReachedGoal || laraAI.distance > pow(BLOCK(2), 2)))
				{
					if (item->Animation.RequiredState != NO_VALUE)
						item->Animation.TargetState = item->Animation.RequiredState;
					else if (AI.ahead)
						item->Animation.TargetState = MONKEY_STATE_SIT;
					else
						item->Animation.TargetState = MONKEY_STATE_RUN_FORWARD;
				}
				else if (AI.bite && AI.distance < pow(341, 2))
				{
					if (LaraItem->Pose.Position.y < item->Pose.Position.y)
						item->Animation.TargetState = MONKEY_STATE_JUMP_ATTACK;
					else
						item->Animation.TargetState = MONKEY_STATE_SWIPE_ATTACK;
				}
				else if (AI.bite && AI.distance < pow(682, 2))
					item->Animation.TargetState = MONKEY_STATE_HIGH_JUMP_ATTACK;
				else if (AI.bite && AI.distance < pow(682, 2))
					item->Animation.TargetState = MONKEY_STATE_WALK_FORWARD;
				else if (AI.distance < pow(682, 2) &&
					!creature->Enemy->IsLara() && creature->Enemy != nullptr &&
					creature->Enemy->ObjectNumber != ID_AI_PATROL1 &&
					creature->Enemy->ObjectNumber != ID_AI_PATROL2 &&
					abs(item->Pose.Position.y - creature->Enemy->Pose.Position.y) < CLICK(1))
				{
					item->Animation.TargetState = MONKEY_STATE_BITE_ATTACK;
				}
				else if (AI.bite && AI.distance < pow(BLOCK(1), 2))
					item->Animation.TargetState = MONKEY_STATE_RUN_FORWARD_ROLL;
				else
					item->Animation.TargetState = MONKEY_STATE_RUN_FORWARD;

				break;

			case MONKEY_STATE_BITE_ATTACK:
				creature->ReachedGoal = true;

				if (creature->Enemy == nullptr)
					break;
				else if ((creature->Enemy->ObjectNumber == ID_SMALLMEDI_ITEM ||
					creature->Enemy->ObjectNumber == ID_KEY_ITEM4) &&
					item->Animation.FrameNumber == 12)
				{
					if (creature->Enemy->RoomNumber == NO_VALUE ||
						creature->Enemy->Status == ITEM_INVISIBLE ||
						creature->Enemy->Flags & -32768)
					{
						creature->Enemy = nullptr;
					}
					else
					{
						item->CarriedItem = creature->Enemy->Index;
						RemoveDrawnItem(creature->Enemy->Index);
						creature->Enemy->RoomNumber = NO_VALUE;
						creature->Enemy->CarriedItem = NO_VALUE;

						for (auto& currentCreature : ActiveCreatures)
						{
							if (currentCreature->ItemNumber == NO_VALUE || currentCreature->ItemNumber == itemNumber)
								continue;

							auto* target = &g_Level.Items[currentCreature->ItemNumber];
							if (currentCreature->Enemy == creature->Enemy)
								currentCreature->Enemy = nullptr;
						}

						creature->Enemy = nullptr;

						if (item->AIBits != MODIFY)
						{
							item->AIBits |= AMBUSH;
							item->AIBits |= MODIFY;
						}
					}
				}
				else if (creature->Enemy->ObjectNumber == ID_AI_AMBUSH &&
					item->Animation.FrameNumber == 12)
				{
					item->AIBits = 0;

					auto* carriedItem = &g_Level.Items[item->CarriedItem];

					carriedItem->Pose.Position = item->Pose.Position;

					ItemNewRoom(item->CarriedItem, item->RoomNumber);
					item->CarriedItem = NO_VALUE;

					carriedItem->AIBits = GUARD;
					creature->Enemy = nullptr;
				}
				else
				{
					creature->MaxTurn = 0;

					if (abs(AI.angle) < ANGLE(7.0f))
						item->Pose.Orientation.y += AI.angle;
					else if (AI.angle < 0)
						item->Pose.Orientation.y -= ANGLE(7.0f);
					else
						item->Pose.Orientation.y += ANGLE(7.0f);
				}

				break;

			case MONKEY_STATE_WALK_FORWARD:
				creature->MaxTurn = ANGLE(7.0f);
				extraTorsoRot.y = laraAI.angle;

				if (item->AIBits & PATROL1)
				{
					item->Animation.TargetState = MONKEY_STATE_WALK_FORWARD;
					extraTorsoRot.y = 0;
				}
				else if (creature->Mood == MoodType::Escape)
					item->Animation.TargetState = MONKEY_STATE_RUN_FORWARD;
				else if (creature->Mood == MoodType::Bored)
				{
					if (Random::TestProbability(1 / 128.0f))
						item->Animation.TargetState = MONKEY_STATE_SIT;
				}
				else if (AI.bite && AI.distance < pow(682, 2))
					item->Animation.TargetState = MONKEY_STATE_IDLE;
				
				break;

			case MONKEY_STATE_RUN_FORWARD:
				creature->MaxTurn = ANGLE(11.0f);
				tilt = angle / 2;

				if (AI.ahead)
					extraTorsoRot.y = AI.angle;

				if (item->AIBits & GUARD)
					item->Animation.TargetState = MONKEY_STATE_IDLE;
				else if (creature->Mood == MoodType::Escape)
				{
					if (Lara.TargetEntity != item && AI.ahead)
						item->Animation.TargetState = MONKEY_STATE_IDLE;

					break;
				}
				else if ((item->AIBits & FOLLOW) &&
					(creature->ReachedGoal || laraAI.distance > pow(BLOCK(2), 2)))
				{
					item->Animation.TargetState = MONKEY_STATE_IDLE;
				}
				else if (creature->Mood == MoodType::Bored)
					item->Animation.TargetState = MONKEY_STATE_RUN_FORWARD_ROLL;
				else if (AI.distance < pow(682, 2))
					item->Animation.TargetState = MONKEY_STATE_IDLE;
				else if (AI.bite && AI.distance < pow(BLOCK(1), 2))
					item->Animation.TargetState = MONKEY_STATE_RUN_FORWARD_ROLL;

				break;

			case MONKEY_STATE_SWIPE_ATTACK:
				creature->MaxTurn = 0;

				if (AI.ahead)
				{
					extraHeadRot.x = AI.xAngle;
					extraHeadRot.y = AI.angle;
				}

				if (abs(AI.angle) < ANGLE(7.0f))
					item->Pose.Orientation.y += AI.angle;
				else if (AI.angle < 0)
					item->Pose.Orientation.y -= ANGLE(7.0f);
				else
					item->Pose.Orientation.y += ANGLE(7.0f);

				if (enemy->IsLara())
				{
					if (!creature->Flags && item->TouchBits.Test(MonkeyAttackJoints))
					{
						DoDamage(enemy, MONKEY_SWIPE_ATTACK_PLAYER_DAMAGE);
						CreatureEffect(item, MonkeyBite, DoBloodSplat);
						creature->Flags = 1;
					}
				}
				else
				{
					if (!creature->Flags && enemy != nullptr)
					{
						float distance = Vector3i::Distance(item->Pose.Position, enemy->Pose.Position);
						if (distance <= CLICK(1))
						{
							DoDamage(enemy, MONKEY_SWIPE_ATTACK_CREATURE_DAMAGE);
							CreatureEffect(item, MonkeyBite, DoBloodSplat);
							creature->Flags = 1;
						}
					}
				}

				break;

			case MONKEY_STATE_JUMP_ATTACK:
				creature->MaxTurn = 0;

				if (AI.ahead)
				{
					extraHeadRot.x = AI.xAngle;
					extraHeadRot.y = AI.angle;
				}

				if (abs(AI.angle) < ANGLE(7.0f))
					item->Pose.Orientation.y += AI.angle;
				else if (AI.angle < 0)
					item->Pose.Orientation.y -= ANGLE(7.0f);
				else
					item->Pose.Orientation.y += ANGLE(7.0f);

				if (enemy->IsLara())
				{
					if (!creature->Flags && item->TouchBits.Test(MonkeyAttackJoints))
					{
						DoDamage(enemy, 40);
						CreatureEffect(item, MonkeyBite, DoBloodSplat);
						creature->Flags = 1;
					}
				}
				else
				{
					if (!creature->Flags && enemy != nullptr)
					{
						if (Vector3i::Distance(item->Pose.Position, enemy->Pose.Position) <= CLICK(1))
						{
							DoDamage(enemy, 20);
							CreatureEffect(item, MonkeyBite, DoBloodSplat);
							creature->Flags = 1;
						}
					}
				}

				break;

			case MONKEY_STATE_HIGH_JUMP_ATTACK:
				creature->MaxTurn = 0;

				if (AI.ahead)
				{
					extraHeadRot.x = AI.xAngle;
					extraHeadRot.y = AI.angle;
				}

				if (abs(AI.angle) < ANGLE(7.0f))
					item->Pose.Orientation.y += AI.angle;
				else if (AI.angle < 0)
					item->Pose.Orientation.y -= ANGLE(7.0f);
				else
					item->Pose.Orientation.y += ANGLE(7.0f);

				if (enemy->IsLara())
				{
					if (creature->Flags != 1 && item->TouchBits.Test(MonkeyAttackJoints))
					{
						DoDamage(enemy, 50);
						CreatureEffect(item, MonkeyBite, DoBloodSplat);
						creature->Flags = 1;
					}
				}
				else
				{
					if (creature->Flags != 1 && enemy != nullptr)
					{
						if (Vector3i::Distance(item->Pose.Position, enemy->Pose.Position) <= CLICK(1))
						{
							DoDamage(enemy, 25);
							CreatureEffect(item, MonkeyBite, DoBloodSplat);
							creature->Flags = 1;
						}
					}
				}

				break;
			}
		}

		CreatureTilt(item, tilt);
		CreatureJoint(item, 0, extraHeadRot.y);
		CreatureJoint(item, 1, extraHeadRot.x);
		CreatureJoint(item, 2, extraTorsoRot.y);

		if (item->Animation.ActiveState < MONKEY_STATE_VAULT_UP_1_BLOCK)
		{
			switch (CreatureVault(itemNumber, angle, 2, CLICK(0.5f)))
			{
			case 2:
				SetAnimation(*item, MONKEY_ANIM_VAULT_UP_0_POINT_2_BLOCKS);
				creature->MaxTurn = 0;
				break;

			case 3:
				SetAnimation(*item, MONKEY_ANIM_VAULT_UP_0_POINT_3_BLOCKS);
				creature->MaxTurn = 0;
				break;

			case 4:
				SetAnimation(*item, MONKEY_ANIM_VAULT_UP_1_BLOCK);
				creature->MaxTurn = 0;
				break;

			case -2:
				SetAnimation(*item, MONKEY_ANIM_VAULT_DOWN_0_POINT_2_BLOCKS);
				creature->MaxTurn = 0;
				break;

			case -3:
				SetAnimation(*item, MONKEY_ANIM_VAULT_DOWN_0_POINT_3_BLOCKS);
				creature->MaxTurn = 0;
				break;

			case -4:
				SetAnimation(*item, MONKEY_ANIM_VAULT_DOWN_1_BLOCK);
				creature->MaxTurn = 0;
				break;
			}
		}
		else
		{
			creature->MaxTurn = 0;
			CreatureAnimation(itemNumber, angle, tilt);
		}
	}
}
