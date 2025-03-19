#include "framework.h"
#include "Objects/TR4/Entity/tr4_skeleton.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/collision/floordata.h"
#include "Game/control/box.h"
#include "Game/control/lot.h"
#include "Game/effects/debris.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "Math/Math.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;
using namespace TEN::Math;

namespace TEN::Entities::TR4
{
	constexpr auto SKELETON_ATTACK_DAMAGE = 80;

	const auto SkeletonBite = CreatureBiteInfo(Vector3(0, -16, 200), 11);
	const auto SkeletonSwordAttackJoints = std::vector<unsigned int>{ 15, 16 };

	enum SkeletonState
	{
		SKELETON_STATE_SUBTERRANEAN = 0,
		SKELETON_STATE_WAIT = 1,
		SKELETON_STATE_IDLE = 2,
		SKELETON_STATE_LOOK_LEFT = 3,
		SKELETON_STATE_LOOK_RIGHT = 4,
		SKELETON_STATE_DODGE_LEFT = 5,
		SKELETON_STATE_DODGE_RIGHT = 6,
		SKELETON_STATE_USE_SHIELD = 7,
		SKELETON_STATE_ATTACK_1 = 8,
		SKELETON_STATE_ATTACK_2 = 9,
		SKELETON_STATE_ATTACK_3 = 10,
		SKELETON_STATE_STUCK_SWORD = 11,
		SKELETON_STATE_RECOIL_FRONT = 12,
		SKELETON_STATE_RECOIL_BACK = 13,
		SKELETON_STATE_STAND_UP = 14,
		SKELETON_STATE_WALK_FORWARD = 15,
		SKELETON_STATE_RUN_FORWARD = 16,
		SKELETON_STATE_BLOCK_ATTACK = 17,
		SKELETON_STATE_WALK_ATTACK = 18,
		SKELETON_STATE_JUMP_LEFT = 19,
		SKELETON_STATE_JUMP_RIGHT = 20,
		SKELETON_STATE_JUMP_FORWARD_1_BLOCK = 21,
		SKELETON_STATE_JUMP_FORWARD_2_BLOCKS = 22,
		SKELETON_STATE_JUMP_CONTINUE = 23,
		SKELETON_STATE_JUMP_START = 24,
		SKELETON_STATE_LAYING_DOWN = 25
	};

	enum SkeletonAnim
	{
		SKELETON_ANIM_EMERGE = 0,
		SKELETON_ANIM_UPRIGHT_IDLE = 1,
		SKELETON_ANIM_UPRIGHT_IDLE_TO_IDLE = 2,
		SKELETON_ANIM_IDLE = 3,
		SKELETON_ANIM_LOOK_LEFT = 4,
		SKELETON_ANIM_LOOK_RIGHT = 5,
		SKELETON_ANIM_DODGE_LEFT = 6,
		SKELETON_ANIM_DODGE_RIGHT = 7,
		SKELETON_ANIM_SWORD_START = 8,
		SKELETON_ANIM_SWORD_LOOP = 9,
		SKELETON_ANIM_SHIELD_IMPACTED = 10,
		SKELETON_ANIM_SHIELD_END = 11,
		SKELETON_ANIM_SWORD_ATTACK_LEFT = 12,
		SKELETON_ANIM_SWORD_ATTACK_RIGHT = 13,
		SKELETON_ANIM_SWORD_ATTACK_FRONT = 14,
		SKELETON_ANIM_SWORD_ATTACK_FRONT_END = 15,
		SKELETON_ANIM_SWORD_ATTACK_FRONT_STUCKED = 16,
		SKELETON_ANIM_HURT_BY_SHOTGUN_FRONT = 17,
		SKELETON_ANIM_STANDING_UP = 18,
		SKELETON_ANIM_IDLE_TO_WALKING_LEFT_STEP = 19,
		SKELETON_ANIM_WALKING_LEFT = 20,
		SKELETON_ANIM_WALKING_RIGHT_STEP = 21,
		SKELETON_ANIM_WALKING_LEFT_ATTACK = 22,
		SKELETON_ANIM_WALKING_RIGHT_ATTACK = 23,
		SKELETON_ANIM_WALKING_LEFT_TO_IDLE = 24,
		SKELETON_ANIM_WALKING_RIGHT_TO_IDLE = 25,
		SKELETON_ANIM_WALKING_LEFT_TO_RUN = 26,
		SKELETON_ANIM_WALKING_RIGHT_TO_RUN = 27,
		SKELETON_ANIM_RUN = 28,
		SKELETON_ANIM_RUN_TO_IDLE_RIGHT = 29,
		SKELETON_ANIM_RUN_TO_IDLE_LEFT = 30,
		SKELETON_ANIM_RUN_TO_WALKING_LEFT = 31,
		SKELETON_ANIM_RUN_TO_WALKING_RIGHT = 32,
		SKELETON_ANIM_HURT_BY_SHOTGUN_BACK = 33,
		SKELETON_ANIM_JUMP_LEFT_START = 34,
		SKELETON_ANIM_JUMP_LEFT_CONTINUE = 35,
		SKELETON_ANIM_JUMP_LEFT_END = 36,
		SKELETON_ANIM_JUMP_RIGHT_START = 37,
		SKELETON_ANIM_JUMP_RIGHT_CONTINUE = 38,
		SKELETON_ANIM_JUMP_RIGHT_END = 39,
		SKELETON_ANIM_JUMP_FORWARD_START = 40,
		SKELETON_ANIM_JUMP_FORWARD_CONTINUE_1_BLOCK = 41,
		SKELETON_ANIM_JUMP_FORWARD_CONTINUE_2_BLOCKS = 42,
		SKELETON_ANIM_JUMP_FORWARD_END = 43,
		SKELETON_ANIM_JUMP_START = 44,
		SKELETON_ANIM_JUMP_CONTINUE = 45,
		SKELETON_ANIM_LAYING_DOWN = 46,
		SKELETON_ANIM_LAYING_DOWN_TO_FALLING = 47,
		SKELETON_ANIM_FALLING = 48
	};

	void InitializeSkeleton(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];
		InitializeCreature(itemNumber);

		// OCBs: Check cases 0 to 3.
		switch (item->TriggerFlags)
		{
		case 0:
			SetAnimation(*item, SKELETON_ANIM_EMERGE);
			break;

		case 1:
			SetAnimation(*item, SKELETON_ANIM_JUMP_RIGHT_START);
			break;

		case 2:
			SetAnimation(*item, SKELETON_ANIM_JUMP_LEFT_START);
			break;

		case 3:
			SetAnimation(*item, SKELETON_ANIM_STANDING_UP);
			item->Status = ITEM_NOT_ACTIVE;
			break;
		}
	}

	void TriggerRiseEffect(ItemInfo* item)
	{
		int fxNumber = CreateNewEffect(item->RoomNumber);
		if (fxNumber == NO_VALUE)
			return;

		auto* fx = &EffectList[fxNumber];

		fx->pos.Position.x = (byte)GetRandomControl() + item->Pose.Position.x - 128;
		fx->pos.Position.y = GetPointCollision(*item).GetFloorHeight();
		fx->pos.Position.z = (byte)GetRandomControl() + item->Pose.Position.z - 128;
		fx->roomNumber = item->RoomNumber;
		fx->pos.Orientation.y = 2 * GetRandomControl();
		fx->speed = GetRandomControl() / 2048;
		fx->fallspeed = -(GetRandomControl() / 1024);
		fx->frameNumber = Objects[103].meshIndex;
		fx->objectNumber = ID_BODY_PART;
		fx->color = Vector4::One;
		fx->flag2 = 0x601;

		auto* spark = GetFreeParticle();
		spark->on = 1;
		spark->sR = 0;
		spark->sG = 0;
		spark->sB = 0;
		spark->dR = 100;
		spark->dG = 60;
		spark->dB = 30;
		spark->fadeToBlack = 8;
		spark->colFadeSpeed = (GetRandomControl() & 3) + 4;
		spark->life = spark->sLife = (GetRandomControl() & 7) + 16;
		spark->x = fx->pos.Position.x;
		spark->y = fx->pos.Position.y;
		spark->z = fx->pos.Position.z;
		spark->xVel = phd_sin(fx->pos.Orientation.y) * 4096;
		spark->yVel = 0;
		spark->zVel = phd_cos(fx->pos.Orientation.y) * 4096;
		spark->blendMode = BlendMode::Additive;
		spark->friction = 68;
		spark->flags = 26;
		spark->rotAng = GetRandomControl() & 0xFFF;

			if (Random::TestProbability(1 / 2.0f))
				spark->rotAdd = -16 - (GetRandomControl() & 0xF);
			else
				spark->rotAdd = (GetRandomControl() & 0xF) + 16;

		spark->gravity = -4 - (GetRandomControl() & 3);
		spark->scalar = 3;
		spark->maxYvel = -4 - (GetRandomControl() & 3);
		spark->sSize = spark->size = (GetRandomControl() & 0x0F) + 8;
		spark->dSize = spark->size * 4;
	}

	void SkeletonControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);
		auto* enemyItem = creature->Enemy;

		bool jumpLeft = false;
		bool jumpRight = false;

		short angle = 0;
		short tilt = 0;
		short joint1 = 0;
		short joint2 = 0;
		short joint3 = 0;
		short rotation = 0;
		int distance = 0;

		// Can skeleton jump? Check for a distance of 1 and 2 blocks.
		int x = item->Pose.Position.x;
		int y = item->Pose.Position.y;
		int z = item->Pose.Position.z;

		int dx = 870 * phd_sin(item->Pose.Orientation.y);
		int dz = 870 * phd_cos(item->Pose.Orientation.y);

		x += dx;
		z += dz;
		int height1 = GetPointCollision(Vector3i(x, y, z), item->RoomNumber).GetFloorHeight();

		x += dx;
		z += dz;
		int height2 = GetPointCollision(Vector3i(x, y, z), item->RoomNumber).GetFloorHeight();

		x += dx;
		z += dz;
		int height3 = GetPointCollision(Vector3i(x, y, z), item->RoomNumber).GetFloorHeight();

		int height = 0;
		bool canJump1Block = true;
		if (enemyItem && item->BoxNumber == LaraItem->BoxNumber && (item->MeshBits & 0x200) ||
			y >= (height1 - CLICK(1.5f)) ||
			y >= (height2 + CLICK(2)) ||
			y <= (height2 - CLICK(2)))
		{
			height = height2;
			canJump1Block = false;
		}

		bool canJump2Blocks = true;
		if (enemyItem && item->BoxNumber == LaraItem->BoxNumber && (item->MeshBits & 0x200) ||
			y >= (height1 - CLICK(1.5f)) ||
			y >= (height - CLICK(1.5f)) ||
			y >= (height3 + CLICK(2)) ||
			y <= (height3 - CLICK(2)))
		{
			canJump2Blocks = false;
		}

		if (item->AIBits)
			GetAITarget(creature);
		else
			creature->Enemy = LaraItem;

		AI_INFO AI;
		CreatureAIInfo(item, &AI);

		if (item->HitStatus &&
			Lara.Control.Weapon.GunType == LaraWeaponType::Shotgun &&
			AI.distance < SQUARE(BLOCK(3.5f)) &&
			item->Animation.ActiveState != SKELETON_STATE_USE_SHIELD &&
			item->Animation.ActiveState != 17 &&
			item->Animation.ActiveState != SKELETON_STATE_RECOIL_FRONT &&
			item->Animation.ActiveState != SKELETON_STATE_RECOIL_BACK &&
			item->Animation.ActiveState != SKELETON_STATE_LAYING_DOWN)
		{
			if (AI.angle >= ANGLE(67.5f) || AI.angle <= -ANGLE(67.5f))
			{
				item->Animation.ActiveState = SKELETON_STATE_RECOIL_BACK;
				item->Animation.AnimNumber = 33;
				item->Pose.Orientation.y += AI.angle - ANGLE(180.0f);
			}
			else
			{
				item->Animation.ActiveState = SKELETON_STATE_RECOIL_FRONT;
				item->Animation.AnimNumber = 17;
				item->Pose.Orientation.y += AI.angle;
			}

			item->Animation.FrameNumber = 0;
			creature->LOT.IsJumping = true;
		}
		else
		{
			AI_INFO laraAI;
			if (creature->Enemy->IsLara())
			{
				laraAI.distance = AI.distance;
				laraAI.angle = AI.angle;
			}
			else
			{
				dx = LaraItem->Pose.Position.x - item->Pose.Position.x;
				dz = LaraItem->Pose.Position.z - item->Pose.Position.z;
				laraAI.angle = phd_atan(dz, dx) - item->Pose.Orientation.y;
				laraAI.distance = SQUARE(dx) + SQUARE(dz);
			}

			GetCreatureMood(item, &AI, true);

			if (!(item->MeshBits & 0x200))
				creature->Mood = MoodType::Escape;

			CreatureMood(item, &AI, true);

			angle = CreatureTurn(item, creature->MaxTurn);

			auto* tempEnemy = creature->Enemy;

			creature->Enemy = LaraItem;

			if (item->HitStatus ||
				distance < SQUARE(BLOCK(1)) ||
				TargetVisible(item, &laraAI))
			{
				creature->Alerted = true;
			}

			creature->Enemy = tempEnemy;

			if (item != Lara.TargetEntity || laraAI.distance <= 870 || angle <= -ANGLE(56.25f) || angle >= ANGLE(56.25f))
			{
				jumpLeft = false;
				jumpRight = false;
			}
			else
			{
				dx = 870 * phd_sin(item->Pose.Orientation.y + ANGLE(45.0f));
				dz = 870 * phd_cos(item->Pose.Orientation.y + ANGLE(45.0f));

				x = item->Pose.Position.x + dx;
				y = item->Pose.Position.y;
				z = item->Pose.Position.z + dz;
				int height4 = GetPointCollision(Vector3i(x, y, z), item->RoomNumber).GetFloorHeight();

				dx = 870 * phd_sin(item->Pose.Orientation.y + ANGLE(78.75f));
				dz = 870 * phd_cos(item->Pose.Orientation.y + ANGLE(78.75f));

				x = item->Pose.Position.x + dx;
				y = item->Pose.Position.y;
				z = item->Pose.Position.z + dz;
				int height5 = GetPointCollision(Vector3i(x, y, z), item->RoomNumber).GetFloorHeight();

				if (abs(height5 - item->Pose.Position.y) > CLICK(1))
					jumpRight = false;
				else
				{
					jumpRight = true;

					if ((height4 + CLICK(2)) >= item->Pose.Position.y)
						jumpRight = false;
				}

				dx = 870 * phd_sin(item->Pose.Orientation.y - ANGLE(45.0f));
				dz = 870 * phd_cos(item->Pose.Orientation.y - ANGLE(45.0f));

				x = item->Pose.Position.x + dx;
				y = item->Pose.Position.y;
				z = item->Pose.Position.z + dz;
				int height6 = GetPointCollision(Vector3i(x, y, z), item->RoomNumber).GetFloorHeight();

				dx = 870 * phd_sin(item->Pose.Orientation.y - ANGLE(78.75f));
				dz = 870 * phd_cos(item->Pose.Orientation.y - ANGLE(78.75f));

				x = item->Pose.Position.x + dx;
				y = item->Pose.Position.y;
				z = item->Pose.Position.z + dz;
				int height7 = GetPointCollision(Vector3i(x, y, z), item->RoomNumber).GetFloorHeight();

				if (abs(height7 - item->Pose.Position.y) > CLICK(1) || height6 + CLICK(2) >= item->Pose.Position.y)
					jumpLeft = false;
				else
					jumpLeft = true;
			}

			switch (item->Animation.ActiveState)
			{
			case SKELETON_STATE_WAIT:
				if (Random::TestProbability(3 / 50.0f))
					item->Animation.TargetState = 2;
				
				break;

			case SKELETON_STATE_IDLE:
				creature->MaxTurn = (creature->Mood != MoodType::Escape) ? ANGLE(2.0f) : 0;
				creature->LOT.IsJumping = false;
				creature->Flags = 0;

				if (item->AIBits & GUARD ||
					Random::TestProbability(1 / 32.0f) &&
					(AI.distance > SQUARE(BLOCK(1)) ||
						creature->Mood != MoodType::Attack))
				{
					if (Random::TestProbability(1 / 64.0f))
					{
						if (Random::TestProbability(1 / 2.0f))
							item->Animation.TargetState = 3;
						else
							item->Animation.TargetState = 4;
					}
				}
				else
				{
					if (item->AIBits & PATROL1)
						item->Animation.TargetState = 15;
					else if (canJump1Block || canJump2Blocks)
					{
						item->Animation.AnimNumber = 40;
						item->Animation.FrameNumber = 0;
						item->Animation.ActiveState = SKELETON_STATE_JUMP_LEFT;
						creature->MaxTurn = 0;

						if (!canJump2Blocks)
						{
							item->Animation.TargetState = SKELETON_STATE_JUMP_FORWARD_1_BLOCK;
							creature->LOT.IsJumping = true;
						}
						else
						{
							item->Animation.TargetState = SKELETON_STATE_JUMP_FORWARD_2_BLOCKS;
							creature->LOT.IsJumping = true;
						}
					}
					else if (jumpLeft)
						SetAnimation(*item, SKELETON_ANIM_JUMP_LEFT_START);
					else if (jumpRight)
						SetAnimation(*item, SKELETON_ANIM_JUMP_RIGHT_START);
					else
					{
						if (creature->Mood == MoodType::Escape)
						{
							if (Lara.TargetEntity == item ||
								!AI.ahead || item->HitStatus ||
								!(item->MeshBits & 0x200))
							{
								item->Animation.TargetState = 15;
								break;
							}

							item->Animation.TargetState = 2;
						}
						else if (creature->Mood == MoodType::Bored ||
							item->AIBits & FOLLOW &&
							(creature->ReachedGoal || laraAI.distance > SQUARE(BLOCK(2))))
						{
							if (item->Animation.RequiredState != NO_VALUE)
								item->Animation.TargetState = item->Animation.RequiredState;
							else if (Random::TestProbability(1 / 64.0f))
								item->Animation.TargetState = 15;
						}
						else if (Lara.TargetEntity == item &&
							laraAI.angle && laraAI.distance < SQUARE(BLOCK(2)) &&
							Random::TestProbability(1 / 2.0f) &&
							(Lara.Control.Weapon.GunType == LaraWeaponType::Shotgun || Random::TestProbability(3 / 50.0f)) &&
							item->MeshBits == -1)
						{
							item->Animation.TargetState = SKELETON_STATE_USE_SHIELD;
						}
						else if (AI.bite && AI.distance < SQUARE(682))
						{
							if (Random::TestProbability(3 / 4.0f) && LaraItem->HitPoints > 0)
							{
								if (Random::TestProbability(1 / 2.0f))
									item->Animation.TargetState = SKELETON_STATE_ATTACK_1;
								else
									item->Animation.TargetState = SKELETON_STATE_ATTACK_2;
							}
							else
								item->Animation.TargetState = SKELETON_STATE_ATTACK_3;
						}
						else if (item->HitStatus || item->Animation.RequiredState != NO_VALUE)
						{
							if (Random::TestProbability(1 / 2.0f))
							{
								item->Animation.TargetState = SKELETON_STATE_DODGE_LEFT;
								item->Animation.RequiredState = item->Animation.TargetState;
							}
							else
							{
								item->Animation.TargetState = SKELETON_STATE_DODGE_RIGHT;
								item->Animation.RequiredState = item->Animation.TargetState;
							}
						}
						else
							item->Animation.TargetState = 15;
					}
				}

				break;

			case SKELETON_STATE_WALK_FORWARD:
				creature->MaxTurn = (creature->Mood != MoodType::Bored) ? ANGLE(6.0f) : ANGLE(2.0f);
				creature->LOT.IsJumping = false;
				creature->Flags = 0;

				if (item->AIBits & PATROL1)
					item->Animation.TargetState = 15;
				else if (item->HitStatus)
				{
					item->Animation.TargetState = 2;
					if (Random::TestProbability(1 / 2.0f))
						item->Animation.RequiredState = 5;
					else
						item->Animation.RequiredState = 6;
				}
				else
				{
					if (jumpLeft || jumpRight)
					{
						item->Animation.TargetState = 2;
						break;
					}
					if (creature->Mood == MoodType::Escape)
						item->Animation.TargetState = 16;
					else if (creature->Mood != MoodType::Bored)
					{
						if (AI.distance >= SQUARE(682))
						{
							if (AI.bite && AI.distance < SQUARE(BLOCK(1)))
								item->Animation.TargetState = 18;
							else if (canJump1Block || canJump2Blocks)
							{
								item->Animation.TargetState = 2;
								creature->MaxTurn = 0;
							}
							else if (!AI.ahead || AI.distance > SQUARE(BLOCK(2)))
								item->Animation.TargetState = 16;
						}
						else
							item->Animation.TargetState = 2;
					}
					else if (Random::TestProbability(1 / 64.0f))
						item->Animation.TargetState = 2;
				}

				break;

			case SKELETON_STATE_RUN_FORWARD:
				creature->MaxTurn = ANGLE(7.0f);
				creature->LOT.IsJumping = false;

				if (item->AIBits & GUARD || canJump1Block || canJump2Blocks)
				{
					if (item->MeshBits & 0x200)
					{
						item->Animation.TargetState = 2;
						creature->MaxTurn = 0;
						break;
					}

					creature->LOT.IsJumping = true;

					if (GetPointCollision(*item).GetFloorHeight() > item->Pose.Position.y + BLOCK(1))
					{
						item->Animation.AnimNumber = 44;
						item->Animation.FrameNumber = 0;
						item->Animation.ActiveState = 23;
						item->Animation.IsAirborne = true;
						creature->MaxTurn = 0;
						creature->LOT.IsJumping = false;
					}
				}
				else
				{
					if (creature->Mood == MoodType::Escape)
					{
						if (Lara.TargetEntity != item && AI.ahead && (item->MeshBits & 0x200))
							item->Animation.TargetState = 2;
					}
					else if (item->AIBits & FOLLOW && (creature->ReachedGoal || laraAI.distance > SQUARE(BLOCK(2))))
						item->Animation.TargetState = 2;
					else if (creature->Mood != MoodType::Bored)
					{
						if (AI.ahead && AI.distance < SQUARE(BLOCK(2)))
							item->Animation.TargetState = 15;
					}
					else
						item->Animation.TargetState = 15;
				}

				break;

			case SKELETON_STATE_ATTACK_3:
				creature->MaxTurn = 0;

				if (abs(AI.angle) >= ANGLE(6.0f))
				{
					if (AI.angle >= 0)
						item->Pose.Orientation.y += ANGLE(6.0f);
					else
						item->Pose.Orientation.y -= ANGLE(6.0f);
				}
				else
				{
					item->Pose.Orientation.y += AI.angle;
				}

				if (!creature->Flags)
				{
					if (item->TouchBits.Test(SkeletonSwordAttackJoints))
					{
						DoDamage(creature->Enemy, SKELETON_ATTACK_DAMAGE);
						CreatureEffect2(item, SkeletonBite, 15, -1, DoBloodSplat);
						SoundEffect(SFX_TR4_LARA_THUD, &item->Pose);
						creature->Flags = 1;
					}
				}

				if (Random::TestProbability(1 / 64.0f) || LaraItem->HitPoints <= 0)
					item->Animation.TargetState = 11;
				
				break;

			case SKELETON_STATE_ATTACK_1:
			case SKELETON_STATE_ATTACK_2:
			case SKELETON_STATE_WALK_ATTACK:
				creature->MaxTurn = 0;

				if (abs(AI.angle) >= ANGLE(6.0f))
				{
					if (AI.angle >= 0)
						item->Pose.Orientation.y += ANGLE(6.0f);
					else
						item->Pose.Orientation.y -= ANGLE(6.0f);
				}
				else
				{
					item->Pose.Orientation.y += AI.angle;
				}

				if (item->Animation.FrameNumber > 15)
				{
					auto* room = &g_Level.Rooms[item->RoomNumber];

					auto pos = GetJointPosition(item, 16);

					auto& sector = GetPointCollision(Vector3i(x, y, z), item->RoomNumber).GetSector();
					if (sector.Stopper)
					{
						for (int i = 0; i < room->mesh.size(); i++)
						{
							auto* staticMesh = &room->mesh[i];

							if (abs(pos.x - staticMesh->pos.Position.x) < BLOCK(1) && 
								abs(pos.z - staticMesh->pos.Position.z) < BLOCK(1) &&
								Statics[staticMesh->staticNumber].shatterType != ShatterType::None)
							{
								ShatterObject(0, staticMesh, -128, LaraItem->RoomNumber, 0);
								SoundEffect(SFX_TR4_SMASH_ROCK, &item->Pose);
								sector.Stopper = false;
								TestTriggers(item, true);
								break;
							}
						}
					}

					if (!creature->Flags)
					{
						if (item->TouchBits.Test(SkeletonSwordAttackJoints))
						{
							DoDamage(creature->Enemy, SKELETON_ATTACK_DAMAGE);
							CreatureEffect2(item, SkeletonBite, 10, item->Pose.Orientation.y, DoBloodSplat);
							SoundEffect(SFX_TR4_LARA_THUD, &item->Pose);
							creature->Flags = 1;
						}
					}
				}

				break;

			case SKELETON_STATE_USE_SHIELD:
				if (item->HitStatus)
				{
					if (item->MeshBits == -1 && laraAI.angle &&
						Lara.Control.Weapon.GunType == LaraWeaponType::Shotgun)
					{
						if (Random::TestProbability(3 / 4.0f))
							item->Animation.TargetState = 17;
						else
							ExplodeItemNode(item, 11, 1, -24);
					}
					else
					{
						item->Animation.TargetState = 2;
					}
				}
				else if (Lara.TargetEntity != item || item->MeshBits != -1 ||
					Lara.Control.Weapon.GunType != LaraWeaponType::Shotgun || Random::TestProbability(1 / 128.0f))
				{
					item->Animation.TargetState = 2;
				}
				
				break;

			case SKELETON_STATE_JUMP_FORWARD_1_BLOCK:
				if (item->Animation.AnimNumber == 43)
				{
					if (GetPointCollision(*item).GetFloorHeight() > (item->Pose.Position.y + CLICK(5)))
					{
						item->Animation.AnimNumber = 44;
						item->Animation.FrameNumber = 0;
						item->Animation.ActiveState = 23;
						item->Animation.IsAirborne = true;
						creature->MaxTurn = 0;
						creature->LOT.IsJumping = false;
					}
				}

				break;

			case SKELETON_STATE_JUMP_CONTINUE:
			case SKELETON_STATE_JUMP_START:
				if (GetPointCollision(*item).GetFloorHeight() <= item->Pose.Position.y)
				{
					if (item->Active)
					{
						ExplodingDeath(itemNumber, 0);
						KillItem(itemNumber);
						DisableEntityAI(itemNumber);
						//Savegame.Kills++;
					}
				}

				break;

			case SKELETON_STATE_LAYING_DOWN:
			case SKELETON_STATE_STUCK_SWORD:
			case SKELETON_STATE_RECOIL_FRONT:
			case SKELETON_STATE_RECOIL_BACK:
				if ((item->Animation.ActiveState == SKELETON_STATE_RECOIL_FRONT ||
					item->Animation.ActiveState == SKELETON_STATE_RECOIL_BACK) &&
					item->Animation.FrameNumber < 20)
				{
					creature->MaxTurn = 0;
					break;
				}

				if (item->Animation.ActiveState == 11)
				{
					creature->MaxTurn = 0;
					break;
				}

				creature->LOT.IsJumping = false;

				if (GetPointCollision(*item).GetFloorHeight() <= (item->Pose.Position.y + BLOCK(1)))
				{
					if (Random::TestProbability(1 / 32.0f))
						item->Animation.TargetState = 14;
				}
				else
				{
					item->Animation.AnimNumber = 47;
					item->Animation.FrameNumber = 0;
					item->Animation.ActiveState = 24;
					item->Animation.IsAirborne = true;
					creature->MaxTurn = 0;
				}

				break;

			case SKELETON_STATE_JUMP_LEFT:
			case SKELETON_STATE_JUMP_RIGHT:
				item->Status = ITEM_ACTIVE;
				creature->MaxTurn = 0;
				creature->Alerted = false;

				break;

			case SKELETON_STATE_SUBTERRANEAN:
				if (item->Animation.FrameNumber < 32)
					TriggerRiseEffect(item);
				
				break;

			default:
				break;
			}

			CreatureAnimation(itemNumber, angle, 0);
		}
	}
}
