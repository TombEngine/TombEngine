#include "framework.h"
#include "Objects/TR3/Entity/Raptor.h"

#include "Game/collision/Point.h"
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

using namespace TEN::Collision::Point;
using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR3
{
	constexpr auto RAPTOR_ATTACK_DAMAGE = 100;

	constexpr auto RAPTOR_BITE_ATTACK_RANGE = SQUARE(BLOCK(0.6f));
	constexpr auto RAPTOR_JUMP_ATTACK_RANGE = SQUARE(BLOCK(1.5f));
	constexpr auto RAPTOR_RUN_ATTACK_RANGE	= SQUARE(BLOCK(1.5f));

	constexpr auto RAPTOR_ROAR_CHANCE		   = 1.0f / 256;
	constexpr auto RAPTOR_SWITCH_TARGET_CHANCE = 1.0f / 128;

	constexpr auto RAPTOR_WALK_TURN_RATE_MAX   = ANGLE(2.0f);
	constexpr auto RAPTOR_RUN_TURN_RATE_MAX	   = ANGLE(4.0f);
	constexpr auto RAPTOR_ATTACK_TURN_RATE_MAX = ANGLE(2.0f);

	const auto RaptorBite = CreatureBiteInfo(Vector3(0, 66, 318), 22);
	const auto RaptorAttackJoints = std::vector<unsigned int>{ 10, 11, 12, 13, 14, 16, 17, 18, 19, 20, 21, 22, 23 };

	enum RaptorState
	{
		RAPTOR_STATE_DEATH = 0,
		RAPTOR_STATE_IDLE = 1,
		RAPTOR_STATE_WALK_FORWARD = 2,
		RAPTOR_STATE_RUN_FORWARD = 3,
		RAPTOR_STATE_JUMP_ATTACK = 4,
		RAPTOR_STATE_NONE = 5,
		RAPTOR_STATE_ROAR = 6,
		RAPTOR_STATE_RUN_BITE_ATTACK = 7,
		RAPTOR_STATE_BITE_ATTACK = 8,
		RAPTOR_STATE_JUMP_START = 9,
		RAPTOR_STATE_JUMP_2_BLOCKS = 10,
		RAPTOR_STATE_JUMP_1_BLOCK = 11,
		RAPTOR_STATE_CLIMB = 12
	};

	enum RaptorAnim
	{
		RAPTOR_ANIM_IDLE = 0,
		RAPTOR_ANIM_RUN_FORWARD = 1,
		RAPTOR_ANIM_RUN_FORWARD_TO_IDLE = 2,
		RAPTOR_ANIM_IDLE_TO_RUN_FORWARD = 3,
		RAPTOR_ANIM_ROAR = 4,
		RAPTOR_ANIM_WALK_FORWARD = 5,
		RAPTOR_ANIM_WALK_FORWARD_TO_IDLE = 6,
		RAPTOR_ANIM_IDLE_TO_WALK_FORWARD = 7,
		RAPTOR_ANIM_RUN_BITE_ATTACK = 8,
		RAPTOR_ANIM_DEATH_1 = 9,
		RAPTOR_ANIM_DEATH_2 = 10,
		RAPTOR_ANIM_JUMP_ATTACK_START = 11,
		RAPTOR_ANIM_JUMP_ATTACK_END = 12,
		RAPTOR_ANIM_BITE_ATTACK = 13,
		RAPTOR_ANIM_JUMP_START = 14,
		RAPTOR_ANIM_JUMP_2_BLOCKS = 15,
		RAPTOR_ANIM_JUMP_END = 16,
		RAPTOR_ANIM_JUMP_1_BLOCK = 17,
		RAPTOR_ANIM_VAULT_2_STEPS = 18,
		RAPTOR_ANIM_VAULT_3_STEPS = 19,
		RAPTOR_ANIM_VAULT_4_STEPS = 20,
		RAPTOR_ANIM_VAULT_DROP_2_STEPS = 21,
		RAPTOR_ANIM_VAULT_DROP_3_STEPS = 22,
		RAPTOR_ANIM_VAULT_DROP_4_STEPS = 23
	};

	enum RaptorFlags
	{
		OCB_NORMAL_BEHAVIOUR = 0,
		OCB_ENABLE_JUMP = 1
	};

	const std::array RaptorDeathAnims = { RAPTOR_ANIM_DEATH_1, RAPTOR_ANIM_DEATH_2, };
	const std::vector<GAME_OBJECT_ID> RaptorIgnoredObjectIds = { ID_RAPTOR, ID_COMPSOGNATHUS };

	void RaptorControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto& item = g_Level.Items[itemNumber];
		auto& creature = *GetCreatureInfo(&item);

		short headingAngle = 0;
		short tiltAngle = 0;
		short headYOrient = 0;
		short neckYOrient = 0;

		bool canJump = item.TestOcb(OCB_ENABLE_JUMP);
		if (!canJump)
		{
			creature.LOT.Step = CLICK(1);
			creature.LOT.Drop = -CLICK(2);
			creature.LOT.Zone = ZoneType::Basic;
			creature.LOT.CanJump = false;
		}

		bool canJump1block = (canJump && CanCreatureJump(item, JumpDistance::Block1));
		bool canJump2blocks = (canJump && !canJump1block && CanCreatureJump(item, JumpDistance::Block2));

		// Require Idle state.
		if (item.HitPoints <= 0 && item.Animation.ActiveState == RAPTOR_STATE_IDLE)
		{
			if (item.Animation.ActiveState != RAPTOR_STATE_DEATH)
				SetAnimation(item, RaptorDeathAnims[Random::GenerateInt(0, (int)RaptorDeathAnims.size() - 1)]);
		}
		else
		{
			// NOTE: Ignores other small dinosaurs.
			TargetNearestEntity(item, RaptorIgnoredObjectIds);

			AI_INFO ai;
			if (item.AIBits)
				GetAITarget(&creature);
			CreatureAIInfo(&item, &ai);

			GetCreatureMood(&item, &ai, true);
			CreatureMood(&item, &ai, true);
			if (creature.Mood == MoodType::Bored)
				creature.MaxTurn /= 2;

			headingAngle = CreatureTurn(&item, creature.MaxTurn);
			if (ai.ahead)
			{
				headYOrient = ai.angle;
				neckYOrient = -headingAngle * 6;
			}

			switch (item.Animation.ActiveState)
			{
			case RAPTOR_STATE_IDLE:
				creature.MaxTurn = 0;
				creature.LOT.IsJumping = false;
				creature.Flags &= ~1;

				if (canJump1block || canJump2blocks)
				{
					creature.MaxTurn = 0;
					creature.LOT.IsJumping = true;
					SetAnimation(item, RAPTOR_ANIM_JUMP_START);

					if (canJump1block)
					{
						item.Animation.TargetState = RAPTOR_STATE_JUMP_1_BLOCK;
					}
					else
					{
						item.Animation.TargetState = RAPTOR_STATE_JUMP_2_BLOCKS;
					}
				}
				else if (item.Animation.RequiredState != NO_VALUE)
				{
					item.Animation.TargetState = item.Animation.RequiredState;
				}
				else if (creature.Flags & 2)
				{
					creature.Flags &= ~2;
					item.Animation.TargetState = RAPTOR_STATE_ROAR;
				}
				else if (item.TouchBits.Test(RaptorAttackJoints) ||
					(ai.distance < RAPTOR_BITE_ATTACK_RANGE && ai.bite))
				{
					item.Animation.TargetState = RAPTOR_STATE_BITE_ATTACK;
				}
				else if (ai.bite && ai.distance < RAPTOR_JUMP_ATTACK_RANGE)
				{
					item.Animation.TargetState = RAPTOR_STATE_JUMP_ATTACK;
				}
				else if (creature.Mood == MoodType::Escape &&
					Lara.TargetEntity != &item && ai.ahead && !item.HitStatus)
				{
					item.Animation.TargetState = RAPTOR_STATE_IDLE;
				}
				else if (creature.Mood == MoodType::Bored)
				{
					item.Animation.TargetState = RAPTOR_STATE_WALK_FORWARD;
				}
				else
				{
					item.Animation.TargetState = RAPTOR_STATE_RUN_FORWARD;
				}

				break;

			case RAPTOR_STATE_WALK_FORWARD:
				creature.MaxTurn = RAPTOR_WALK_TURN_RATE_MAX;
				creature.LOT.IsJumping = false;
				creature.Flags &= ~1;

				if (item.HitPoints <= 0)
				{
					item.Animation.TargetState = RAPTOR_STATE_IDLE;
				}
				else if (canJump1block || canJump2blocks)
				{
					creature.MaxTurn = 0;
					creature.LOT.IsJumping = true;
					SetAnimation(item, RAPTOR_ANIM_JUMP_START);

					if (canJump1block)
					{
						item.Animation.TargetState = RAPTOR_STATE_JUMP_1_BLOCK;
					}
					else
					{
						item.Animation.TargetState = RAPTOR_STATE_JUMP_2_BLOCKS;
					}
				}
				else if (creature.Mood != MoodType::Bored)
				{
					item.Animation.TargetState = RAPTOR_STATE_IDLE;
				}
				else if (ai.ahead && Random::TestProbability(RAPTOR_ROAR_CHANCE))
				{
					item.Animation.TargetState = RAPTOR_STATE_IDLE;
					item.Animation.RequiredState = RAPTOR_STATE_ROAR;
					creature.Flags &= ~2;
				}

				break;

			case RAPTOR_STATE_RUN_FORWARD:
				creature.MaxTurn = RAPTOR_RUN_TURN_RATE_MAX;
				creature.LOT.IsJumping = false;
				creature.Flags &= ~1;
				tiltAngle = headingAngle;

				if (item.HitPoints <= 0)
				{
					item.Animation.TargetState = RAPTOR_STATE_IDLE;
				}
				else if (canJump1block || canJump2blocks)
				{
					creature.MaxTurn = 0;
					creature.LOT.IsJumping = true;
					SetAnimation(item, RAPTOR_ANIM_JUMP_START);

					if (canJump1block)
					{
						item.Animation.TargetState = RAPTOR_STATE_JUMP_1_BLOCK;
					}
					else
					{
						item.Animation.TargetState = RAPTOR_STATE_JUMP_2_BLOCKS;
					}
				}
				else if (item.TouchBits.Test(RaptorAttackJoints))
				{
					item.Animation.TargetState = RAPTOR_STATE_IDLE;
				}
				else if (creature.Flags & 2)
				{
					item.Animation.TargetState = RAPTOR_STATE_IDLE;
					item.Animation.RequiredState = RAPTOR_STATE_ROAR;
					creature.Flags &= ~2;
				}
				else if (ai.bite && ai.distance < RAPTOR_RUN_ATTACK_RANGE)
				{
					if (Random::TestProbability(1 / 4.0f))
					{
						item.Animation.TargetState = RAPTOR_STATE_IDLE;
					}
					else
					{
						item.Animation.TargetState = RAPTOR_STATE_RUN_BITE_ATTACK;
					}
				}
				else if (ai.ahead && creature.Mood != MoodType::Escape &&
					Random::TestProbability(RAPTOR_ROAR_CHANCE))
				{
					item.Animation.TargetState = RAPTOR_STATE_IDLE;
					item.Animation.RequiredState = RAPTOR_STATE_ROAR;
				}
				else if (creature.Mood == MoodType::Bored ||
					(creature.Mood == MoodType::Escape && Lara.TargetEntity != &item && ai.ahead))
				{
					item.Animation.TargetState = RAPTOR_STATE_IDLE;
				}

				break;

			case RAPTOR_STATE_JUMP_ATTACK:
				creature.MaxTurn = RAPTOR_ATTACK_TURN_RATE_MAX;
				tiltAngle = headingAngle;

				if (creature.Enemy != nullptr)
				{
					if (creature.Enemy->IsLara() && !(creature.Flags & 1) &&
						item.TouchBits.Test(RaptorAttackJoints))
					{
						DoDamage(creature.Enemy, RAPTOR_ATTACK_DAMAGE);
						CreatureEffect(&item, RaptorBite, DoBloodSplat);
						creature.Flags |= 1;

						if (LaraItem->HitPoints <= 0)
							creature.Flags |= 2;

						item.Animation.RequiredState = RAPTOR_STATE_IDLE;
					}
					else if (!(creature.Flags & 1))
					{
						if (Vector3i::Distance(item.Pose.Position, creature.Enemy->Pose.Position) <= BLOCK(0.5f))
						{
							if (creature.Enemy->HitPoints <= 0)
								creature.Flags |= 2;

							DoDamage(creature.Enemy, 25);
							CreatureEffect(&item, RaptorBite, DoBloodSplat);
							creature.Flags |= 1;
						}
					}
				}

				break;

			case RAPTOR_STATE_BITE_ATTACK:
				creature.MaxTurn = RAPTOR_ATTACK_TURN_RATE_MAX;
				tiltAngle = headingAngle;

				if (creature.Enemy != nullptr)
				{
					if (creature.Enemy->IsLara() && !(creature.Flags & 1) &&
						item.TouchBits.Test(RaptorAttackJoints))
					{
						DoDamage(creature.Enemy, RAPTOR_ATTACK_DAMAGE);
						CreatureEffect(&item, RaptorBite, DoBloodSplat);
						creature.Flags |= 1;

						if (LaraItem->HitPoints <= 0)
							creature.Flags |= 2;

						item.Animation.RequiredState = RAPTOR_STATE_IDLE;
					}
					else if (!(creature.Flags & 1))
					{
						if (Vector3i::Distance(item.Pose.Position, creature.Enemy->Pose.Position) <= BLOCK(0.5f))
						{
							if (creature.Enemy->HitPoints <= 0)
								creature.Flags |= 2;

							DoDamage(creature.Enemy, 25);
							CreatureEffect(&item, RaptorBite, DoBloodSplat);
							creature.Flags |= 1;
						}
					}
				}

				break;

			case RAPTOR_STATE_RUN_BITE_ATTACK:
				creature.MaxTurn = RAPTOR_ATTACK_TURN_RATE_MAX;
				tiltAngle = headingAngle;

				if (creature.Enemy != nullptr)
				{
					if (creature.Enemy->IsLara() && !(creature.Flags & 1) &&
						item.TouchBits.Test(RaptorAttackJoints))
					{
						DoDamage(creature.Enemy, RAPTOR_ATTACK_DAMAGE);
						CreatureEffect(&item, RaptorBite, DoBloodSplat);
						item.Animation.RequiredState = RAPTOR_STATE_RUN_FORWARD;
						creature.Flags |= 1;

						if (LaraItem->HitPoints <= 0)
							creature.Flags |= 2;
					}
					else if (!(creature.Flags & 1))
					{
						if (Vector3i::Distance(item.Pose.Position, creature.Enemy->Pose.Position) <= BLOCK(0.5f))
						{
							if (creature.Enemy->HitPoints <= 0)
								creature.Flags |= 2;

							DoDamage(creature.Enemy, 25);
							CreatureEffect(&item, RaptorBite, DoBloodSplat);
							creature.Flags |= 1;
						}
					}
				}

				break;
			}
		}

		CreatureTilt(&item, tiltAngle);
		CreatureJoint(&item, 0, headYOrient / 2);
		CreatureJoint(&item, 1, headYOrient / 2);
		CreatureJoint(&item, 2, neckYOrient);
		CreatureJoint(&item, 3, neckYOrient);

		if (item.Animation.ActiveState != RAPTOR_STATE_JUMP_2_BLOCKS &&
			item.Animation.ActiveState != RAPTOR_STATE_JUMP_1_BLOCK &&
			item.Animation.ActiveState != RAPTOR_STATE_CLIMB &&
			item.Animation.ActiveState != RAPTOR_STATE_JUMP_START)
		{
			switch (CreatureVault(itemNumber, headingAngle, 2, CLICK(2.5f)))
			{
			case 2:
				SetAnimation(item, RAPTOR_ANIM_VAULT_2_STEPS);
				creature.MaxTurn = 0;
				break;

			case 3:
				SetAnimation(item, RAPTOR_ANIM_VAULT_3_STEPS);
				creature.MaxTurn = 0;
				break;

			case 4:
				SetAnimation(item, RAPTOR_ANIM_VAULT_4_STEPS);
				creature.MaxTurn = 0;
				break;

			case -2:
				SetAnimation(item, RAPTOR_ANIM_VAULT_DROP_2_STEPS);
				creature.MaxTurn = 0;
				break;

			case -3:
				SetAnimation(item, RAPTOR_ANIM_VAULT_DROP_3_STEPS);
				creature.MaxTurn = 0;
				break;

			case -4:
				SetAnimation(item, RAPTOR_ANIM_VAULT_DROP_4_STEPS);
				creature.MaxTurn = 0;
				break;
			}
		}
		else
		{
			CreatureAnimation(itemNumber, headingAngle, tiltAngle);
		}
	}
}
