#include "framework.h"
#include "Objects/TR4/Entity/tr4_demigod.h"

#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
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

namespace TEN::Entities::TR4
{
	constexpr auto DEMIGOD_IDLE_RANGE					   = SQUARE(BLOCK(2));
	constexpr auto DEMIGOD_WALK_RANGE					   = SQUARE(BLOCK(3));
	constexpr auto DEMIGOD1_WALK_RANGE					   = SQUARE(BLOCK(3));
	constexpr auto DEMIGOD2_RADIAL_PROJECTILE_ATTACK_RANGE = SQUARE(BLOCK(5));
	constexpr auto DEMIGOD3_RADIAL_PROJECTILE_ATTACK_RANGE = SQUARE(BLOCK(5));

	enum DemigodState
	{
		DEMIGOD_STATE_IDLE = 0,
		DEMIGOD_STATE_WALK_FORWARD = 1,
		DEMIGOD_STATE_RUN_FORWARD = 2,

		// ID_DEMIGOD2:
		DEMIGOD2_STATE_AIM = 3,
		DEMIGOD2_STATE_SINGLE_PROJECTILE_ATTACK = 4,
		DEMIGOD2_STATE_RADIAL_AIM = 5,
		DEMIGOD2_STATE_RADIAL_PROJECTILE_ATTACK = 6,
		DEMIGOD2_STATE_RADIAL_UNAIM = 7,

		DEMIGOD_STATE_DEATH = 8,

		// ID_DEMIGOD3:
		DEMIGOD3_STATE_RADIAL_AIM = 9,
		DEMIGOD3_STATE_RADIAL_PROJECTILE_ATTACK = 10,
		DEMIGOD3_STATE_AIM = 11,
		DEMIGOD3_STATE_SINGLE_PROJECTILE_ATTACK = 12,

		// ID_DEMIGOD1:
		DEMIGOD1_STATE_AIM = 13,
		DEMIGOD1_STATE_HAMMER_ATTACK = 14,

		DEMIGOD_STATE_RUN_OVER_DEATH = 15
	};

	enum DemigodAnim
	{
		DEMIGOD_ANIM_IDLE = 0,
		DEMIGOD_ANIM_WALK_FORWARD = 1,
		DEMIGOD_ANIM_RUN_FORWARD = 2,

		// ID_DEMIGOD2:
		DEMIGOD2_ANIM_RADIAL_AIM = 3,
		DEMIGOD2_ANIM_RADIAL_PROJECTILE_ATTACK = 4,
		DEMIGOD2_ANIM_RADIAL_UNAIM = 5,
		DEMIGOD2_ANIM_AIM = 6,
		DEMIGOD2_ANIM_AIM_IDLE = 7,
		DEMIGOD2_ANIM_SINGLE_PROJECTILE_ATTACK = 8,
		DEMIGOD2_ANIM_UNAIM = 9,

		DEMIGOD_ANIM_RUN_FORWARD_TO_IDLE = 10,
		DEMIGOD_ANIM_IDLE_TO_RUN_FORWARD = 11,
		DEMIGOD_ANIM_DEATH = 12,
		DEMIGOD_ANIM_IDLE_TO_WALK_FORWARD = 13,
		DEMIGOD_ANIM_WALK_FORWARD_TO_IDLE = 14,

		// ID_DEMIGOD3:
		DEMIGOD3_ANIM_RADIAL_AIM = 15,
		DEMIGOD3_ANIM_RADIAL_PROJECTILE_ATTACK = 16,
		DEMIGOD3_ANIM_RADIAL_UNAIM = 17,
		DEMIGOD3_ANIM_AIM = 18,
		DEMIGOD3_ANIM_SINGLE_PROJECTILE_ATTACK = 19,
		DEMIGOD3_ANIM_UNAIM = 20,
		DEMIGOD3_ANIM_AIM_IDLE = 21,

		// ID_DEMIGOD1:
		DEMIGOD1_ANIM_AIM = 22,
		DEMIGOD1_ANIM_HAMMER_ATTACK = 23,
		DEMIGOD1_ANIM_AIM_IDLE = 24,
		DEMIGOD1_ANIM_UNAIM = 25,

		DEMIGOD_ANIM_RUN_TO_IDLE = 26,
		DEMIGOD_ANIM_RUN_OVER_DEATH = 27
	};

	void InitializeDemigod(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(*item, 0);

		/*if (g_Level.NumItems > 0)
		{
			ItemInfo* currentItem = &g_Level.Items[0];
			int k = 0;

			while (item == currentItem || currentItem->objectNumber != ID_DEMIGOD3 || currentItem->itemFlags[0])
			{
				k++;
				currentItem++;
				if (k >= g_Level.NumItems)
					return;
			}

			item->itemFlags[0] = k;
		}*/
	}

	void TriggerDemigodMissileFlame(short fxNumber, short xVel, short yVel, short zVel)
	{
		auto* fx = &EffectList[fxNumber];

		int dx = LaraItem->Pose.Position.x - fx->pos.Position.x;
		int dz = LaraItem->Pose.Position.z - fx->pos.Position.z;

		if (dx >= -BLOCK(16) && dx <= BLOCK(16) &&
			dz >= -BLOCK(16) && dz <= BLOCK(16))
		{
			auto* spark = GetFreeParticle();

			spark->on = 1;
			if (fx->flag1 == 3 || fx->flag1 == 4)
			{
				spark->sR = 0;
				spark->dR = 0;
				spark->sB = (GetRandomControl() & 0x7F) + 32;
				spark->sG = spark->sB + 64;
				spark->dG = (GetRandomControl() & 0x7F) + 32;
				spark->dB = spark->dG + 64;
			}
			else
			{
				spark->sR = (GetRandomControl() & 0x7F) + 32;
				spark->sG = spark->sR - (GetRandomControl() & 0x1F);
				spark->sB = 0;
				spark->dR = (GetRandomControl() & 0x7F) + 32;
				spark->dB = 0;
				spark->dG = spark->dR - (GetRandomControl() & 0x1F);
			}

			spark->fadeToBlack = 8;
			spark->colFadeSpeed = (GetRandomControl() & 3) + 4;
			spark->blendMode = BlendMode::Additive;
			spark->life = spark->sLife = (GetRandomControl() & 3) + 16;
			spark->y = 0;
			spark->x = (GetRandomControl() & 0xF) - 8;
			spark->yVel = yVel;
			spark->zVel = zVel;
			spark->z = (GetRandomControl() & 0xF) - 8;
			spark->xVel = xVel;
			spark->friction = 68;
			spark->flags = 602;
			spark->rotAng = GetRandomControl() & 0xFFF;

			if (Random::TestProbability(1 / 2.0f))
				spark->rotAdd = -32 - (GetRandomControl() & 0x1F);
			else
				spark->rotAdd = (GetRandomControl() & 0x1F) + 32;

			spark->gravity = 0;
			spark->maxYvel = 0;
			spark->fxObj = fxNumber;
			spark->scalar = 2;
			spark->sSize = spark->size = (GetRandomControl() & 7) + 64;
			spark->dSize = spark->size / 32;
		}
	}

	void TriggerDemigodMissile(Pose* pose, short roomNumber, int flags)
	{
		short fxNumber = CreateNewEffect(roomNumber);
		if (fxNumber != -1)
		{
			auto* fx = &EffectList[fxNumber];

			fx->pos.Position.x = pose->Position.x;
			fx->pos.Position.y = pose->Position.y - (GetRandomControl() & 0x3F) - 32;
			fx->pos.Position.z = pose->Position.z;

			fx->pos.Orientation.x = pose->Orientation.x;

			if (flags < 4)
				fx->pos.Orientation.y = pose->Orientation.y;
			else
				fx->pos.Orientation.y = pose->Orientation.y + (GetRandomControl() & 0x7FF) - 1024;

			fx->pos.Orientation.z = 0;

			fx->roomNumber = roomNumber;
			fx->counter = 2 * GetRandomControl() + -ANGLE(180.0f);
			fx->flag1 = flags;
			fx->speed = (GetRandomControl() & 0x1F) + 96;
			fx->objectNumber = ID_ENERGY_BUBBLES;
			fx->frameNumber = Objects[ID_ENERGY_BUBBLES].meshIndex + ((flags >= 4) ? flags - 1 : flags);
		}
	}

	void DoDemigodEffects(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		int animNumber = item->Animation.AnimNumber;

		if (animNumber == DEMIGOD2_ANIM_SINGLE_PROJECTILE_ATTACK)
		{
			if (item->Animation.FrameNumber == 0)
			{
				auto origin = GetJointPosition(item, 16, Vector3i(-544, 96, 0));
				auto target = GetJointPosition(item, 16, Vector3i(-900, 96, 0));
				auto orient = Geometry::GetOrientToPoint(origin.ToVector3(), target.ToVector3());

				auto pose = Pose(origin, orient);
				if (item->ObjectNumber == ID_DEMIGOD3)
					TriggerDemigodMissile(&pose, item->RoomNumber, 3);
				else
					TriggerDemigodMissile(&pose, item->RoomNumber, 5);
			}
		}
		else if (animNumber == DEMIGOD3_ANIM_SINGLE_PROJECTILE_ATTACK)
		{
			if (item->Animation.FrameNumber == 0)
			{
				auto pos1 = GetJointPosition(item,  16, Vector3i(-544, 96, 0));
				auto pos2 = GetJointPosition(item, 16, Vector3i(-900, 96, 0));
				auto orient = Geometry::GetOrientToPoint(pos1.ToVector3(), pos2.ToVector3());

				auto pose = Pose(pos1, orient);
				if (item->ObjectNumber == ID_DEMIGOD3)
					TriggerDemigodMissile(&pose, item->RoomNumber, 3);
				else
					TriggerDemigodMissile(&pose, item->RoomNumber, 5);
			}
		}
		else if (animNumber == DEMIGOD3_ANIM_RADIAL_PROJECTILE_ATTACK)
		{
			int frameNumber = item->Animation.FrameNumber;

			if (frameNumber >= 8 && frameNumber <= 64)
			{
				auto pos1 = Vector3i(0, 0, 192);
				auto pos2 = Vector3i(0, 0, 384);

				if (GlobalCounter & 1)
				{
					pos1 = GetJointPosition(item, 18, pos1);
					pos2 = GetJointPosition(item, 18, pos2);
				}
				else
				{
					pos1 = GetJointPosition(item, 17, pos1);
					pos2 = GetJointPosition(item, 17, pos2);
				}

				auto orient = Geometry::GetOrientToPoint(pos1.ToVector3(), pos2.ToVector3());
				auto pose = Pose(pos1, orient);
				TriggerDemigodMissile(&pose, item->RoomNumber, 4);
			}
		}
	}

	void TriggerHammerSmoke(int x, int y, int z, int maxSmokeCount)
	{
		int angle = GetRandomControl() * 2;
		int deltaAngle = 0x10000 / maxSmokeCount;

		if (maxSmokeCount > 0)
		{
			for (int i = 0; i < maxSmokeCount; i++)
			{
				auto* spark = &SmokeSparks[GetFreeSmokeSpark()];

				spark->on = true;
				spark->sShade = 0;
				spark->colFadeSpeed = 4;
				spark->dShade = (GetRandomControl() & 0x1F) + 96;
				spark->fadeToBlack = 24 - (GetRandomControl() & 7);
				spark->blendMode = BlendMode::Additive;
				spark->life = spark->sLife = (GetRandomControl() & 7) + 48;
				spark->position.x = (GetRandomControl() & 0x1F) + x - 16;
				spark->position.y = (GetRandomControl() & 0x1F) + y - 16;
				spark->position.z = (GetRandomControl() & 0x1F) + z - 16;
				spark->velocity.x = (byte)(GetRandomControl() + 256) * phd_sin(angle);
				spark->velocity.y = -32 - (GetRandomControl() & 0x3F);
				spark->velocity.z = (byte)(GetRandomControl() + 256) * phd_cos(angle);
				spark->friction = 9;

				if (Random::TestProbability(1 / 2.0f))
				{
					spark->flags = 16;
					spark->rotAng = GetRandomControl() & 0xFFF;

					if (Random::TestProbability(1 / 2.0f))
						spark->rotAdd = -64 - (GetRandomControl() & 0x3F);
					else
						spark->rotAdd = (GetRandomControl() & 0x3F) + 64;
				}
				else if (g_Level.Rooms[LaraItem->RoomNumber].flags & ENV_FLAG_WIND)
					spark->flags = 256;
				else
					spark->flags = SP_NONE;
				
				spark->gravity = -4 - (GetRandomControl() & 3);
				spark->maxYvel = -4 - (GetRandomControl() & 3);
				spark->dSize = ((GetRandomControl() & 0x3F) + 64);
				spark->sSize = spark->dSize / 8;
				spark->size = spark->dSize / 8;

				angle += deltaAngle;
			}
		}
	}

	void DemigodControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];

		int someItemNumber = item->ItemFlags[0];
		if (someItemNumber &&
			g_Level.Items[someItemNumber].Status == ITEM_ACTIVE &&
			g_Level.Items[someItemNumber].Active)
		{
			item->HitPoints = Objects[item->ObjectNumber].HitPoints;
			return;
		}

		auto* creature = GetCreatureInfo(item);

		short angle = 0;
		short tilt = 0;
		short joint0 = 0;
		short joint1 = 0;
		short joint2 = 0;
		short joint3 = 0;

		if (item->HitPoints <= 0)
		{
			item->HitPoints = 0;

			if (item->Animation.ActiveState != DEMIGOD_STATE_DEATH &&
				item->Animation.ActiveState != DEMIGOD_STATE_RUN_OVER_DEATH)
			{
				if (item->Animation.ActiveState == DEMIGOD_STATE_WALK_FORWARD ||
					item->Animation.ActiveState == DEMIGOD_STATE_RUN_FORWARD)
				{
					item->Animation.AnimNumber = DEMIGOD_ANIM_RUN_OVER_DEATH;
					item->Animation.FrameNumber = 0;
					item->Animation.ActiveState = DEMIGOD_STATE_RUN_OVER_DEATH;
				}
				else
				{
					item->Animation.AnimNumber = DEMIGOD_ANIM_DEATH;
					item->Animation.FrameNumber = 0;
					item->Animation.ActiveState = DEMIGOD_STATE_DEATH;
				}
			}
		}
		else
		{
			if (item->AIBits)
				GetAITarget(creature);

			int dx = 0;
			int dy = 0;
			int dz = 0;

			AI_INFO AI;
			CreatureAIInfo(item, &AI);

			AI_INFO laraAI;
			if (creature->Enemy->IsLara())
			{
				laraAI.ahead = AI.ahead;
				laraAI.angle = AI.angle;
				laraAI.xAngle = 0;
			}
			else
			{
				dx = LaraItem->Pose.Position.x - item->Pose.Position.x;
				dz = LaraItem->Pose.Position.z - item->Pose.Position.z;
				laraAI.angle = phd_atan(dz, dx) - item->Pose.Orientation.y;
				laraAI.xAngle = 0;

				laraAI.ahead = true;
				if (laraAI.angle <= -ANGLE(90.0f) || laraAI.angle >= ANGLE(90.0f))
					laraAI.ahead = false;

				dx = abs(dx);
				dy = item->Pose.Position.y - LaraItem->Pose.Position.y;
				dz = abs(dz);

				if (dx <= dz)
					laraAI.xAngle = phd_atan(dz + (dx >> 1), dy);
				else
					laraAI.xAngle = phd_atan(dx + (dz >> 1), dy);
			}

			GetCreatureMood(item, &AI, true);
			CreatureMood(item, &AI, true);

			angle = CreatureTurn(item, creature->MaxTurn);

			if (laraAI.ahead)
			{
				joint0 = laraAI.angle / 2;
				joint1 = -laraAI.xAngle;
				joint2 = laraAI.angle / 2;
				joint3 = laraAI.angle / 2;
			}
			else if (AI.ahead)
			{
				joint0 = AI.angle / 2;
				joint1 = -AI.xAngle;
				joint2 = AI.angle / 2;
				joint3 = AI.angle / 2;
			}

			switch (item->Animation.ActiveState)
			{
			case DEMIGOD_STATE_IDLE:
				creature->MaxTurn = 0;

				if (AI.ahead)
					joint1 = -AI.xAngle;

				if (item->ObjectNumber == ID_DEMIGOD1)
				{
					if (AI.distance >= DEMIGOD1_WALK_RANGE)
					{
						item->Animation.TargetState = DEMIGOD_STATE_WALK_FORWARD;
						break;
					}

					if (AI.bite ||
						LaraItem->Animation.ActiveState >= LS_LADDER_IDLE &&
						LaraItem->Animation.ActiveState <= LS_LADDER_DOWN &&
						!Lara.Location)
					{
						item->Animation.TargetState = DEMIGOD1_STATE_AIM;
						break;
					}

					if (AI.distance <= DEMIGOD1_WALK_RANGE)
					{
						item->Animation.TargetState = DEMIGOD_STATE_WALK_FORWARD;
						break;
					}
				}
				else
				{
					if (Targetable(item, &AI))
					{
						creature->Flags = 1;

						if (item->ObjectNumber == ID_DEMIGOD2)
							item->Animation.TargetState = DEMIGOD2_STATE_AIM;
						else
							item->Animation.TargetState = DEMIGOD3_STATE_AIM;

						break;
					}

					if (item->ObjectNumber == ID_DEMIGOD3)
					{
						if (AI.distance <= DEMIGOD_IDLE_RANGE ||
							AI.distance >= DEMIGOD3_RADIAL_PROJECTILE_ATTACK_RANGE)
						{
							item->Animation.TargetState = DEMIGOD_STATE_WALK_FORWARD;
							break;
						}

						if (Random::TestProbability(1 / 4.0f))
						{
							item->Animation.TargetState = DEMIGOD3_STATE_RADIAL_AIM;
							break;
						}
					}
				}

				if (AI.distance > DEMIGOD_WALK_RANGE && item->ObjectNumber == ID_DEMIGOD2)
					item->Animation.TargetState = DEMIGOD2_STATE_RADIAL_AIM;
				else
					item->Animation.TargetState = DEMIGOD_STATE_WALK_FORWARD;

				break;

			case DEMIGOD_STATE_WALK_FORWARD:
				creature->MaxTurn = ANGLE(7.0f);

				if (AI.distance < DEMIGOD_IDLE_RANGE)
				{
					item->Animation.TargetState = DEMIGOD_STATE_IDLE;
					break;
				}

				if (item->ObjectNumber == ID_DEMIGOD1)
				{
					if (AI.distance < DEMIGOD1_WALK_RANGE)
					{
						item->Animation.TargetState = DEMIGOD_STATE_IDLE;
						break;
					}
				}
				else
				{
					if (Targetable(item, &AI))
					{
						item->Animation.TargetState = DEMIGOD_STATE_IDLE;
						break;
					}
				}

				if (AI.distance > DEMIGOD_WALK_RANGE)
				{
					if (item->ObjectNumber == ID_DEMIGOD2)
						item->Animation.TargetState = DEMIGOD2_STATE_RADIAL_PROJECTILE_ATTACK;
					else
						item->Animation.TargetState = DEMIGOD_STATE_RUN_FORWARD;
				}

				break;

			case DEMIGOD_STATE_RUN_FORWARD:
				creature->MaxTurn = ANGLE(7.0f);

				if (AI.distance < DEMIGOD_IDLE_RANGE)
				{
					item->Animation.TargetState = DEMIGOD_STATE_IDLE;
					break;
				}

				if (item->ObjectNumber == ID_DEMIGOD1)
				{
					if (AI.distance < DEMIGOD1_WALK_RANGE)
					{
						item->Animation.TargetState = DEMIGOD_STATE_IDLE;
						break;
					}
				}
				else
				{
					if (Targetable(item, &AI) || item->ObjectNumber == ID_DEMIGOD3 && AI.distance > DEMIGOD_IDLE_RANGE)
					{
						item->Animation.TargetState = DEMIGOD_STATE_IDLE;
						break;
					}

					if (AI.distance < DEMIGOD_WALK_RANGE)
						item->Animation.TargetState = DEMIGOD_STATE_WALK_FORWARD;
				}

				break;

			case DEMIGOD2_STATE_AIM:
				creature->MaxTurn = 0;

				if (AI.ahead)
					joint1 = -AI.xAngle;

				if (item->Animation.AnimNumber == DEMIGOD2_ANIM_AIM)
				{
					if (AI.angle >= ANGLE(7.0f))
						item->Pose.Orientation.y += ANGLE(7.0f);
					else if (AI.angle <= -ANGLE(7.0f))
						item->Pose.Orientation.y += -ANGLE(7.0f);
					else
						item->Pose.Orientation.y += AI.angle;
				}

				if (Targetable(item, &AI) || creature->Flags)
				{
					item->Animation.TargetState = DEMIGOD2_STATE_SINGLE_PROJECTILE_ATTACK;
					creature->Flags = 0;
				}
				else
				{
					item->Animation.TargetState = DEMIGOD_STATE_IDLE;
					creature->Flags = 0;
				}

				break;

			case DEMIGOD2_STATE_SINGLE_PROJECTILE_ATTACK:
			case DEMIGOD3_STATE_SINGLE_PROJECTILE_ATTACK:
				DoDemigodEffects(itemNumber);
				break;

			case DEMIGOD2_STATE_RADIAL_PROJECTILE_ATTACK:
				creature->MaxTurn = ANGLE(7.0f);
			
				if (Targetable(item, &AI))
					item->Animation.TargetState = DEMIGOD2_STATE_RADIAL_UNAIM;

				break;

			case DEMIGOD3_STATE_RADIAL_AIM:
				creature->MaxTurn = ANGLE(7.0f);

				if (!Targetable(item, &AI) && AI.distance < DEMIGOD3_RADIAL_PROJECTILE_ATTACK_RANGE)
					item->Animation.TargetState = DEMIGOD3_STATE_RADIAL_PROJECTILE_ATTACK;

				break;

			case DEMIGOD2_STATE_RADIAL_AIM:
				creature->MaxTurn = ANGLE(7.0f);
				if (!Targetable(item, &AI) && AI.distance < DEMIGOD2_RADIAL_PROJECTILE_ATTACK_RANGE)
					item->Animation.TargetState = DEMIGOD2_STATE_RADIAL_PROJECTILE_ATTACK;

				break;

			case DEMIGOD3_STATE_RADIAL_PROJECTILE_ATTACK:
				creature->MaxTurn = ANGLE(7.0f);

				DoDemigodEffects(itemNumber);

				if (!Targetable(item, &AI) || AI.distance < DEMIGOD3_RADIAL_PROJECTILE_ATTACK_RANGE || !GetRandomControl())
				{
					item->Animation.TargetState = DEMIGOD_STATE_IDLE;
					break;
				}

				break;

			case DEMIGOD3_STATE_AIM:
				creature->MaxTurn = 0;
				joint2 = joint0;
				joint0 = 0;

				if (AI.ahead)
					joint1 = -AI.xAngle;

				if (item->Animation.AnimNumber == DEMIGOD2_ANIM_AIM)
				{
					if (AI.angle >= ANGLE(7.0f))
						item->Pose.Orientation.y += ANGLE(7.0f);
					else if (AI.angle <= -ANGLE(7.0f))
						item->Pose.Orientation.y += -ANGLE(7.0f);
					else
						item->Pose.Orientation.y += AI.angle;
				}

				if (Targetable(item, &AI) || creature->Flags)
				{
					item->Animation.TargetState = DEMIGOD3_STATE_SINGLE_PROJECTILE_ATTACK;
					creature->Flags = 0;
				}
				else
				{
					item->Animation.TargetState = DEMIGOD_STATE_IDLE;
					creature->Flags = 0;
				}

				break;

			case DEMIGOD1_STATE_AIM:
				creature->MaxTurn = 0;
				joint2 = joint0;
				joint0 = 0;

				if (AI.angle >= ANGLE(7.0f))
					item->Pose.Orientation.y += ANGLE(7.0f);
				else if (AI.angle <= -ANGLE(7.0f))
					item->Pose.Orientation.y += -ANGLE(7.0f);
				else
					item->Pose.Orientation.y += AI.angle;

				if (AI.distance >= DEMIGOD1_WALK_RANGE ||
					!AI.bite &&
					(LaraItem->Animation.ActiveState < LS_LADDER_IDLE ||
						LaraItem->Animation.ActiveState > LS_LADDER_DOWN ||
						Lara.Location))
				{
					item->Animation.TargetState = DEMIGOD_STATE_IDLE;
				}
				else
					item->Animation.TargetState = DEMIGOD1_STATE_HAMMER_ATTACK;

				break;

			case DEMIGOD1_STATE_HAMMER_ATTACK:
				if (item->Animation.FrameNumber == DEMIGOD_ANIM_RUN_TO_IDLE)
				{
					auto pos = GetJointPosition(item, 17, Vector3i(80, -8, -40));

					short roomNumber = item->RoomNumber;
					FloorInfo* floor = GetFloor(pos.x, pos.y, pos.z, &roomNumber);
					int height = GetFloorHeight(floor, pos.x, pos.y, pos.z);
					if (height == NO_HEIGHT)
						pos.y = pos.y - CLICK(0.5f);
					else
						pos.y = height - CLICK(0.5f);

					TriggerShockwave((Pose*)&pos, 24, 88, 256, 128, 128, 128, 32, EulerAngles::Identity, 8, true, false, false, (int)ShockwaveStyle::Normal);
					TriggerHammerSmoke(pos.x, pos.y + 128, pos.z, 8);

					pos.y -= 64;
					TriggerShockwave((Pose*)&pos, 24, 88, 200, 128, 128, 128, 32, EulerAngles::Identity, 8, true, false, true, (int)ShockwaveStyle::Normal);

					auto lightColor = Color(1.0f, 0.4f, 0.2f);
					SpawnDynamicPointLight(pos.ToVector3(), lightColor, BLOCK(6));

					Camera.bounce = -128;

					// Lara is climbing a ladder; shake her off.
					if (/*Lara.Control.IsClimbingLadder &&*/ // TODO: Try with only this line and include hang shimmy states.
						LaraItem->Animation.ActiveState >= LS_LADDER_IDLE &&
						LaraItem->Animation.ActiveState <= LS_LADDER_DOWN &&
						!Lara.Location)
					{
						SetAnimation(*LaraItem, LA_FALL_START);
						LaraItem->Animation.Velocity.z = 2;
						LaraItem->Animation.Velocity.y = 1;

						ResetPlayerFlex(LaraItem);
						LaraItem->HitStatus = true;
						Lara.Control.HandStatus = HandStatus::Free;
					}
				}

			default:
				break;
			}
		}

		CreatureTilt(item, 0);

		CreatureJoint(item, 0, joint0);
		CreatureJoint(item, 1, joint1);
		CreatureJoint(item, 2, joint2);
		CreatureJoint(item, 3, joint3);

		CreatureAnimation(itemNumber, angle, 0);
	}
}
