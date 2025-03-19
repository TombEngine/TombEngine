#include "framework.h"
#include "Objects/TR4/Entity/tr4_knight_templar.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_item.h"
#include "Game/control/box.h"
#include "Game/effects/debris.h"
#include "Game/effects/effects.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Math;

namespace TEN::Entities::TR4
{
	constexpr auto KNIGHT_TEMPLAR_SWORD_ATTACK_DAMAGE = 120;

	constexpr auto KTEMPLAR_IDLE_TURN_RATE_MAX = ANGLE(2.0f);
	constexpr auto KTEMPLAR_WALK_TURN_RATE_MAX = ANGLE(7.0f);

	const auto KnightTemplarBite = CreatureBiteInfo(Vector3::Zero, 11);
	const auto KnightTemplarSwordAttackJoints = std::vector<unsigned int>{ 10, 11 };

	enum KnightTemplarState
	{
		// No state 0.
		KTEMPLAR_STATE_IDLE = 1,
		KTEMPLAR_STATE_WALK_FORWARD = 2,
		KTEMPLAR_STATE_SWORD_ATTACK_1 = 3,
		KTEMPLAR_STATE_SWORD_ATTACK_2 = 4,
		KTEMPLAR_STATE_SWORD_ATTACK_3 = 5,
		KTEMPLAR_STATE_SHIELD = 6,
		KTEMPLAR_STATE_SHIELD_HIT_1 = 7,
		KTEMPLAR_STATE_SHIELD_HIT_2 = 8
	};

	enum KnightTemplarAnim
	{
		KTEMPLAR_ANIM_WALK_FORWARD_LEFT_1 = 0,
		KTEMPLAR_ANIM_WALK_FORWARD_RIGHT_1 = 1,
		KTEMPLAR_ANIM_IDLE = 2,
		KTEMPLAR_ANIM_SWORD_ATTACK_1 = 3,
		KTEMPLAR_ANIM_SWORD_ATTACK_2 = 4,
		KTEMPLAR_ANIM_SWORD_ATTACK_3 = 5,
		KTEMPLAR_ANIM_SHIELD_START = 6,
		KTEMPLAR_ANIM_SHIELD_CONTINUE = 7,
		KTEMPLAR_ANIM_SHIELD_END = 8,
		KTEMPLAR_ANIM_SHIELD_HIT_1 = 9,
		KTEMPLAR_ANIM_SHIELD_HIT_2 = 10,
		KTEMPLAR_ANIM_WALK_FORWARD_LEFT_2 = 11,
		KTEMPLAR_ANIM_WALK_FORWARD_RIGHT_2 = 12
	};

	void InitializeKnightTemplar(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(*item, KTEMPLAR_ANIM_IDLE);
		item->MeshBits &= 0xF7FF;
	}

	void KnightTemplarControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* object = &Objects[item->ObjectNumber];
		auto* creature = GetCreatureInfo(item);

		short angle = 0;
		short tilt = 0;
		short joint0 = 0;
		short joint1 = 0;
		short joint2 = 0;

		if (item->Animation.AnimNumber == 0 ||
			item->Animation.AnimNumber == KTEMPLAR_ANIM_WALK_FORWARD_RIGHT_1 ||
			item->Animation.AnimNumber == KTEMPLAR_ANIM_WALK_FORWARD_LEFT_2 ||
			item->Animation.AnimNumber == KTEMPLAR_ANIM_WALK_FORWARD_RIGHT_2)
		{
			if (Random::TestProbability(1 / 2.0f))
			{
				auto color = Vector3(0.9f, 0.8f, 0.6f);
				auto pos = GetJointPosition(item, 10, Vector3i(0, 48, 448));
				TriggerMetalSparks(pos.x, pos.y, pos.z, (GetRandomControl() & 0x1FF) - 256, -128 - (GetRandomControl() & 0x7F), (GetRandomControl() & 0x1FF) - 256, color, 0);
			}
		}

		// Knight is immortal.
		if (item->HitPoints < object->HitPoints)
			item->HitPoints = object->HitPoints;

		if (item->AIBits)
			GetAITarget(creature);
		else if (creature->HurtByLara)
			creature->Enemy = LaraItem;

		AI_INFO AI;
		CreatureAIInfo(item, &AI);

		// TODO: Unused block.
		int a = 0;
		if (creature->Enemy != LaraItem)
			a = phd_atan(item->Pose.Position.z - LaraItem->Pose.Position.z, item->Pose.Position.x - LaraItem->Pose.Position.x);

		GetCreatureMood(item, &AI, true);
		CreatureMood(item, &AI, true);

		angle = CreatureTurn(item, creature->MaxTurn);

		if (AI.ahead)
		{
			joint0 = AI.angle / 2;
			joint1 = AI.xAngle;
			joint2 = AI.angle / 2;
		}

		int frameBase = 0;
		int frameNumber = 0;

		switch (item->Animation.ActiveState)
		{
		case KTEMPLAR_STATE_IDLE:
			item->Animation.TargetState = KTEMPLAR_STATE_WALK_FORWARD;
			creature->MaxTurn = KTEMPLAR_IDLE_TURN_RATE_MAX;
			creature->Flags = 0;

			if (AI.distance > SQUARE(BLOCK(2 / 3.0f)))
			{
				if (Lara.TargetEntity == item)
					item->Animation.TargetState = KTEMPLAR_STATE_SHIELD;
			}
			else if (Random::TestProbability(1 / 2.0f))
			{
				item->Animation.TargetState = KTEMPLAR_STATE_SWORD_ATTACK_2;
			}
			else if (Random::TestProbability(1 / 2.0f))
			{
				item->Animation.TargetState = KTEMPLAR_STATE_SWORD_ATTACK_1;
			}
			else
			{
				item->Animation.TargetState = KTEMPLAR_STATE_SWORD_ATTACK_3;
			}

			break;

		case KTEMPLAR_STATE_WALK_FORWARD:
			creature->MaxTurn = KTEMPLAR_WALK_TURN_RATE_MAX;

			if (Lara.TargetEntity == item || AI.distance <= SQUARE(BLOCK(2 / 3.0f)))
				item->Animation.TargetState = KTEMPLAR_STATE_IDLE;

			break;

		case KTEMPLAR_STATE_SWORD_ATTACK_1:
		case KTEMPLAR_STATE_SWORD_ATTACK_2:
		case KTEMPLAR_STATE_SWORD_ATTACK_3:
			creature->MaxTurn = 0;

			if (abs(AI.angle) >= ANGLE(1.0f))
			{
				if (AI.angle >= 0)
					item->Pose.Orientation.y += ANGLE(1.0f);
				else
					item->Pose.Orientation.y -= ANGLE(1.0f);
			}
			else
			{
				item->Pose.Orientation.y += AI.angle;
			}

			frameNumber = item->Animation.FrameNumber;
			frameBase = 0;
			if (frameNumber > (frameBase + 42) &&
				frameNumber < (frameBase + 51))
			{
				auto pos = GetJointPosition(item, LM_LINARM);
				
				auto& room = g_Level.Rooms[item->RoomNumber];
				auto& currentFloor = room.Sectors[(pos.z - room.Position.z) / BLOCK(1) + (pos.x - room.Position.x) / BLOCK(1) * room.ZSize];

				if (currentFloor.Stopper)
				{
					for (auto& mesh : room.mesh)
					{
						if (abs(pos.x - mesh.pos.Position.x) < BLOCK(1) &&
							abs(pos.z - mesh.pos.Position.z) < BLOCK(1) &&
							Statics[mesh.staticNumber].shatterType == ShatterType::None)
						{
							ShatterObject(nullptr, &mesh, -64, LaraItem->RoomNumber, 0);
							SoundEffect(SFX_TR4_SMASH_ROCK, &item->Pose);

							currentFloor.Stopper = false;

							TestTriggers(pos.x, pos.y, pos.z, item->RoomNumber, true);
						}
					}
				}

				if (!creature->Flags)
				{
					if (item->TouchBits.Test(KnightTemplarSwordAttackJoints))
					{
						DoDamage(creature->Enemy, KNIGHT_TEMPLAR_SWORD_ATTACK_DAMAGE);
						CreatureEffect2(item, KnightTemplarBite, 20, -1, DoBloodSplat);
						creature->Flags = 1;
					}
				}
			}

		case KTEMPLAR_STATE_SHIELD:
			creature->MaxTurn = 0;

			if (abs(AI.angle) >= ANGLE(1.0f))
			{
				if (AI.angle >= 0)
					item->Pose.Orientation.y += ANGLE(1.0f);
				else
					item->Pose.Orientation.y -= ANGLE(1.0f);
			}
			else
			{
				item->Pose.Orientation.y += AI.angle;
			}

			if (item->HitStatus)
			{
				if (Random::TestProbability(1 / 2.0f))
					item->Animation.TargetState = KTEMPLAR_STATE_SHIELD_HIT_1;
				else
					item->Animation.TargetState = KTEMPLAR_STATE_SHIELD_HIT_2;
			}
			else if (AI.distance <= SQUARE(BLOCK(2 / 3.0f)) || Lara.TargetEntity != item)
			{
				item->Animation.TargetState = KTEMPLAR_STATE_IDLE;
			}
			else
			{
				item->Animation.TargetState = KTEMPLAR_STATE_SHIELD;
			}

			break;

		default:
			break;
		}

		CreatureTilt(item, tilt);
		CreatureJoint(item, 0, joint0);
		CreatureJoint(item, 1, joint1);
		CreatureJoint(item, 2, joint2);
		CreatureAnimation(itemNumber, angle, tilt);
	}
}
