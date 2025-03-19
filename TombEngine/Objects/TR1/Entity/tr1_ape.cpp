#include "framework.h"
#include "Objects/TR1/Entity/tr1_ape.h"

#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Specific/level.h"

using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR1
{
	constexpr auto APE_ATTACK_DAMAGE = 200;

	constexpr auto APE_ATTACK_RANGE = SQUARE(BLOCK(0.42f));
	constexpr auto APE_PANIC_RANGE	= SQUARE(BLOCK(2));

	constexpr auto APE_IDLE_JUMP_CHANCE			= 1 / 6.0f;
	constexpr auto APE_IDLE_POUND_CHEST_CHANCE  = 1 / 3.0f;
	constexpr auto APE_IDLE_POUND_GROUND_CHANCE = 1 / 2.0f;
	constexpr auto APE_IDLE_RUN_LEFT_CHANCE		= 1 / 2.0f;
	constexpr auto APE_RUN_JUMP_CHANCE			= APE_IDLE_JUMP_CHANCE / 32;
	constexpr auto APE_RUN_POUND_CHEST_CHANCE	= APE_IDLE_POUND_CHEST_CHANCE / 32;
	constexpr auto APE_RUN_POUND_GROUND_CHANCE	= APE_IDLE_POUND_GROUND_CHANCE / 32;
	constexpr auto APE_RUN_RUN_LEFT_CHANCE		= APE_IDLE_RUN_LEFT_CHANCE / 32;

	constexpr auto APE_SHIFT = 75;

	constexpr auto APE_RUN_TURN_RATE_MAX = ANGLE(5.0f);
	constexpr auto APE_DISPLAY_ANGLE	 = ANGLE(45.0f);

	const auto ApeBite = CreatureBiteInfo(Vector3(0, -19, 75), 15);
	const auto ApeAttackJoints = std::vector<unsigned int>{ 8, 9, 10, 11, 12, 13, 14, 15 };

	enum ApeState
	{
		// No state 0.
		APE_STATE_IDLE = 1,
		APE_STATE_WALK_FORWARD = 2,
		APE_STATE_RUN_FORWARD = 3,
		APE_STATE_ATTACK = 4,
		APE_STATE_DEATH = 5,
		APE_STATE_POUND_CHEST = 6,
		APE_STATE_POUND_GROUND = 7,
		APE_STATE_RUN_LEFT = 8,
		APE_STATE_RUN_RIGHT = 9,
		APE_STATE_JUMP = 10,
		APE_STATE_VAULT = 11
	};

	enum ApeAnim
	{
		APE_ANIM_IDLE = 0,
		APE_ANIM_POUND_CHEST = 1,
		APE_ANIM_POUND_GROUND = 2,
		APE_ANIM_IDLE_TO_RUN_FORWARD = 3,
		APE_ANIM_RUN_FORWARD_TO_IDLE = 4,
		APE_ANIM_JUMP = 5,
		APE_ANIM_RUN_FORWARD = 6,
		APE_ANIM_DEATH_1 = 7,
		APE_ANIM_DEATH_2 = 8,
		APE_ANIM_ATTACK_START = 9,
		APE_ANIM_ATTACK_CONTINUE_1 = 10,
		APE_ANIM_ATTACK_CONTINUE_2 = 11,
		APE_ANIM_ATTACK_END = 12,
		APE_ANIM_RUN_LEFT = 13,
		APE_ANIM_RUN_RIGHT = 14,
		APE_ANIM_RUN_LEFT_TO_IDLE = 15,
		APE_ANIM_RUN_RIGHT_TO_IDLE = 16,
		APE_ANIM_IDLE_TO_RUN_LEFT = 17,
		APE_ANIM_IDLE_TO_RUN_RIGHT = 18,
		APE_ANIM_VAULT = 19
	};

	const std::array ApeDeathAnims = { APE_ANIM_DEATH_1, APE_ANIM_DEATH_2 };

	enum ApeFlags
	{
		APE_FLAG_ATTACK		= (1 << 0),
		APE_FLAG_TURN_LEFT	= (1 << 1),
		APE_FLAG_TURN_RIGHT = (1 << 2)
	};

	void ApeVault(short itemNumber, short angle)
	{
		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		if (creature->Flags & APE_FLAG_TURN_LEFT)
		{
			item->Pose.Orientation.y -= ANGLE(90.0f);
			creature->Flags &= ~APE_FLAG_TURN_LEFT;
		}
		else if (item->Flags & APE_FLAG_TURN_RIGHT)
		{
			item->Pose.Orientation.y += ANGLE(90.0f);
			creature->Flags &= ~APE_FLAG_TURN_RIGHT;
		}

		int xx = item->Pose.Position.z / BLOCK(1);
		int yy = item->Pose.Position.x / BLOCK(1);
		int y = item->Pose.Position.y;

		CreatureAnimation(itemNumber, angle, 0);

		if (item->Pose.Position.y > (y - CLICK(1.5f)))
			return;

		int xFloor = item->Pose.Position.z / BLOCK(1);
		int yFloor = item->Pose.Position.x / BLOCK(1);
		if (xx == xFloor)
		{
			if (yy == yFloor)
				return;

			if (yy < yFloor)
			{
				item->Pose.Position.x = (yFloor * BLOCK(1)) - APE_SHIFT;
				item->Pose.Orientation.y = ANGLE(90.0f);
			}
			else
			{
				item->Pose.Position.x = (yy * BLOCK(1)) + APE_SHIFT;
				item->Pose.Orientation.y = -ANGLE(90.0f);
			}
		}
		else if (yy == yFloor)
		{
			if (xx < xFloor)
			{
				item->Pose.Position.z = (xFloor * BLOCK(1)) - APE_SHIFT;
				item->Pose.Orientation.y = 0;
			}
			else
			{
				item->Pose.Position.z = (xx * BLOCK(1)) + APE_SHIFT;
				item->Pose.Orientation.y = -ANGLE(180.0f);
			}
		}
		else
		{
			// diagonal
		}

		if (CreatureVault(itemNumber, angle, 2, APE_SHIFT) == 2)
		{
			item->Pose.Position.y = y;
			SetAnimation(*item, APE_ANIM_VAULT);
		}
	}

	void ApeControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creatureInfo = GetCreatureInfo(item);

		short head = 0;
		short angle = 0;

		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState != APE_STATE_DEATH)
				SetAnimation(*item, ApeDeathAnims[Random::GenerateInt(0, (int)ApeDeathAnims.size() - 1)]);
		}
		else
		{
			AI_INFO AI;
			CreatureAIInfo(item, &AI);

			if (AI.ahead)
				head = AI.angle;

			GetCreatureMood(item, &AI, false);
			CreatureMood(item, &AI, false);

			angle = CreatureTurn(item, creatureInfo->MaxTurn);

			if (item->HitStatus || AI.distance < APE_PANIC_RANGE)
				creatureInfo->Flags |= APE_FLAG_ATTACK;

			switch (item->Animation.ActiveState)
			{
			case APE_STATE_IDLE:
				if (creatureInfo->Flags & APE_FLAG_TURN_LEFT)
				{
					item->Pose.Orientation.y -= ANGLE(90.0f);
					creatureInfo->Flags -= APE_FLAG_TURN_LEFT;
				}
				else if (item->Flags & APE_FLAG_TURN_RIGHT)
				{
					item->Pose.Orientation.y += ANGLE(90.0f);
					creatureInfo->Flags -= APE_FLAG_TURN_RIGHT;
				}

				if (item->Animation.RequiredState != NO_VALUE)
					item->Animation.TargetState = item->Animation.RequiredState;
				else if (AI.bite && AI.distance < APE_ATTACK_RANGE)
					item->Animation.TargetState = APE_STATE_ATTACK;
				else if (!(creatureInfo->Flags & APE_FLAG_ATTACK) &&
					AI.zoneNumber == AI.enemyZone && AI.ahead)
				{
					if (Random::TestProbability(APE_IDLE_JUMP_CHANCE))
						item->Animation.TargetState = APE_STATE_JUMP;
					else if (Random::TestProbability(APE_IDLE_POUND_CHEST_CHANCE))
						item->Animation.TargetState = APE_STATE_POUND_CHEST;
					else if (Random::TestProbability(APE_IDLE_POUND_GROUND_CHANCE))
						item->Animation.TargetState = APE_STATE_POUND_GROUND;
					else if (Random::TestProbability(APE_IDLE_RUN_LEFT_CHANCE))
					{
						item->Animation.TargetState = APE_STATE_RUN_LEFT;
						creatureInfo->MaxTurn = 0;
					}
					else
					{
						item->Animation.TargetState = APE_STATE_RUN_RIGHT;
						creatureInfo->MaxTurn = 0;
					}
				}
				else
					item->Animation.TargetState = APE_STATE_RUN_FORWARD;

				break;

			case APE_STATE_RUN_FORWARD:
				creatureInfo->MaxTurn = APE_RUN_TURN_RATE_MAX;

				if (!creatureInfo->Flags &&
					AI.angle > -APE_DISPLAY_ANGLE &&
					AI.angle < APE_DISPLAY_ANGLE)
				{
					item->Animation.TargetState = APE_STATE_IDLE;
				}
				else if (AI.ahead && item->TouchBits.Test(ApeAttackJoints))
				{
					item->Animation.RequiredState = APE_STATE_ATTACK;
					item->Animation.TargetState = APE_STATE_IDLE;
				}
				else if (creatureInfo->Mood != MoodType::Escape)
				{
					if (Random::TestProbability(APE_RUN_JUMP_CHANCE))
					{
						item->Animation.RequiredState = APE_STATE_JUMP;
						item->Animation.TargetState = APE_STATE_IDLE;
					}
					else if (Random::TestProbability(APE_RUN_POUND_CHEST_CHANCE))
					{
						item->Animation.RequiredState = APE_STATE_POUND_CHEST;
						item->Animation.TargetState = APE_STATE_IDLE;
					}
					else if (Random::TestProbability(APE_RUN_POUND_GROUND_CHANCE))
					{
						item->Animation.RequiredState = APE_STATE_POUND_GROUND;
						item->Animation.TargetState = APE_STATE_IDLE;
					}
				}

				break;

			case APE_STATE_RUN_LEFT:
				if (!(creatureInfo->Flags & APE_FLAG_TURN_RIGHT))
				{
					item->Pose.Orientation.y -= ANGLE(90.0f);
					creatureInfo->Flags |= APE_FLAG_TURN_RIGHT;
				}

				item->Animation.TargetState = APE_STATE_IDLE;
				break;

			case APE_STATE_RUN_RIGHT:
				if (!(creatureInfo->Flags & APE_FLAG_TURN_LEFT))
				{
					item->Pose.Orientation.y += ANGLE(90.0f);
					creatureInfo->Flags |= APE_FLAG_TURN_LEFT;
				}

				item->Animation.TargetState = APE_STATE_IDLE;
				break;

			case APE_STATE_ATTACK:
				if (item->Animation.RequiredState == NO_VALUE &&
					item->TouchBits.Test(ApeAttackJoints))
				{
					item->Animation.RequiredState = APE_STATE_IDLE;
					DoDamage(creatureInfo->Enemy, APE_ATTACK_DAMAGE);
					CreatureEffect(item, ApeBite, DoBloodSplat);
				}

				break;
			}
		}

		CreatureJoint(item, 0, head);

		if (item->Animation.ActiveState != APE_STATE_VAULT)
			ApeVault(itemNumber, angle);
		else
			CreatureAnimation(itemNumber, angle, 0);
	}
}
