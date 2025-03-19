#include "framework.h"
#include "Objects/TR4/Entity/tr4_harpy.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_room.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/control/lot.h"
#include "Game/effects/effects.h"
#include "Game/effects/spark.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Renderer/RendererEnums.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Math;
using namespace TEN::Math::Random;
using namespace TEN::Effects::Spark;
using std::vector;

namespace TEN::Entities::TR4
{
	constexpr auto HARPY_STINGER_ATTACK_DAMAGE	= 100;
	constexpr auto HARPY_SWOOP_ATTACK_DAMAGE	= 10;
	constexpr auto HARPY_STINGER_POISON_POTENCY = 8;

	const auto HarpyBite1	= CreatureBiteInfo(Vector3::Zero, 4);
	const auto HarpyBite2	= CreatureBiteInfo(Vector3::Zero, 2);
	const auto HarpyBite3	= CreatureBiteInfo(Vector3::Zero, 15);
	const auto HarpyAttack1 = CreatureBiteInfo(Vector3(0, 128, 0), 2);
	const auto HarpyAttack2 = CreatureBiteInfo(Vector3(0, 128, 0), 4);
	const auto HarpySwoopAttackJoints   = std::vector<unsigned int>{ 2, 4, 15 };
	const auto HarpyStingerAttackJoints = std::vector<unsigned int>{ 2, 4 };

	enum HarpyState
	{
		// No state 0.
		HARPY_STATE_IDLE = 1,
		HARPY_STATE_FLY_FORWARD = 2,
		HARPY_STATE_FLY_DOWN = 3,
		HARPY_STATE_FLY_FORWARD_DOWN = 4,
		HARPY_STATE_SWOOP_ATTACK = 5,
		HARPY_STATE_STINGER_ATTACK = 6,
		HARPY_STATE_FLY_FORWARD_SPIN = 7,
		HARPY_STATE_FLAME_ATTACK = 8,
		HARPY_STATE_DEATH_START = 9,
		HARPY_STATE_DEATH_FALL = 10,
		HARPY_STATE_DEATH_END = 11,
		HARPY_STATE_FLY_BACK = 12,
		HARPY_STATE_GLIDE = 13
	};

	enum HarpyAnim
	{
		HARPY_ANIM_FLY_FORWARD = 0,
		HARPY_ANIM_FLAME_ATTACK_START = 1,
		HARPY_ANIM_FLAME_ATTACK_CONTINUE = 2,
		HARPY_ANIM_FLAME_ATTACK_END = 3,
		HARPY_ANIM_IDLE = 4,
		HARPY_ANIM_DEATH_START = 5,
		HARPY_ANIM_DEATH_FALL = 6,
		HARPY_ANIM_DEATH_END = 7,
		HARPY_ANIM_STINGER_ATTACK = 8,
		HARPY_ANIM_FLY_BACK = 9,
		HARPY_ANIM_FLY_FORWARD_DOWN_START = 10,
		HARPY_ANIM_FLY_FORWARD_DOWN_CONTINUE = 11,
		HARPY_ANIM_FLY_FORWARD_DOWN_END = 12,
		HARPY_ANIM_SWOOP_ATTACK = 13,
		HARPY_ANIM_FLY_DOWN_START = 14,
		HARPY_ANIM_FLY_DOWN_CONTINUE = 15,
		HARPY_ANIM_FLY_DOWN_END = 16,
		HARPY_ANIM_FLY_FORWARD_SPIN = 17,
		HARPY_ANIM_GLIDE = 18
	};

	void TriggerHarpyMissile(Pose* pose, short roomNumber, short mesh)
	{
		short fxNumber = CreateNewEffect(roomNumber);
		if (fxNumber == -1)
			return;

		auto* fx = &EffectList[fxNumber];

		fx->pos.Position.x = pose->Position.x;
		fx->pos.Position.y = pose->Position.y - (GetRandomControl() & 0x3F) - 32;
		fx->pos.Position.z = pose->Position.z;
		fx->pos.Orientation.x = pose->Orientation.x;
		fx->pos.Orientation.y = pose->Orientation.y;
		fx->pos.Orientation.z = 0;
		fx->roomNumber = roomNumber;
		fx->counter = short(2 * GetRandomControl() + 0x8000);
		fx->objectNumber = ID_ENERGY_BUBBLES;
		fx->speed = (GetRandomControl() & 0x1F) + 96;
		fx->flag1 = mesh;
		fx->frameNumber = Objects[fx->objectNumber].meshIndex + mesh * 2;
	}

	void DoHarpyEffects(ItemInfo* item, CreatureInfo* creature, short itemNumber)
	{
		item->ItemFlags[0]++;

		auto rh = GetJointPosition(item, HarpyAttack1);
		auto lr = GetJointPosition(item, HarpyAttack2);

		int sG = (GetRandomControl() & 0x7F) + 32;
		int sR = sG;
		int sB = 0;
		auto sparkColor = Vector3(sR, sG, sB);
		int fG = (GetRandomControl() & 0x7F) + 64;
		int fR = fG;
		int fB = 0;
		auto flameColor = Vector3(fR, fG, fB);

		if (item->ItemFlags[0] >= 24 &&
			item->ItemFlags[0] <= 47 &&
			(GetRandomControl() & 0x1F) < item->ItemFlags[0])
		{
			for (int i = 0; i < 2; i++)
			{
				TriggerAttackSpark(lr.ToVector3(), sparkColor);
				TriggerAttackSpark(rh.ToVector3(), sparkColor);
			}
		}

		int size = item->ItemFlags[0] * 2;
		if (size > 64)
			size = 64;
		if (size < 80)
		{
			if ((Wibble & 0xF) == 8)
			{
				TriggerAttackFlame(lr, flameColor, size);
				TriggerAttackFlame(rh, flameColor, size);
			}
			else if (!(Wibble & 0xF))
			{
				TriggerAttackFlame(lr, flameColor, size);
				TriggerAttackFlame(rh, flameColor, size);
			}
		}

		if (item->ItemFlags[0] >= 61)
		{
			if (item->ItemFlags[0] <= 65 && GlobalCounter & 1)
			{
				auto pos3 = GetJointPosition(item, HarpyAttack1.BoneID, Vector3i(HarpyAttack1.Position.x, HarpyAttack1.Position.y * 2, HarpyAttack1.Position.z));
				auto orient = Geometry::GetOrientToPoint(lr.ToVector3(), rh.ToVector3());
				auto pose = Pose(rh, orient);
				TriggerHarpyMissile(&pose, item->RoomNumber, 2);
			}

			if (item->ItemFlags[0] >= 61 && item->ItemFlags[0] <= 65 && !(GlobalCounter & 1))
			{
				auto pos3 = GetJointPosition(item, HarpyAttack2.BoneID, Vector3i(HarpyAttack2.Position.x, HarpyAttack2.Position.y * 2, HarpyAttack2.Position.z));
				auto orient = Geometry::GetOrientToPoint(lr.ToVector3(), rh.ToVector3());
				auto pose = Pose(rh, orient);
				TriggerHarpyMissile(&pose, item->RoomNumber, 2);
			}
		}
	}

	void InitializeHarpy(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];
		InitializeCreature(itemNumber);
		SetAnimation(*item, HARPY_ANIM_IDLE);
	}

	void HarpyControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short angle = 0;
		short joint0 = 0;
		short joint1 = 0;
		short joint2 = 0;

		if (item->HitPoints <= 0)
		{
			item->HitPoints = 0;

			int state = item->Animation.ActiveState - 9;
			if (state)
			{
				state--;
				if (state)
				{
					if (state == HARPY_STATE_IDLE)
					{
						item->Pose.Position.y = item->Floor;
						item->Pose.Orientation.x = 0;
					}
					else
					{
						SetAnimation(*item, HARPY_ANIM_DEATH_START);
						item->Animation.IsAirborne = true;
						item->Animation.Velocity.z = 0;
						item->Pose.Orientation.x = 0;
					}

					CreatureTilt(item, 0);

					CreatureJoint(item, 0, joint0);
					CreatureJoint(item, 1, joint1);
					CreatureJoint(item, 2, joint2);

					CreatureAnimation(itemNumber, angle, 0);
					return;
				}
			}
			else
				item->Animation.TargetState = HARPY_STATE_DEATH_FALL;

			if (item->Pose.Position.y >= item->Floor)
			{
				item->Animation.TargetState = HARPY_STATE_DEATH_END;
				item->Animation.IsAirborne = false;
				item->Animation.Velocity.y = 0.0f;
				item->Pose.Position.y = item->Floor;
				AlignEntityToSurface(item, Vector2(Objects[item->ObjectNumber].radius));
			}

			item->Pose.Orientation.x = 0;
		}
		else
		{
			if (item->AIBits)
				GetAITarget(creature);

			int minDistance = INT_MAX;

			creature->Enemy = nullptr;

			for (auto& currentCreature : ActiveCreatures)
			{
				if (currentCreature->ItemNumber == NO_VALUE || currentCreature->ItemNumber == itemNumber)
					continue;

				auto* target = &g_Level.Items[currentCreature->ItemNumber];

				if (target->ObjectNumber == ID_LARA_DOUBLE)
				{
					int dx = target->Pose.Position.x - item->Pose.Position.x;
					int dz = target->Pose.Position.z - item->Pose.Position.z;
					int distance = SQUARE(dx) + SQUARE(dz);

					if (distance < minDistance)
					{
						creature->Enemy = target;
						minDistance = distance;
					}
				}
			}

			AI_INFO AI;
			CreatureAIInfo(item, &AI);

			if (!creature->Enemy->IsLara())
				phd_atan(LaraItem->Pose.Position.z - item->Pose.Position.z, LaraItem->Pose.Position.x - item->Pose.Position.x);

			GetCreatureMood(item, &AI, true);
			CreatureMood(item, &AI, true);

			angle = CreatureTurn(item, creature->MaxTurn);

			if (AI.ahead)
			{
				joint0 = AI.angle / 2;
				joint2 = AI.angle / 2;
				joint1 = AI.xAngle;
			}

			int height = 0;
			int dy = 0;

			switch (item->Animation.ActiveState)
			{
			case HARPY_STATE_IDLE:
				creature->MaxTurn = ANGLE(7.0f);
				creature->Flags = 0;

				if (creature->Enemy)
				{
					height = (item->Pose.Position.y + BLOCK(2));
					if (creature->Enemy->Pose.Position.y > height && item->Floor > height)
					{
						item->Animation.TargetState = HARPY_STATE_FLY_DOWN;
						break;
					}
				}

				if (AI.ahead)
				{
					dy = abs(creature->Enemy->Pose.Position.y - item->Pose.Position.y);
					if (dy <= BLOCK(1))
					{
						if (AI.distance < SQUARE(341))
						{
							item->Animation.TargetState = HARPY_STATE_STINGER_ATTACK;
							break;
						}

						if (dy <= BLOCK(1) && AI.distance < SQUARE(BLOCK(2)))
						{
							item->Animation.TargetState = HARPY_STATE_FLY_FORWARD_DOWN;
							break;
						}
					}
				}

				if (creature->Enemy != LaraItem ||
					!Targetable(item, &AI) ||
					AI.distance <= SQUARE(BLOCK(3.5f)) ||
					Random::TestProbability(1 / 2.0f))
				{
					item->Animation.TargetState = HARPY_STATE_FLY_FORWARD;
					break;
				}

				item->Animation.TargetState = HARPY_STATE_FLAME_ATTACK;
				item->ItemFlags[0] = 0;
				break;

			case HARPY_STATE_FLY_FORWARD:
				creature->MaxTurn = ANGLE(7.0f);
				creature->Flags = 0;

				if (item->Animation.RequiredState != NO_VALUE)
				{
					item->Animation.TargetState = item->Animation.RequiredState;
					if (item->Animation.RequiredState == HARPY_STATE_FLAME_ATTACK)
						item->ItemFlags[0] = 0;

					break;
				}

				if (item->HitStatus)
				{
					item->Animation.TargetState = HARPY_STATE_FLY_FORWARD_SPIN;
					break;
				}

				if (AI.ahead)
				{
					if (AI.distance >= SQUARE(341))
					{
						if (AI.ahead && Random::TestProbability(1 / 2.0f) &&
							AI.distance >= SQUARE(BLOCK(2)) &&
							AI.distance > SQUARE(BLOCK(3.5f)))
						{
							item->Animation.TargetState = HARPY_STATE_FLAME_ATTACK;
							item->ItemFlags[0] = 0;
						}
						else
							item->Animation.TargetState = HARPY_STATE_FLY_FORWARD_DOWN;
					}
					else
						item->Animation.TargetState = HARPY_STATE_STINGER_ATTACK;

					break;
				}

				if (Random::TestProbability(1 / 2.0f))
				{
					item->Animation.TargetState = HARPY_STATE_FLY_FORWARD_SPIN;
					break;
				}

				if (!AI.ahead)
				{
					item->Animation.TargetState = HARPY_STATE_FLY_FORWARD_DOWN;
					break;
				}

				if (AI.distance >= SQUARE(341))
				{
					if (AI.ahead && AI.distance >= SQUARE(BLOCK(2)) &&
						AI.distance > SQUARE(BLOCK(3.5f)) &&
						Random::TestProbability(1 / 2.0f))
					{
						item->Animation.TargetState = HARPY_STATE_FLAME_ATTACK;
						item->ItemFlags[0] = 0;
					}
					else
						item->Animation.TargetState = HARPY_STATE_FLY_FORWARD_DOWN;
				}
				else
					item->Animation.TargetState = HARPY_STATE_STINGER_ATTACK;

				break;

			case HARPY_STATE_FLY_DOWN:
				if (!creature->Enemy ||
					creature->Enemy->Pose.Position.y < (item->Pose.Position.y + BLOCK(2)))
				{
					item->Animation.TargetState = HARPY_STATE_IDLE;
				}

				break;

			case HARPY_STATE_FLY_FORWARD_DOWN:
				creature->MaxTurn = ANGLE(2.0f);

				if (AI.ahead && AI.distance < SQUARE(BLOCK(2)))
					item->Animation.TargetState = HARPY_STATE_SWOOP_ATTACK;
				else
					item->Animation.TargetState = HARPY_STATE_GLIDE;

				break;

			case HARPY_STATE_SWOOP_ATTACK:
				item->Animation.TargetState = HARPY_STATE_FLY_FORWARD;
				creature->MaxTurn = ANGLE(2.0f);

				if (item->TouchBits.Test(HarpySwoopAttackJoints) ||
					creature->Enemy != nullptr && !creature->Enemy->IsLara() &&
					abs(creature->Enemy->Pose.Position.y - item->Pose.Position.y) <= BLOCK(1) &&
					AI.distance < SQUARE(BLOCK(2)))
				{
					DoDamage(creature->Enemy, HARPY_SWOOP_ATTACK_DAMAGE);

					if (item->TouchBits & 0x10)
						CreatureEffect2(item, HarpyBite1, 5, -1, DoBloodSplat);
					else
						CreatureEffect2(item, HarpyBite2, 5, -1, DoBloodSplat);
				}

				break;

			case HARPY_STATE_STINGER_ATTACK:
				creature->MaxTurn = ANGLE(2.0f);

				if (creature->Flags == 0 &&
						(item->TouchBits.Test(HarpyStingerAttackJoints) ||
						creature->Enemy != nullptr &&
						abs(creature->Enemy->Pose.Position.y - item->Pose.Position.y) <= BLOCK(1) &&
						AI.distance < SQUARE(BLOCK(2)) &&
						item->Animation.AnimNumber == HARPY_ANIM_STINGER_ATTACK &&
						item->Animation.FrameNumber > 17)
					)
				{
					if (creature->Enemy->IsLara())
						GetLaraInfo(creature->Enemy)->Status.Poison += HARPY_STINGER_POISON_POTENCY;

					DoDamage(creature->Enemy, HARPY_STINGER_ATTACK_DAMAGE);
					CreatureEffect2(item, HarpyBite3, 10, -1, DoBloodSplat);
					creature->Flags = 1;
				}

				break;

			case HARPY_STATE_FLAME_ATTACK:
				DoHarpyEffects(item, creature, itemNumber);
				break;

			case HARPY_STATE_FLY_BACK:
				if (AI.ahead && AI.distance > SQUARE(BLOCK(3.5f)))
				{
					item->Animation.TargetState = HARPY_STATE_FLY_FORWARD;
					item->Animation.RequiredState = HARPY_STATE_FLAME_ATTACK;
				}
				else if (Random::TestProbability(1 / 2.0f))
					item->Animation.TargetState = HARPY_STATE_IDLE;

				break;

			case HARPY_STATE_GLIDE:
				item->Animation.TargetState = HARPY_STATE_FLY_FORWARD;
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
