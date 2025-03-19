#include "framework.h"
#include "Objects/TR4/Entity/tr4_mutant.h"

#include "Game/Animation/Animation.h"
#include "Game/control/control.h"
#include "Game/control/lot.h"
#include "Game/effects/effects.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Objects/Effects/tr4_locusts.h"
#include "Objects/objectslist.h"
#include "Renderer/RendererEnums.h"

using namespace TEN::Animation;
using namespace TEN::Math;

namespace TEN::Entities::TR4
{
	constexpr auto MUTANT_PROJECTILE_ATTACK_RANGE = SQUARE(BLOCK(10));
	constexpr auto MUTANT_LOCUST_ATTACK_1_RANGE	  = SQUARE(BLOCK(15));
	constexpr auto MUTANT_LOCUST_ATTACK_2_RANGE   = SQUARE(BLOCK(30));

	enum MutantState
	{
		MUTANT_STATE_NONE = 0,
		MUTANT_STATE_APPEAR = 1,
		MUTANT_STATE_IDLE = 2,
		MUTANT_STATE_PROJECTILE_ATTACK = 3,
		MUTANT_STATE_LOCUST_ATTACK_1 = 4,
		MUTANT_STATE_LOCUST_ATTACK_2 = 5,
	};

	enum MutantAnim
	{
		MUTANT_ANIM_APPEAR = 0,
		MUTANT_ANIM_IDLE = 1,
		MUTANT_ANIM_PROJECTILE_ATTACK = 2,
		MUTANT_ANIM_LOCUST_ATTACK_1 = 3,
		MUTANT_ANIM_LOCUST_ATTACK_1_TO_PROJECTILE_ATTACK = 4,
		MUTANT_ANIM_LOCUST_ATTACK_2 = 5
	};

	enum class MissileRotationType
	{
		Front,
		Left,
		Right
	};

	enum CARDINAL_POINT
	{
		C_NORTH = 0,
		C_NORTH_EAST = 45,
		C_EAST = 90,
		C_EAST_SOUTH = 135,
		C_SOUTH = 180,
		C_SOUTH_WEST = 225,
		C_WEST = 270,
		C_WEST_NORTH = 315
	};

	void TriggerCrocgodMissile(Pose* src, short roomNumber, short counter)
	{
		short fxNumber = NO_VALUE;

		fxNumber = CreateNewEffect(roomNumber);
		if (fxNumber != NO_VALUE)
		{
			auto* fx = &EffectList[fxNumber];
			fx->pos.Position.x = src->Position.x;
			fx->pos.Position.y = src->Position.y - (GetRandomControl() & 0x3F) - 32;
			fx->pos.Position.z = src->Position.z;
			fx->pos.Orientation.x = src->Orientation.x;
			fx->pos.Orientation.y = src->Orientation.y;
			fx->pos.Orientation.z = 0;
			fx->roomNumber = roomNumber;
			fx->counter = 16 * counter + 15;
			fx->objectNumber = ID_ENERGY_BUBBLES;
			fx->frameNumber = Objects[fx->objectNumber].meshIndex + 5;
			fx->speed = (GetRandomControl() & 0x1F) + 96;
			fx->flag1 = 6;
		}
	}

	void TriggerCrocgodMissileFlame(short fxNumber, short xVel, short yVel, short zVel)
	{
		//x = LaraItem->pos.Position.x - Effects[m_fxNumber].pos.Position.x;
		//z = LaraItem->pos.Position.z - Effects[m_fxNumber].pos.Position.z;
		//if (x >= -0x4000u && x <= 0x4000 && z >= -0x4000u && z <= 0x4000)

		auto* fx = &EffectList[fxNumber];
		auto* sptr = GetFreeParticle();

		sptr->on = true;
		BYTE color = (GetRandomControl() & 0x3F) - 128;
		sptr->sB = 0;
		sptr->sR = color;
		sptr->sG = color / 2;
		color = (GetRandomControl() & 0x3F) - 128;
		sptr->dB = 0;
		sptr->dR = color;
		sptr->dG = color / 2;
		sptr->fadeToBlack = 8;
		sptr->colFadeSpeed = (GetRandomControl() & 3) + 8;
		sptr->blendMode = BlendMode::Additive;
		sptr->dynamic = -1;
		BYTE life = (GetRandomControl() & 7) + 32;
		sptr->life = life;
		sptr->sLife = life;
		sptr->x = (GetRandomControl() & 0xF) - 8;
		sptr->y = 0;
		sptr->z = (GetRandomControl() & 0xF) - 8;
		sptr->x += fx->pos.Position.x;
		sptr->y += fx->pos.Position.y;
		sptr->z += fx->pos.Position.z;
		sptr->xVel = xVel;
		sptr->yVel = yVel;
		sptr->zVel = zVel;
		sptr->friction = 34;
		sptr->flags = SP_EXPDEF | SP_ROTATE | SP_DEF | SP_SCALE;
		sptr->rotAng = GetRandomControl() & 0xFFF;

		if (Random::TestProbability(1 / 2.0f))
			sptr->rotAdd = (GetRandomControl() & 0x1F) - 32;
		else
			sptr->rotAdd = (GetRandomControl() & 0x1F) + 32;

		sptr->gravity = 0;
		sptr->maxYvel = 0;
		sptr->fxObj = byte(fxNumber);
		sptr->scalar = 2;
		BYTE size = (GetRandomControl() & 0xF) + 128;
		sptr->size = size;
		sptr->sSize = size;
		sptr->dSize = size / 4;
	}

	void ShootFireball(Pose* src, MissileRotationType rotationType, short roomNumber, int timer)
	{
		switch (rotationType)
		{
		case MissileRotationType::Left:
			src->Orientation.y -= GetRandomControl() % 0x2000;
			break;

		case MissileRotationType::Right:
			src->Orientation.y += GetRandomControl() % 0x2000;
			break;
		}

		TriggerCrocgodMissile(src, roomNumber, timer);
	}

	bool ShootFrame(ItemInfo* item)
	{
		int frameNumber = item->Animation.FrameNumber;
		if (frameNumber == 45 ||
			/*frameNumber == 50 ||
			frameNumber == 55 ||*/
			frameNumber == 60 ||
			/*frameNumber == 65 ||
			frameNumber == 70 ||*/
			frameNumber == 75)
		{
			return true;
		}
		else
			return false;
	}

	static void RotateHeadToTarget(ItemInfo* item, CreatureInfo* creature, int joint, short& headAngle)
	{
		if (creature->Enemy == nullptr)
		{
			headAngle = item->Pose.Orientation.y;
			return;
		}

		auto* enemy = creature->Enemy;

		auto pos = GetJointPosition(item,joint);
		int x = enemy->Pose.Position.x - pos.x;
		int z = enemy->Pose.Position.z - pos.z;
		headAngle = (short)(phd_atan(z, x) - item->Pose.Orientation.y) / 2;
	}

	void GetTargetPosition(ItemInfo* originEntity, Pose* targetPose)
	{
		auto origin = GetJointPosition(originEntity, 9, Vector3i(0, -96, 144));
		auto target = GetJointPosition(originEntity, 9, Vector3i(0, -128, 288));
		auto orient = Geometry::GetOrientToPoint(origin.ToVector3(), target.ToVector3());

		targetPose->Position = target;
		targetPose->Orientation = orient;
	}

	void MoveItemFront(ItemInfo* item, int distance)
	{
		short angle = short(TO_DEGREES(item->Pose.Orientation.y));
		switch (angle)
		{
		case C_NORTH:
			item->Pose.Position.z += distance;
			break;

		case C_EAST:
			item->Pose.Position.x += distance;
			break;

		case C_SOUTH:
			item->Pose.Position.z -= distance;
			break;

		case C_WEST:
			item->Pose.Position.x -= distance;
			break;
		}
	}

	void MoveItemBack(ItemInfo* item, int distance)
	{
		short angle = short(TO_DEGREES(item->Pose.Orientation.y));
		switch (angle)
		{
		case C_NORTH:
			item->Pose.Position.z -= distance;
			break;

		case C_EAST:
			item->Pose.Position.x -= distance;
			break;

		case C_SOUTH:
			item->Pose.Position.z += distance;
			break;

		case C_WEST:
			item->Pose.Position.x += distance;
			break;
		}
	}

	void MutantAIFix(ItemInfo* item, AI_INFO* AI)
	{
		MoveItemFront(item, BLOCK(2));
		item->Pose.Position.y -= CLICK(3);
		CreatureAIInfo(item, AI);
		item->Pose.Position.y += CLICK(3);
		MoveItemBack(item, BLOCK(2));
	}

	void InitializeCrocgod(short itemNumber)
	{
		InitializeCreature(itemNumber);

		auto* item = &g_Level.Items[itemNumber];
		item->Animation.AnimNumber = MUTANT_ANIM_APPEAR;
		item->Animation.FrameNumber = 0;
		item->Animation.ActiveState = MUTANT_STATE_APPEAR;
		item->Animation.TargetState = MUTANT_STATE_APPEAR;
	}

	void CrocgodControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short headingAngle = 0;
		auto extraHeadRot = EulerAngles::Identity;
		auto extraTorsoRot = EulerAngles::Identity;

		int frameNumber;

		if (item->AIBits & ALL_AIOBJ)
		{
			GetAITarget(creature);
		}
		else if (creature->HurtByLara)
		{
			creature->Enemy = LaraItem;
		}
		else
		{
			TargetNearestEntity(*item);
		}

		AI_INFO ai;
		MutantAIFix(item, &ai);

		RotateHeadToTarget(item, creature, 9, extraHeadRot.y);
		GetCreatureMood(item, &ai, true);
		CreatureMood(item, &ai, true);

		creature->MaxTurn = 0;
		headingAngle = CreatureTurn(item, 0);

		switch (item->Animation.ActiveState)
		{
		case MUTANT_STATE_IDLE:
			if (ai.ahead)
			{
				if (Random::TestProbability(0.28f) && ai.distance <= MUTANT_PROJECTILE_ATTACK_RANGE)
					item->Animation.TargetState = MUTANT_STATE_PROJECTILE_ATTACK;
				else if (Random::TestProbability(0.28f) && ai.distance <= MUTANT_LOCUST_ATTACK_1_RANGE)
					item->Animation.TargetState = MUTANT_STATE_LOCUST_ATTACK_1;
				else if (Random::TestProbability(0.28f) && ai.distance <= MUTANT_LOCUST_ATTACK_2_RANGE)
					item->Animation.TargetState = MUTANT_STATE_LOCUST_ATTACK_2;
			}

			break;

		case MUTANT_STATE_PROJECTILE_ATTACK:
			frameNumber = item->Animation.FrameNumber;
			if (frameNumber >= 94 && frameNumber <= 96)
			{
				Pose src;
				GetTargetPosition(item, &src);

				if (frameNumber == 94)
					ShootFireball(&src, MissileRotationType::Front, item->RoomNumber, 0);
				else if (frameNumber == 95)
				{
					ShootFireball(&src, MissileRotationType::Left, item->RoomNumber, 1);
					//ShootFireball(&src, MissileRotationType::M_LEFT, item->roomNumber, 1);
				}
				else if (frameNumber == 96)
				{
					ShootFireball(&src, MissileRotationType::Right, item->RoomNumber, 1);
					//ShootFireball(&src, MissileRotationType::M_RIGHT, item->roomNumber, 1);
				}
			}

			break;

		case MUTANT_STATE_LOCUST_ATTACK_1:
			frameNumber = (item->Animation.FrameNumber);
			if (frameNumber >= 60 && frameNumber <= 120)
				SpawnLocust(item);

			break;

		case MUTANT_STATE_LOCUST_ATTACK_2:
			if (ShootFrame(item))
			{
				Pose src;
				GetTargetPosition(item, &src);
				ShootFireball(&src, MissileRotationType::Front, item->RoomNumber, 1);
			}

			break;
		}

		if (item->Animation.ActiveState != MUTANT_STATE_LOCUST_ATTACK_1)
		{
			extraHeadRot.x = ai.xAngle;
			extraTorsoRot.x = ai.xAngle;
			extraTorsoRot.y = ai.angle;
		}

		CreatureJoint(item, 0, extraHeadRot.y);
		CreatureJoint(item, 1, extraHeadRot.x);
		CreatureJoint(item, 2, extraTorsoRot.y);
		CreatureJoint(item, 3, extraTorsoRot.x);
		CreatureAnimation(itemNumber, headingAngle, 0);
	}
}
