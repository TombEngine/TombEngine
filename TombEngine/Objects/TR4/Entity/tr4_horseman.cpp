#include "framework.h"
#include "Objects/TR4/Entity/tr4_horseman.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/effects/debris.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;
using namespace TEN::Math;

namespace TEN::Entities::TR4
{
	const auto HorsemanBite1 = CreatureBiteInfo(Vector3::Zero, 6);
	const auto HorsemanBite2 = CreatureBiteInfo(Vector3::Zero, 14);
	const auto HorsemanBite3 = CreatureBiteInfo(Vector3::Zero, 10);
	const auto HorsemanAxeAttackJoints	   = std::vector<unsigned int>{ 5, 6 };
	const auto HorsemanKickAttackJoints	   = std::vector<unsigned int>{ 14 };
	const auto HorsemanMountedAttackJoints = std::vector<unsigned int>{ 5, 6, 10 };
	const auto HorsemanShieldAttackJoints  = std::vector<unsigned int>{ 10 };

	const auto HorseBite1 = CreatureBiteInfo(Vector3::Zero, 13);
	const auto HorseBite2 = CreatureBiteInfo(Vector3::Zero, 17);
	const auto HorseBite3 = CreatureBiteInfo(Vector3::Zero, 19);

	enum HorsemanState
	{
		// No state 0.
		HORSEMAN_STATE_MOUNTED_RUN_FORWARD = 1,
		HORSEMAN_STATE_MOUNTED_WALK_FORWARD = 2,
		HORSEMAN_STATE_MOUNTED_IDLE = 3,
		HORSEMAN_STATE_MOUNTED_REAR = 4,
		HORSEMAN_STATE_MOUNT_HORSE = 5,
		HORSEMAN_STATE_MOUNTED_ATTACK_RIGHT = 6,
		HORSEMAN_STATE_MOUNTED_ATTACK_LEFT = 7,
		HORSEMAN_STATE_FALL_OFF_HORSE = 8,
		HORSEMAN_STATE_IDLE = 9,
		HORSEMAN_STATE_WALK_FORWARD = 10,
		HORSEMAN_STATE_RUN_FORWARD = 11,
		HORSEMAN_STATE_WALK_FORWARD_ATTACK_RIGHT = 12,
		HORSEMAN_STATE_WALK_FORWARD_ATTACK_LEFT = 13,
		HORSEMAN_STATE_IDLE_ATTACK = 14,
		HORSEMAN_STATE_SHIELD = 15,
		HORSEMAN_STATE_DEATH = 16,
		HORSEMAN_STATE_MOUNTED_SPRINT = 17,
	};

	enum HorsemanAnim
	{
		HORSEMAN_ANIM_MOUNTED_RUN_FORWARD = 0,
		HORSEMAN_ANIM_MOUNTED_REAR = 1,
		HORSEMAN_ANIM_MOUNTED_IDLE = 2,
		HORSEMAN_ANIM_FALL_OFF_HORSE_START = 3,
		HORSEMAN_ANIM_FALL_OFF_HORSE_END = 4,
		HORSEMAN_ANIM_WALK_FORWARD = 5,
		HORSEMAN_ANIM_WALK_FORWARD_ATTACK_RIGHT = 6,
		HORSEMAN_ANIM_WALK_FORWARD_ATTACK_LEFT = 7,
		HORSEMAN_ANIM_IDLE = 8,
		HORSEMAN_ANIM_IDLE_TO_WALK_FORWARD = 9,
		HORSEMAN_ANIM_WALK_FORWARD_TO_IDLE = 10,
		HORSEMAN_ANIM_IDLE_ATTACK = 11,
		HORSEMAN_ANIM_MOUNTED_ATTACK_RIGHT = 12,
		HORSEMAN_ANIM_MOUNTED_ATTACK_LEFT = 13,
		HORSEMAN_ANIM_MOUNT_HORSE = 14,
		HORSEMAN_ANIM_RUN_FORWARD = 15,
		HORSEMAN_ANIM_RUN_FORWARD_TO_WALK_FORWARD = 16,
		HORSEMAN_ANIM_WALK_FORWARD_TO_RUN_FORWARD = 17,
		HORSEMAN_ANIM_SHIELD_START = 18,
		HORSEMAN_ANIM_SHIELD_CONTINUE = 19,
		HORSEMAN_ANIM_SHIELD_END = 20,
		HORSEMAN_ANIM_DEATH = 21,
		HORSEMAN_ANIM_MOUNTED_RUN_FORWARD_TO_IDLE = 22,
		HORSEMAN_ANIM_MOUNTED_WALK_FORWARD = 23,
		HORSEMAN_ANIM_MOUNTED_IDLE_TO_WALK_FORWARD = 24,
		HORSEMAN_ANIM_MOUNTED_WALK_FORWARD_TO_IDLE = 25,
		HORSEMAN_ANIM_MOUNTED_RUN_FORWARD_TO_WALK_FORWARD = 26,
		HORSEMAN_ANIM_MOUNTED_IDLE_TO_RUN_FORWARD = 27,
		HORSEMAN_ANIM_MOUNTED_WALK_FORWARD_TO_RUN_FORWARD = 28,
		HORSEMAN_ANIM_MOUNTED_SPRINT = 29,
		HORSEMAN_ANIM_MOUNTED_RUN_FORWARD_TO_SPRINT = 30,
		HORSEMAN_ANIM_MOUNTED_SPRINT_TO_RUN_FORWARD = 31,
		HORSEMAN_ANIM_MOUNTED_SPRINT_TO_IDLE = 32
	};

	enum HorseState
	{
		// No state 0.
		HORSE_STATE_IDLE = 1,
		HORSE_STATE_RUN_FORWARD = 2,
		HORSE_STATE_WALK_FORWARD = 3,
		HORSE_STATE_REAR = 4,
		HORSE_STATE_SPRINT = 5
	};

	enum HorseAnim
	{
		HORSE_ANIM_RUN = 0,
		HORSE_ANIM_REAR = 1,
		HORSE_ANIM_IDLE = 2,
		HORSE_ANIM_RUN_TO_IDLE = 3,
		HORSE_ANIM_WALK_FORWARD = 4,
		HORSE_ANIM_IDLE_TO_WALK_FORWARD = 5,
		HORSE_ANIM_WALK_FORWARD_TO_IDLE = 6,
		HORSE_ANIM_RUN_FORWARD_TO_WALK_FORWARD = 7,
		HORSE_ANIM_IDLE_TO_RUN_FORWARD = 8,
		HORSE_ANIM_WALK_FORWARD_TO_RUN_FORWARD = 9,
		HORSE_ANIM_SPRINT = 10,
		HORSE_ANIM_RUN_FORWARD_TO_SPRINT = 11,
		HORSE_ANIM_SPRINT_TO_RUN_FORWARD = 12,
		HORSE_ANIM_SPRINT_TO_IDLE = 13
	};

	void InitializeHorse(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		SetAnimation(*item, HORSE_ANIM_IDLE);
		item->Animation.ActiveState = HORSEMAN_STATE_MOUNTED_RUN_FORWARD; // TODO: Check if needed. -- Sezz
		item->Animation.TargetState = HORSEMAN_STATE_MOUNTED_RUN_FORWARD;
	}

	void InitializeHorseman(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(*item, HORSEMAN_ANIM_IDLE);
		item->ItemFlags[0] = NO_VALUE; // No horse yet.
	}

	void HorsemanSparks(Vector3i* pos, int param1, int maxSparks)
	{
		for (int i = 0; i < maxSparks; i++)
		{
			auto* spark = GetFreeParticle();

			int random = GetRandomControl();

			spark->on = 1;
			spark->sG = -128;
			spark->sB = (random & 0xF) + 16;
			spark->sR = 0;
			spark->dG = 96;
			spark->dB = ((random / 16) & 0x1F) + 48;
			spark->dR = 0;
			spark->colFadeSpeed = 2;
			spark->fadeToBlack = 4;
			spark->life = 9;
			spark->sLife = 9;
			spark->blendMode = BlendMode::Additive;
			spark->x = pos->x;
			spark->y = pos->y;
			spark->z = pos->z;
			spark->friction = 34;
			spark->yVel = (random & 0xFFF) - 2048;
			spark->flags = SP_NONE;
			spark->gravity = (random / 128) & 0x1F;
			spark->maxYvel = 0;
			spark->zVel = phd_cos((random & 0x7FF) + param1 - 1024) * 4096;
			spark->xVel = -phd_sin((random & 0x7FF) + param1 - 1024) * 4096;
		}

		for (int i = 0; i < maxSparks; i++)
		{
			auto* spark = GetFreeParticle();

			int random = GetRandomControl();

			spark->on = 1;
			spark->sG = -128;
			spark->sR = 0;
			spark->dG = 96;
			spark->sB = (random & 0xF) + 16;
			spark->dR = 0;
			spark->colFadeSpeed = 2;
			spark->fadeToBlack = 4;
			spark->dB = ((random / 16) & 0x1F) + 48;
			spark->life = 9;
			spark->sLife = 9;
			spark->blendMode = BlendMode::Additive;
			spark->x = pos->x;
			spark->y = pos->y;
			spark->z = pos->z;
			spark->yVel = (random & 0xFFF) - 2048;
			spark->gravity = (random / 128) & 0x1F;
			spark->rotAng = random / 8;

			if (Random::TestProbability(1 / 2.0f))
				spark->rotAdd = -16 - (random & 0xF);
			else
				spark->rotAdd = spark->sB;
			
			spark->scalar = 3;
			spark->friction = 34;
			spark->sSize = spark->size = ((random / 32) & 7) + 4;
			spark->dSize = spark->sSize / 2;
			spark->flags = SP_DEF | SP_ROTATE | SP_SCALE;
			spark->maxYvel = 0;
			spark->xVel = -phd_sin((random & 0x7FF) + param1 - 1024) * 4096;
			spark->zVel = phd_cos((random & 0x7FF) + param1 - 1024) * 4096;
		}
	}

	void HorsemanControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		// Try to find a horse.
		if (item->ItemFlags[0] == NO_VALUE)
		{
			for (int i = 0; i < g_Level.NumItems; i++)
			{
				auto* currentItem = &g_Level.Items[i];

				if (currentItem->ObjectNumber == ID_HORSE &&
					item->TriggerFlags == currentItem->TriggerFlags)
				{
					item->ItemFlags[0] = i;
					currentItem->Flags |= 0x20;
				}
			}
		}

		// If no horse was found, set ItemFlags[0] to 0 so it isn't searched for again.
		if (item->ItemFlags[0] == NO_VALUE)
			item->ItemFlags[0] = 0;

		// Get horse.
		ItemInfo* horseItem = nullptr;
		if (item->ItemFlags[0] != 0)
			horseItem = &g_Level.Items[item->ItemFlags[0]];

		int xRot = 0;

		if (horseItem != nullptr)
		{
			int x = horseItem->Pose.Position.x + 341 * phd_sin(horseItem->Pose.Orientation.y);
			int y = horseItem->Pose.Position.y;
			int z = horseItem->Pose.Position.z + 341 * phd_cos(horseItem->Pose.Orientation.y);

			auto probe = GetPointCollision(Vector3i(x, y, z), item->RoomNumber);
			int height1 = probe.GetFloorHeight();

			x = horseItem->Pose.Position.x - 341 * phd_sin(horseItem->Pose.Orientation.y);
			y = horseItem->Pose.Position.y;
			z = horseItem->Pose.Position.z - 341 * phd_cos(horseItem->Pose.Orientation.y);

			int height2 = GetPointCollision(Vector3i(x, y, z), probe.GetRoomNumber()).GetFloorHeight();

			xRot = phd_atan(682, height2 - height1);
		}

		short angle = 0;

		if (item->HitPoints <= 0)
		{
			item->HitPoints = 0;
			if (item->ItemFlags[1] == 0)
			{
				if (item->Animation.ActiveState != HORSEMAN_STATE_DEATH)
				{
					item->Animation.AnimNumber = HORSEMAN_ANIM_DEATH;
					item->Animation.ActiveState = HORSEMAN_STATE_DEATH;
					item->Animation.FrameNumber = 0;

					if (item->ItemFlags[0])
					{
						horseItem->AfterDeath = 1;
						item->ItemFlags[0] = 0;
					}
				}
			}
			else
			{
				item->HitPoints = 100;
				item->AIBits = 0;
				item->ItemFlags[1] = 0;
				item->Animation.AnimNumber =HORSEMAN_ANIM_FALL_OFF_HORSE_START;
				item->Animation.FrameNumber = 0;
				item->Animation.ActiveState = HORSEMAN_STATE_FALL_OFF_HORSE;
				creature->Enemy = nullptr;

				horseItem->Animation.TargetState = HORSEMAN_STATE_MOUNTED_RUN_FORWARD;
			}
		}
		else
		{
			if (item->AIBits)
				GetAITarget(creature);
			else if (creature->HurtByLara)
				creature->Enemy = LaraItem;

			AI_INFO AI;
			CreatureAIInfo(item, &AI);

			AI_INFO laraAI;
			if (creature->Enemy->IsLara())
			{
				laraAI.angle = AI.angle;
				laraAI.distance = AI.distance;
			}
			else
			{
				int deltaX = LaraItem->Pose.Position.z - item->Pose.Position.z;
				int deltaZ = LaraItem->Pose.Position.z - item->Pose.Position.z;

				laraAI.angle = phd_atan(deltaZ, deltaX) - item->Pose.Orientation.y;
				laraAI.distance = pow(deltaX, 2) + pow(deltaZ, 2);
			}

			if (item->HitStatus &&
				laraAI.angle < ANGLE(67.5f) &&
				laraAI.angle > -ANGLE(67.5f) &&
				laraAI.distance < pow(BLOCK(2), 2))
			{
				if (item->Animation.ActiveState != HORSEMAN_STATE_SHIELD)
				{
					if (laraAI.angle <= 0)
					{
						if (item->ItemFlags[1])
						{
							if (!item->ItemFlags[1])
							{
								if (item->MeshBits & 0x400)
									item->Animation.RequiredState = HORSEMAN_STATE_SHIELD;
							}
						}
						else
						{
							if (laraAI.angle > 0 || !(item->MeshBits & 0x400))
							{
								if (Lara.Control.Weapon.GunType == LaraWeaponType::Shotgun)
									DoDamage(item, 10);
								else if (Lara.Control.Weapon.GunType == LaraWeaponType::Revolver)
									DoDamage(item, 20);
								else
									item->HitPoints--;

								SoundEffect(SFX_TR4_HORSEMAN_TAKEHIT, &item->Pose);
								SoundEffect(SFX_TR4_HORSE_RICOCHET, &item->Pose);

								auto pos = GetJointPosition(item, 0, Vector3i(0, -128, 80));
								HorsemanSparks(&pos, item->Pose.Orientation.y, 7);
							}
							else if (Random::TestProbability(1 / 8.0f))
							{
								if (item->Animation.ActiveState == HORSEMAN_STATE_SHIELD)
									item->Animation.TargetState = HORSEMAN_STATE_IDLE;
								
								ExplodeItemNode(item, 10, 1, -24);
							}
						}
					}
				}
			}

			creature->HurtByLara = false;

			GetCreatureMood(item, &AI, true);
			CreatureMood(item, &AI, true);

			angle = CreatureTurn(item, creature->MaxTurn);

			switch (item->Animation.ActiveState)
			{
			case HORSEMAN_STATE_MOUNTED_RUN_FORWARD:
				creature->MaxTurn = ANGLE(3.0f);
				horseItem->Animation.TargetState = HORSEMAN_STATE_MOUNTED_WALK_FORWARD;
				if (item->Animation.RequiredState != NO_VALUE)
				{
					item->Animation.TargetState = HORSEMAN_STATE_MOUNTED_SPRINT;
					horseItem->Animation.TargetState = HORSEMAN_STATE_MOUNT_HORSE;
				}
				else if ((item->HitStatus && !GetRandomControl()) ||
					creature->Flags ||
					creature->ReachedGoal)
				{
					if (laraAI.distance > pow(BLOCK(4), 2) ||
						creature->ReachedGoal)
					{
						creature->Enemy = LaraItem;
						creature->Flags = 0;

						if (laraAI.angle > -ANGLE(45.0f) &&
							laraAI.angle < ANGLE(45.0f))
						{
							item->Animation.TargetState = HORSEMAN_STATE_MOUNTED_IDLE;
							horseItem->Animation.TargetState = HORSEMAN_STATE_MOUNTED_RUN_FORWARD;
						}
					}
					else
					{
						item->AIBits = FOLLOW;
						item->ItemFlags[3] = (-(item->ItemFlags[3] != 1)) + 2;
					}
				}

				if (AI.distance >= pow(BLOCK(1), 2))
				{
					if (AI.bite)
					{
						if (AI.angle >= -ANGLE(10.0f) ||
							(AI.distance >= pow(BLOCK(1), 2) &&
							(AI.distance >= pow(1365, 2) || AI.angle <= -ANGLE(20.0f))))
						{
							if (AI.angle > ANGLE(10.0f) &&
								(AI.distance < pow(BLOCK(1), 2) ||
									(AI.distance < pow(1365, 2) && AI.angle < ANGLE(20.0f))))
							{
								item->Animation.TargetState = HORSEMAN_STATE_MOUNTED_ATTACK_RIGHT;
								creature->MaxTurn = 0;
							}
						}
						else
						{
							item->Animation.TargetState = HORSEMAN_STATE_MOUNTED_ATTACK_LEFT;
							creature->MaxTurn = 0;
						}
					}
				}
				else if (AI.bite)
				{
					if (AI.angle >= -ANGLE(10.0f) ||
						AI.angle <= ANGLE(10.0f))
					{
						if (AI.bite)
						{
							if (AI.angle >= -ANGLE(10.0f) ||
								(AI.distance >= pow(BLOCK(1), 2) &&
								(AI.distance >= pow(1365, 2) || AI.angle <= -ANGLE(20.0f))))
							{
								if (AI.angle > ANGLE(10.0f) &&
									(AI.distance < pow(BLOCK(1), 2) ||
										AI.distance < pow(1365, 2) &&
										AI.angle < ANGLE(20.0f)))
								{
									item->Animation.TargetState = HORSEMAN_STATE_MOUNTED_ATTACK_RIGHT;
									creature->MaxTurn = 0;
								}
							}
							else
							{
								item->Animation.TargetState = HORSEMAN_STATE_MOUNTED_ATTACK_LEFT;
								creature->MaxTurn = 0;
							}
						}
					}
				}

				break;

			case HORSEMAN_STATE_MOUNTED_WALK_FORWARD:
				creature->MaxTurn = ANGLE(1.5f);

				if (laraAI.distance > pow(BLOCK(4), 2) || creature->ReachedGoal || creature->Enemy->IsLara())
				{
					item->Animation.TargetState = HORSEMAN_STATE_MOUNTED_RUN_FORWARD;
					creature->ReachedGoal = false;
					creature->Enemy = LaraItem;
					creature->Flags = 0;

					horseItem->Animation.TargetState = HORSEMAN_STATE_MOUNTED_WALK_FORWARD;
				}

				break;

			case HORSEMAN_STATE_MOUNTED_IDLE:
				creature->MaxTurn = 0;
				horseItem->Animation.TargetState = HORSEMAN_STATE_MOUNTED_RUN_FORWARD;

				if (creature->Flags)
				{
					item->AIBits = FOLLOW;
					item->ItemFlags[3] = -(item->ItemFlags[3] != 1) + 2;
				}
				else
					creature->Flags = 0;

				if (item->Animation.RequiredState != NO_VALUE)
				{
					item->Animation.TargetState = HORSEMAN_STATE_MOUNTED_RUN_FORWARD;
					horseItem->Animation.TargetState = HORSEMAN_STATE_MOUNTED_WALK_FORWARD;
					horseItem->Flags = 0;
				}
				else if (creature->ReachedGoal ||
					!horseItem->Flags &&
					AI.distance < pow(BLOCK(1), 2) &&
					AI.bite &&
					AI.angle < ANGLE(10.0f) &&
					AI.angle > -ANGLE(10.0f))
				{
					item->Animation.TargetState = HORSEMAN_STATE_MOUNTED_REAR;

					if (creature->ReachedGoal)
						item->Animation.RequiredState = HORSEMAN_STATE_MOUNTED_SPRINT;
					
					horseItem->Flags = 0;
				}
				else
				{
					item->Animation.TargetState = HORSEMAN_STATE_MOUNTED_RUN_FORWARD;
					horseItem->Animation.TargetState = HORSEMAN_STATE_MOUNTED_WALK_FORWARD;
					horseItem->Flags = 0;
				}

				break;

			case HORSEMAN_STATE_MOUNTED_REAR:
				creature->MaxTurn = 0;

				if (item->Animation.FrameNumber == 0)
				{
					horseItem->Animation.AnimNumber = HORSE_ANIM_REAR;
					horseItem->Animation.FrameNumber = 0;
					horseItem->Animation.ActiveState = HORSE_STATE_REAR;
				}

				if (!horseItem->Flags)
				{
					if (horseItem->TouchBits & 0x22000)
					{
						DoDamage(creature->Enemy, 150);

						if (horseItem->TouchBits & 0x2000)
							CreatureEffect2(horseItem, HorseBite1, 10, -1, DoBloodSplat);
						else
							CreatureEffect2(horseItem, HorseBite2, 10, -1, DoBloodSplat);

						horseItem->Flags = 1;
					}
				}

				break;

			case HORSEMAN_STATE_MOUNTED_ATTACK_RIGHT:
				if (!creature->Flags)
				{
					if (item->TouchBits.Test(HorsemanAxeAttackJoints))
					{
						DoDamage(creature->Enemy, 250);
						CreatureEffect2(item, HorsemanBite1, 10, item->Pose.Orientation.y, DoBloodSplat);
						creature->Flags = 1;
					}
				}

				if (item->HitStatus)
					item->Animation.TargetState = HORSEMAN_STATE_IDLE;

				break;

			case HORSEMAN_STATE_MOUNTED_ATTACK_LEFT:
				if (!creature->Flags)
				{
					if (item->TouchBits.Test(HorsemanKickAttackJoints))
					{
						DoDamage(creature->Enemy, 100);
						CreatureEffect2(item, HorsemanBite2, 3, item->Pose.Orientation.y, DoBloodSplat);
						creature->Flags = 1;
					}
				}

				break;

			case HORSEMAN_STATE_IDLE:
				creature->MaxTurn = 0;
				creature->Flags = 0;

				if (!item->AIBits || item->ItemFlags[3])
				{
					if (item->Animation.RequiredState != NO_VALUE)
						item->Animation.TargetState = item->Animation.RequiredState;
					else if (AI.bite && AI.distance < pow(682,2))
						item->Animation.TargetState = HORSEMAN_STATE_IDLE_ATTACK;
					else if (AI.distance < pow(BLOCK(6), 2) && AI.distance > pow(682, 2))
						item->Animation.TargetState = HORSEMAN_STATE_WALK_FORWARD;
				}
				else
					item->Animation.TargetState = HORSEMAN_STATE_WALK_FORWARD;

				break;

			case HORSEMAN_STATE_WALK_FORWARD:
				creature->MaxTurn = ANGLE(3.0f);
				creature->Flags = 0;

				if (creature->ReachedGoal)
				{
					item->AIBits = 0;
					item->ItemFlags[1] = 1;

					item->Pose = horseItem->Pose;

					creature->ReachedGoal = false;
					creature->Enemy = nullptr;

					item->Animation.AnimNumber = HORSEMAN_ANIM_MOUNT_HORSE;
					item->Animation.FrameNumber = 0;
					item->Animation.ActiveState = HORSEMAN_STATE_MOUNT_HORSE;

					creature->MaxTurn = 0;
					break;
				}

				if (item->HitStatus)
					item->Animation.TargetState = HORSEMAN_STATE_IDLE;
				else if (AI.bite && AI.distance < pow(682, 2))
				{
					if (GetRandomControl() & 1)
						item->Animation.TargetState = HORSEMAN_STATE_WALK_FORWARD_ATTACK_RIGHT;
					else if (GetRandomControl() & 1)
						item->Animation.TargetState = HORSEMAN_STATE_WALK_FORWARD_ATTACK_LEFT;
					else
						item->Animation.TargetState = HORSEMAN_STATE_IDLE;
				}
				else if (AI.distance < pow(BLOCK(5), 2) && AI.distance > pow(1365, 2))
					item->Animation.TargetState = HORSEMAN_STATE_RUN_FORWARD;

				break;

			case HORSEMAN_STATE_RUN_FORWARD:
				if (AI.distance < pow(1365, 2))
					item->Animation.TargetState = HORSEMAN_STATE_WALK_FORWARD;

				break;

			case HORSEMAN_STATE_WALK_FORWARD_ATTACK_RIGHT:
			case HORSEMAN_STATE_WALK_FORWARD_ATTACK_LEFT:
			case HORSEMAN_STATE_IDLE_ATTACK:
				creature->MaxTurn = 0;

				if (abs(AI.angle) >= ANGLE(3.0f))
				{
					if (AI.angle >= 0)
						item->Pose.Orientation.y += ANGLE(3.0f);
					else
						item->Pose.Orientation.y -= ANGLE(3.0f);
				}
				else
					item->Pose.Orientation.y += AI.angle;

				if (!creature->Flags)
				{
					if (item->TouchBits.Test(HorsemanAxeAttackJoints))
					{
						DoDamage(creature->Enemy, 100);
						CreatureEffect2(item, HorsemanBite2, 3, item->Pose.Orientation.y, DoBloodSplat);
						creature->Flags = 1;
					}
				}

				break;

			case HORSEMAN_STATE_SHIELD:
				if (Lara.TargetEntity != item || AI.bite && AI.distance < pow(682, 2))
					item->Animation.TargetState = HORSEMAN_STATE_IDLE;

				break;

			case HORSEMAN_STATE_MOUNTED_SPRINT:
				creature->MaxTurn = ANGLE(3.0f);
				creature->ReachedGoal = false;

				if (!horseItem->Flags)
				{
					if (horseItem->TouchBits & 0xA2000)
					{
						DoDamage(creature->Enemy, 150);

						if (horseItem->TouchBits & 0x2000)
							CreatureEffect2(horseItem, HorseBite1, 10, -1, DoBloodSplat);

						if (horseItem->TouchBits & 0x20000)
							CreatureEffect2(horseItem, HorseBite2, 10, -1, DoBloodSplat);

						if (horseItem->TouchBits & 0x80000)
							CreatureEffect2(horseItem, HorseBite3, 10, -1, DoBloodSplat);

						horseItem->Flags = 1;
					}
				}

				if (!creature->Flags)
				{
					if (item->TouchBits.Test(HorsemanMountedAttackJoints))
					{
						LaraItem->HitStatus = true;

						if (item->TouchBits.Test(HorsemanAxeAttackJoints))
						{
							DoDamage(creature->Enemy, 250);
							CreatureEffect2(horseItem, HorsemanBite1, 20, -1, DoBloodSplat);
						}
						else if (item->TouchBits.Test(HorsemanShieldAttackJoints))
						{
							DoDamage(creature->Enemy, 150);
							CreatureEffect2(horseItem, HorsemanBite3, 10, -1, DoBloodSplat);
						}

						creature->Flags = 1;
					}
				}

				if (item->Animation.AnimNumber == HORSEMAN_ANIM_MOUNTED_SPRINT &&
					item->Animation.FrameNumber == 0)
				{
					horseItem->Animation.AnimNumber = HORSE_ANIM_SPRINT;
					horseItem->Animation.FrameNumber = 0;
				}

				if (laraAI.distance > pow(BLOCK(4), 2) || creature->ReachedGoal)
				{
					creature->ReachedGoal = false;
					creature->Enemy = LaraItem;
					creature->Flags = 0;
				}
				else if (!AI.ahead)
				{
					item->Animation.TargetState = HORSEMAN_STATE_MOUNTED_IDLE;
					horseItem->Animation.TargetState = HORSEMAN_STATE_MOUNTED_RUN_FORWARD;
				}

				break;

			default:
				break;
			}

			if (horseItem && item->ItemFlags[1])
			{
				if (abs(xRot - item->Pose.Orientation.x) < ANGLE(1.4f))
					item->Pose.Orientation.x = xRot;
				else if (xRot <= item->Pose.Orientation.x)
				{
					if (xRot < item->Pose.Orientation.x)
						item->Pose.Orientation.x -= ANGLE(1.4f);
				}
				else
					item->Pose.Orientation.x += ANGLE(1.4f);

				horseItem->Pose = item->Pose;

				if (horseItem->RoomNumber != item->RoomNumber)
					ItemNewRoom(item->ItemFlags[0], item->RoomNumber);
				
				AnimateItem(*horseItem);
			}
		}

		Objects[ID_HORSEMAN].radius = item->ItemFlags[1] != 0 ? 409 : 170;
		CreatureAnimation(itemNumber, angle, 0);
	}
}
