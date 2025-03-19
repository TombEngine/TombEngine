#include "framework.h"
#include "Objects/TR5/Entity/tr5_roman_statue.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_room.h"
#include "Game/control/box.h"
#include "Game/effects/debris.h"
#include "Game/effects/effects.h"
#include "Game/effects/Electricity.h"
#include "Game/effects/spark.h"
#include "Game/effects/tomb4fx.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Room;
using namespace TEN::Effects::Electricity;
using namespace TEN::Effects::Spark;
using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR5
{
	constexpr auto ROMAN_STATUE_GRENADE_SUPER_AMMO_LIMITER = 2.0f;
	constexpr auto ROMAN_STATUE_EXPLOSIVE_DAMAGE_COEFF	   = 2.0f;

	const auto RomanStatueBite = CreatureBiteInfo(Vector3::Zero, 15);

	struct RomanStatueInfo
	{
		Vector3i Position = Vector3i::Zero;
		Electricity* EnergyArcs[8] = {};
		unsigned int Count = 0;
	};

	RomanStatueInfo RomanStatueData;

	// TODO
	enum RomanStatueState
	{
		// No state 0.
		STATUE_STATE_IDLE = 1,
		STATUE_STATE_SCREAM = 2,
		STATUE_STATE_ATTACK_1 = 3,
		STATUE_STATE_ATTACK_2 = 4,
		STATUE_STATE_ATTACK_3 = 5,
		STATUE_STATE_HIT = 6,
		STATUE_STATE_WALK = 7,

		STATUE_STATE_ATTACK_4 = 9,
		STATUE_STATE_TURN_180 = 10,
		STATUE_STATE_DEATH = 11,
		STATUE_STATE_ENERGY_ATTACK = 12
	};

	// TODO
	enum RomanStatueAnim
	{
		STATUE_ANIM_RECOIL = 5,
		STATUE_ANIM_DEATH = 14,
		STATUE_ANIM_START_JUMP_DOWN = 16
	};

	enum RomanStatueHitMeshFlags
	{
		MS_LIGHT_DMG  = 0x10,
		MS_MEDIUM_DMG = 0x410,
		MS_HARD_DMG	  = 0x510,
		MS_HEAVY_DMG  = 0x10510
	};

	static void RomanStatueHitEffect(ItemInfo* item, Vector3i* pos, int joint)
	{
		*pos = GetJointPosition(item, joint, *pos);

		if (!(GetRandomControl() & 0x1F))
		{
			int fxNumber = CreateNewEffect(item->RoomNumber);
			if (fxNumber != NO_VALUE)
			{
				auto* fx = &EffectList[fxNumber];

				fx->pos.Position = *pos;
				fx->roomNumber = item->RoomNumber;
				fx->pos.Orientation.z = 0;
				fx->pos.Orientation.x = 0;
				fx->pos.Orientation.y = 2 * GetRandomControl();
				fx->speed = 1;
				fx->fallspeed = 0;
				fx->objectNumber = ID_BODY_PART;
				fx->color = Vector4::One;
				fx->flag2 = 9729;
				fx->frameNumber = Objects[ID_BUBBLES].meshIndex + (GetRandomControl() & 7);
				fx->counter = 0;
				fx->flag1 = 0;
			}
		}

		if (!(GetRandomControl() & 0xF))
		{
			auto* spark = &SmokeSparks[GetFreeSmokeSpark()];

			spark->on = 1;
			spark->sShade = 0;
			spark->colFadeSpeed = 4;
			spark->fadeToBlack = 32;
			spark->dShade = (GetRandomControl() & 0xF) + 64;
			spark->blendMode = BlendMode::Additive;
			spark->life = spark->sLife = (GetRandomControl() & 3) + 64;
			spark->position = Vector3i(
				(GetRandomControl() & 0x1F) + pos->x - 16,
				(GetRandomControl() & 0x1F) + pos->y - 16,
				(GetRandomControl() & 0x1F) + pos->z - 16
			);
			spark->velocity = Vector3i(
				(GetRandomControl() & 0x7F) - 64,
				0,
				(GetRandomControl() & 0x7F) - 64
			);
			spark->friction = 4;
			spark->flags = SP_ROTATE;
			spark->rotAng = GetRandomControl() & 0xFFF;
			spark->rotAdd = (GetRandomControl() & 0x1F) - 16;
			spark->maxYvel = 0;
			spark->gravity = (GetRandomControl() & 7) + 8;
			spark->sSize = spark->size = (GetRandomControl() & 7) + 8;
			spark->dSize = spark->size * 2;
		}
	}

	static void TriggerRomanStatueShockwaveAttackSparks(int x, int y, int z, byte r, byte g, byte b, byte size)
	{
		auto* spark = GetFreeParticle();

		spark->dG = g;
		spark->sG = g;
		spark->colFadeSpeed = 2;
		spark->dR = r;
		spark->sR = r;
		spark->blendMode = BlendMode::Additive;
		spark->life = 16;
		spark->sLife = 16;
		spark->x = x;
		spark->on = 1;
		spark->dB = b;
		spark->sB = b;
		spark->fadeToBlack = 4;
		spark->y = y;
		spark->z = z;
		spark->zVel = 0;
		spark->yVel = 0;
		spark->xVel = 0;
		spark->flags = SP_SCALE | SP_DEF;
		spark->scalar = 3;
		spark->maxYvel = 0;
		spark->SpriteSeqID = ID_DEFAULT_SPRITES;
		spark->SpriteID = SPR_LENS_FLARE_LIGHT;
		spark->gravity = 0;
		spark->dSize = spark->sSize = spark->size = size + (GetRandomControl() & 3);
	}

	static void TriggerRomanStatueAttackEffect1(short itemNum, int factor)
	{
		auto* spark = GetFreeParticle();

		spark->on = true;
		spark->sR = 0;
		spark->sB = (GetRandomControl() & 0x3F) - 96;
		spark->dB = (GetRandomControl() & 0x3F) - 96;
		spark->dR = 0;

		if (factor < 16)
		{
			spark->sB = (factor * spark->sB) / 16;
			spark->dB = (factor * spark->dB) / 16;
		}

		spark->sG = spark->sB / 2;
		spark->dG = spark->dB / 2;
		spark->fadeToBlack = 4;
		spark->colFadeSpeed = (GetRandomControl() & 3) + 8;
		spark->blendMode = BlendMode::Additive;
		spark->dynamic = -1;
		spark->life = spark->sLife = (GetRandomControl() & 3) + 32;
		spark->y = 0;
		spark->x = (GetRandomControl() & 0x1F) - 16;
		spark->z = (GetRandomControl() & 0x1F) - 16;
		spark->yVel = 0;
		spark->xVel = (byte)GetRandomControl() - 128;
		spark->friction = 4;
		spark->zVel = (byte)GetRandomControl() - 128;
		spark->flags = SP_NODEATTACH | SP_EXPDEF | SP_ITEM | SP_ROTATE | SP_DEF | SP_SCALE; // 4762;
		spark->fxObj = itemNum;
		spark->nodeNumber = 6;
		spark->rotAng = GetRandomControl() & 0xFFF;
		spark->rotAdd = (GetRandomControl() & 0x3F) - 32;
		spark->maxYvel = 0;
		spark->gravity = -8 - (GetRandomControl() & 7);
		spark->scalar = 2;
		spark->dSize = 4;
		spark->sSize = (spark->size = factor * ((GetRandomControl() & 0x1F) + 64)) / 16;
	}

	static void RomanStatueAttack(Pose* pos, short roomNumber, short count)
	{
		int fxNumber = CreateNewEffect(roomNumber);
		if (fxNumber == NO_VALUE)
			return;

		auto* fx = &EffectList[fxNumber];

		fx->pos.Position.x = pos->Position.x;
		fx->pos.Position.y = pos->Position.y;
		fx->pos.Position.z = pos->Position.z;
		fx->pos.Orientation.x = pos->Orientation.x;
		fx->pos.Orientation.y = pos->Orientation.y;
		fx->pos.Orientation.z = 0;
		fx->roomNumber = roomNumber;
		fx->counter = 16 * count + 15;
		fx->flag1 = 1;
		fx->objectNumber = ID_BUBBLES;
		fx->speed = (GetRandomControl() & 0x1F) + 64;
		fx->frameNumber = Objects[ID_BUBBLES].meshIndex + 8;
	}

	void TriggerRomanStatueMissileSparks(Vector3i* pos, char fxObject)
	{
		auto* spark = GetFreeParticle();

		spark->on = true;
		spark->sR = 0;
		spark->sG = (GetRandomControl() & 0x3F) - 96;
		spark->sB = spark->sG / 2;
		spark->dR = 0;
		spark->dG = (GetRandomControl() & 0x3F) - 96;
		spark->dB = spark->dG / 2;
		spark->fadeToBlack = 8;
		spark->colFadeSpeed = (GetRandomControl() & 3) + 8;
		spark->blendMode = BlendMode::Additive;
		spark->dynamic = -1;
		spark->life = spark->sLife = (GetRandomControl() & 3) + 20;
		spark->x = (GetRandomControl() & 0xF) - 8;
		spark->y = (GetRandomControl() & 0xF) - 8;
		spark->z = (GetRandomControl() & 0xF) - 8;
		spark->xVel = (GetRandomControl() & 0x3FF) - 512;
		spark->yVel = (GetRandomControl() & 0x3FF) - 512;
		spark->zVel = (GetRandomControl() & 0x3FF) - 512;
		spark->friction = 68;
		spark->flags = SP_ROTATE | SP_FX | SP_ROTATE | SP_DEF | SP_SCALE;
		spark->rotAng = GetRandomControl() & 0xFFF;
		spark->gravity = 0;
		spark->maxYvel = 0;
		spark->rotAdd = (GetRandomControl() & 0x3F) - 32;
		spark->fxObj = fxObject;
		spark->scalar = 2;
		spark->sSize = spark->size = (GetRandomControl() & 0x0F) + 96;
		spark->dSize = spark->size / 4;
	}

	void InitializeRomanStatue(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(*item, STATUE_ANIM_START_JUMP_DOWN);
		item->Status = ITEM_NOT_ACTIVE;
		item->Pose.Position.x += 486 * phd_sin(item->Pose.Orientation.y + ANGLE(90.0f));
		item->Pose.Position.z += 486 * phd_cos(item->Pose.Orientation.y + ANGLE(90.0f));

		ZeroMemory(&RomanStatueData, sizeof(RomanStatueInfo));
	}

	void RomanStatueControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short headingAngle = 0;
		short joint0 = 0;
		short joint1 = 0;
		short joint2 = 0;
		auto prevMeshSwapBits = item->Model.MeshIndex;

		// At determined HP values, the statue sheds material.
		if (item->HitPoints < 1 && !item->TestMeshSwapFlags(MS_HEAVY_DMG))
		{
			ExplodeItemNode(item, 16, 0, 8);
			item->MeshBits |= MS_HEAVY_DMG;
			item->SetMeshSwapFlags(MS_HEAVY_DMG);
		}
		else if (item->HitPoints < 75 && !item->TestMeshSwapFlags(MS_HARD_DMG))
		{
			ExplodeItemNode(item, 8, 0, 8);
			item->MeshBits |= MS_HARD_DMG;
			item->SetMeshSwapFlags(MS_HARD_DMG);
		}
		else if (item->HitPoints < 150 && !item->TestMeshSwapFlags(MS_MEDIUM_DMG))
		{
			ExplodeItemNode(item, 10, 0, 32);
			ExplodeItemNode(item, 11, 0, 32);
			item->MeshBits |= MS_MEDIUM_DMG;
			item->SetMeshSwapFlags(MS_MEDIUM_DMG);
		}
		else if (item->HitPoints < 225 && !item->TestMeshSwapFlags(MS_LIGHT_DMG))
		{
			ExplodeItemNode(item, 4, 0, 8);
			item->MeshBits |= MS_LIGHT_DMG;
			item->SetMeshSwapFlags(MS_LIGHT_DMG);
		}

		// Set recoil animation.
		if (prevMeshSwapBits != item->Model.MeshIndex)
			SetAnimation(*item, STATUE_ANIM_RECOIL);

		if (item->HitPoints > 0)
		{
			creature->Enemy = LaraItem;

			AI_INFO ai;
			CreatureAIInfo(item, &ai);

			GetCreatureMood(item, &ai, true);
			CreatureMood(item, &ai, true);

			headingAngle = CreatureTurn(item, creature->MaxTurn);

			if (ai.ahead)
			{
				joint0 = ai.angle / 2;
				joint2 = ai.angle / 2;
				joint1 = ai.xAngle;
			}

			creature->MaxTurn = 0;

			Vector3i pos, pos1, pos2;
			byte color;
			int deltaFrame;
			bool unknown;
			
			color = (GetRandomControl() & 0x3F) + 128;

			switch (item->Animation.ActiveState)
			{
			case STATUE_STATE_IDLE:
				creature->Flags = 0;
				joint2 = ai.angle;

				if (creature->Mood == MoodType::Attack)
				{
					creature->MaxTurn = ANGLE(2.0f);
				}
				else
				{
					item->Animation.TargetState = STATUE_STATE_WALK;
					creature->MaxTurn = 0;
				}

				if (item->AIBits ||
					!(GetRandomControl() & 0x1F) &&
					(ai.distance > pow(BLOCK(1), 2) ||
						creature->Mood != MoodType::Attack))
				{
					joint2 = AIGuard((CreatureInfo*)creature);
				}
				else if (ai.angle > ANGLE(112.5f) || ai.angle < -ANGLE(112.5f))
				{
					item->Animation.TargetState = STATUE_STATE_TURN_180;
				}
				else if (ai.ahead && ai.distance < pow(BLOCK(1), 2))
				{
					if (ai.bite & ((GetRandomControl() & 3) == 0))
						item->Animation.TargetState = STATUE_STATE_ATTACK_1;
					else if (GetRandomControl() & 1)
						item->Animation.TargetState = STATUE_STATE_ATTACK_2;
					else
						item->Animation.TargetState = STATUE_STATE_ATTACK_3;
				}
				else
				{
					if (!item->ItemFlags[0])
					{
						item->Animation.TargetState = STATUE_STATE_SCREAM;
						item->ItemFlags[0] = 5;
						break;
					}

					if (item->TriggerFlags == 1)
					{
						if (Targetable(item, &ai) && Random::TestProbability(1 / 2.0f))
						{
							item->Animation.TargetState = STATUE_STATE_ENERGY_ATTACK;
							break;
						}
					}

					if (item->TriggerFlags || ai.distance >= pow(BLOCK(2.5f), 2) || !ai.bite)
					{
						item->Animation.TargetState = STATUE_STATE_WALK;
						break;
					}

					item->Animation.TargetState = STATUE_STATE_ATTACK_1;
				}

				break;

			case STATUE_STATE_SCREAM:
				unknown = false;

				pos1 = GetJointPosition(item, 14, Vector3i(-32, 48, 64));
				pos2 = GetJointPosition(item, 14, Vector3i(-48, 48, 490));

				pos = Vector3i((pos1.x + pos2.x) / 2, (pos1.y + pos2.y) / 2, (pos1.z + pos2.z) / 2);

				deltaFrame = item->Animation.FrameNumber;

				if (deltaFrame > 68 && deltaFrame < 130)
				{
					int deltaFrame2 = deltaFrame - 68;
					if (deltaFrame2 <= 58)
					{
						if (deltaFrame2 > 16)
							deltaFrame2 = 16;
					}
					else
					{
						deltaFrame2 = 4 * (62 - deltaFrame);
					}

					color = (deltaFrame2 * ((GetRandomControl() & 0x3F) + 128)) / 16;

					if (item->TriggerFlags)
						SpawnDynamicLight(pos2.x, pos2.y, pos2.z, 16, 0, color, color / 2);
					else
						SpawnDynamicLight(pos2.x, pos2.y, pos2.z, 16, 0, color / 2, color);

					for (int i = 0; i < 2; i++)
					{
						if (item->TriggerFlags)
							TriggerAttackSpark(pos2.ToVector3(), Vector3(0, color, color / 2));
						else
							TriggerAttackSpark(pos2.ToVector3(), Vector3(0, color / 2, color));
					}				
				}

				if (deltaFrame <= 90 || deltaFrame >= 130)
					break;

				if (item->TriggerFlags)
					pos = Vector3i(-48, 48, GetRandomControl() % 480);
				else
					pos = Vector3i(-40, 64, GetRandomControl() % 360);
				pos = GetJointPosition(item, 14, pos);

				pos2 = GetJointPosition(item, 14, Vector3i(-48, 48, 450));

				pos1.x = (GetRandomControl() & 0xFFF) + item->Pose.Position.x - BLOCK(2);
				pos1.y = item->Pose.Position.y - (GetRandomControl() & 0x3FF) - BLOCK(4);
				pos1.z = (GetRandomControl() & 0xFFF) + item->Pose.Position.z - BLOCK(2);

				for (int i = 0; i < 8; i++)
				{
					auto* arc = RomanStatueData.EnergyArcs[i];

					if (arc && arc->life)
					{
						arc->pos4.x = pos2.x;
						arc->pos4.y = pos2.y;
						arc->pos4.z = pos2.z;

						if (item->TriggerFlags)
							SpawnElectricityGlow(pos.ToVector3(), 16, 0, color, color / 2);
						else
							SpawnElectricityGlow(pos.ToVector3(), 16, 0, color / 2, color);

						continue;
					}

					if (!(GlobalCounter & 3) || unknown)
					{
						unknown = true;
						continue;
					}

					if (item->TriggerFlags)
					{
						SpawnElectricity(
							pos1.ToVector3(), pos.ToVector3(),
							Random::GenerateInt(64, 80),
							0, color, color / 2,
							50, (int)ElectricityFlags::ThinIn | (int)ElectricityFlags::Spline | (int)ElectricityFlags::ThinOut, 2, 10);
						RomanStatueData.EnergyArcs[i] = &ElectricityArcs.back();

						TriggerRomanStatueShockwaveAttackSparks(pos1.x, pos1.y, pos1.z, 84, 164, 10, 128);
						SpawnElectricityGlow(RomanStatueData.EnergyArcs[i]->pos4, 36, 0, color, color / 2);

						RomanStatueData.EnergyArcs[i] = nullptr;
						unknown = 1;
						continue;
					}

					SpawnElectricity(
						pos1.ToVector3(), pos.ToVector3(),
						Random::GenerateInt(64, 80),
						0, color / 2, color,
						50, (int)ElectricityFlags::ThinIn | (int)ElectricityFlags::Spline | (int)ElectricityFlags::ThinOut, 2, 10);
					RomanStatueData.EnergyArcs[i] = &ElectricityArcs.back();

					SpawnElectricityGlow(RomanStatueData.EnergyArcs[i]->pos4, 36, 0, color / 2, color);

					RomanStatueData.EnergyArcs[i] = nullptr;
					unknown = true;
				}
				
				break;

			case STATUE_STATE_ATTACK_1:
			case STATUE_STATE_ATTACK_2:
			case STATUE_STATE_ATTACK_3:
			case STATUE_STATE_ATTACK_4:
				creature->MaxTurn = 0;

				if (abs(ai.angle) >= ANGLE(2.0f))
				{
					if (ai.angle >= 0)
						item->Pose.Orientation.y += ANGLE(2.0f);
					else
						item->Pose.Orientation.y -= ANGLE(2.0f);
				}
				else
				{
					item->Pose.Orientation.y += ai.angle;
				}

				if (item->Animation.FrameNumber > 10)
				{
					pos = GetJointPosition(item, 16);

					auto* room = &g_Level.Rooms[item->RoomNumber];
					FloorInfo* floor = GetSector(room, pos.x - room->Position.x, pos.z - room->Position.z);

					// If floor is stopped, then try to find static meshes and shatter them, activating heavy triggers below
					if (floor->Stopper)
					{
						for (int i = 0; i < room->mesh.size(); i++)
						{
							auto* mesh = &room->mesh[i];

							if (!((mesh->pos.Position.z ^ pos.z) & 0xFFFFFC00) && !((mesh->pos.Position.x ^ pos.x) & 0xFFFFFC00))
							{
								if (Statics[mesh->staticNumber].shatterType != ShatterType::None)
								{
									ShatterObject(0, mesh, -64, LaraItem->RoomNumber, 0);
									SoundEffect(GetShatterSound(mesh->staticNumber), (Pose*)mesh);

									floor->Stopper = false;

									TestTriggers(pos.x, pos.y, pos.z, item->RoomNumber, true);
								}
							}
						}
					}

					if (!creature->Flags)
					{
						if (item->TouchBits & 0xC000)
						{
							DoDamage(creature->Enemy, 200);
							CreatureEffect2(item, RomanStatueBite, 20, item->Pose.Orientation.y, DoBloodSplat);
							SoundEffect(SFX_TR4_LARA_THUD, &item->Pose);
							creature->Flags = 1;
						}
					}

					if (!item->TriggerFlags)
					{
						pos1 = GetJointPosition(item, 14, Vector3i(-40, 64, 360));
						pos1.y = item->Pose.Position.y - 64;

						if (item->Animation.FrameNumber == 34 && item->Animation.ActiveState == 3)
						{
							if (item->ItemFlags[0])
								item->ItemFlags[0]--;

							TriggerShockwave(&Pose(pos1), 16, 160, 96, 0, color / 2, color, 48, EulerAngles::Identity, 1, true, false, true, (int)ShockwaveStyle::Normal);
							TriggerRomanStatueShockwaveAttackSparks(pos1.x, pos1.y, pos1.z, 128, 64, 0, 128);

							pos1.y -= 64;

							TriggerShockwave(&Pose(pos1), 16, 160, 64, 0, color / 2, color, 48, EulerAngles::Identity, 1, true, false, true, (int)ShockwaveStyle::Normal);
							
							auto lightColor = Color(0.4f, 0.3f, 0.0f);
							SpawnDynamicPointLight(pos.ToVector3(), lightColor, BLOCK(2.5f));
						}

						deltaFrame = item->Animation.FrameNumber;
						int deltaFrame2 = item->Animation.FrameNumber;

						if (deltaFrame2 >= 16)
						{
							if (deltaFrame > 16)
								deltaFrame = 16;

							TriggerRomanStatueAttackEffect1(itemNumber, deltaFrame);

							if (item->ItemFlags[3])
							{
								SpawnDynamicLight(pos1.x, pos1.y, pos1.z, 8, 0, color / 4, color / 2);
							}
							else
							{
								SpawnDynamicLight(pos1.x, pos1.y - 64, pos1.z, 18, 0, color / 4, color / 2);
							}
						}
						else
						{
							TriggerRomanStatueAttackEffect1(itemNumber, deltaFrame2);

							if (item->ItemFlags[3])
							{
								auto lightColor = Color(0.0f, 0.4f, 1.0f);
								SpawnDynamicPointLight(pos.ToVector3(), lightColor, BLOCK(4));
							}
						}
					}
				}

				break;

			case STATUE_STATE_WALK:
				creature->Flags = 0;
				joint2 = ai.angle;

				if (creature->Mood == MoodType::Attack)
				{
					creature->MaxTurn = ANGLE(7.0f);
				}
				else
				{
					creature->MaxTurn = 0;
					if (abs(ai.angle) >= ANGLE(2.0f))
					{
						if (ai.angle > 0)
							item->Pose.Orientation.y += ANGLE(2.0f);
						else
							item->Pose.Orientation.y -= ANGLE(2.0f);
					}
					else
					{
						item->Pose.Orientation.y += ai.angle;
					}
				}

				if (ai.distance < pow(BLOCK(1), 2))
				{
					item->Animation.TargetState = STATUE_STATE_IDLE;
					break;
				}

				if (ai.bite && ai.distance < pow(BLOCK(1.75f), 2))
				{
					item->Animation.TargetState = 9;
					break;
				}

				if (item->TriggerFlags == 1)
				{
					if (Targetable(item, &ai) && !(GetRandomControl() & 3))
					{
						item->Animation.TargetState = STATUE_STATE_IDLE;
						break;
					}
				}

				if (item->TriggerFlags || ai.distance >= pow(BLOCK(2.5f), 2))
					item->Animation.TargetState = STATUE_STATE_WALK;
				else
					item->Animation.TargetState = STATUE_STATE_IDLE;

				break;

			case STATUE_STATE_TURN_180:
				creature->MaxTurn = 0;
				creature->Flags = 0;

				if (ai.angle > 0)
					item->Pose.Orientation.y -= ANGLE(2.0f);
				else
					item->Pose.Orientation.y += ANGLE(2.0f);

				if (TestLastFrame(*item))
					item->Pose.Orientation.y += -ANGLE(180.0f);

				break;

			case STATUE_STATE_ENERGY_ATTACK:
				creature->MaxTurn = 0;
				creature->Flags = 0;

				if (RomanStatueData.Count)
				{
					RomanStatueData.Count--;
					color = (RomanStatueData.Count * ((GetRandomControl() & 0x3F) + 128)) / 16;
					SpawnDynamicLight(RomanStatueData.Position.x, RomanStatueData.Position.y, RomanStatueData.Position.z, 16, 0, color, color / 2);
				}

				deltaFrame = item->Animation.FrameNumber;

				if (deltaFrame == 34)
				{
					pos1 = GetJointPosition(item, 14, Vector3i(-48, 48, BLOCK(1)));
					pos2 = GetJointPosition(item, 14, Vector3i(-48, 48, 450));

				auto orient = Geometry::GetOrientToPoint(pos2.ToVector3(), pos1.ToVector3());
				auto attackPose = Pose(pos2, orient);

					short roomNumber = item->RoomNumber;
					GetFloor(pos2.x, pos2.y, pos2.z, &roomNumber);

					RomanStatueAttack(&attackPose, roomNumber, 1);

					TriggerRomanStatueShockwaveAttackSparks(
						attackPose.Position.x,
						attackPose.Position.y,
						attackPose.Position.z,
						0,
						(((GetRandomControl() & 0x3F) + 128) / 2),
						(((GetRandomControl() & 0x3F) + 128)),
						64);

					RomanStatueData.Count = 16;
					RomanStatueData.Position = attackPose.Position;

					if (item->ItemFlags[0])
						item->ItemFlags[0]--;
				}
				else if (deltaFrame < 10 || deltaFrame > 49)
				{
					break;
				}

				deltaFrame -= 10;
				if (deltaFrame < 32)
				{
					pos1 = GetJointPosition(item, 14, Vector3i(-32, 48, 64));
					pos2 = GetJointPosition(item, 14, Vector3i(-48, 48, 490));

					for (int i = 0; i < 4; i++)
					{
						byte r = (deltaFrame * ((GetRandomControl() & 0x3F) + 128)) / 32;
						byte g = (deltaFrame * ((GetRandomControl() & 0x3F) + 128)) / 16;
						byte b = (deltaFrame * ((GetRandomControl() & 0x3F) + 128)) / 32;

						if (i == 0)
						{
							SpawnDynamicLight(
								pos2.x, pos2.y, pos2.z,
								8,
								0,
								(deltaFrame * ((GetRandomControl() & 0x3F) + 128)) / 32,
								(deltaFrame * ((GetRandomControl() & 0x3F) + 128)) / 64);
						}

						auto* arc = RomanStatueData.EnergyArcs[i];

						if (arc && deltaFrame && deltaFrame != 24)
						{
							if (deltaFrame < 16)
								arc->life = 56;

							arc->pos1 = pos1.ToVector3();
							arc->pos4 = pos2.ToVector3();
						}
						else if (deltaFrame <  16)
						{
							SpawnElectricity(
								pos1.ToVector3(), pos2.ToVector3(),
								Random::GenerateInt(8, 16),
								84, 164, 10,
								50, (int)ElectricityFlags::ThinIn | (int)ElectricityFlags::Spline | (int)ElectricityFlags::ThinOut, 6, 2);
							RomanStatueData.EnergyArcs[i] = &ElectricityArcs.back();

						}						
						else if (deltaFrame == 24)
						{
							color = (GetRandomControl() & 0x3F) + 128;

							SpawnElectricity(
								pos1.ToVector3(), pos2.ToVector3(),
								Random::GenerateInt(18, 26),
								0, color, color / 2,
								50, (int)ElectricityFlags::ThinIn | (int)ElectricityFlags::Spline | (int)ElectricityFlags::ThinOut, 8, 2);
						}
					}
				}

				break;

			default:
				break;
			}
		}
		else
		{
			item->HitPoints = 0;

			if (item->Animation.ActiveState == STATUE_STATE_DEATH)
			{
				if (TestAnimFrameRange(*item, 55, 73) && item->TouchBits.TestAny())
				{
					DoDamage(creature->Enemy, 40);
				}
				else if (TestLastFrame(*item))
				{
					// Activate trigger on death
					short roomNumber = item->ItemFlags[2] & 0xFF;
					short floorHeight = item->ItemFlags[2] & 0xFF00;
					auto* room = &g_Level.Rooms[roomNumber];

					int x = room->Position.x + (creature->Tosspad / 256 & 0xFF) * BLOCK(1) + 512;
					int y = room->BottomHeight + floorHeight;
					int z = room->Position.z + (creature->Tosspad & 0xFF) * BLOCK(1) + 512;

					TestTriggers(x, y, z, roomNumber, true);
				}
			}
			else
			{
				SetAnimation(*item, STATUE_ANIM_DEATH);
			}
		}

		CreatureTilt(item, 0);
		CreatureJoint(item, 0, joint0);
		CreatureJoint(item, 1, joint1);
		CreatureJoint(item, 2, joint2);

		if (item->TestMeshSwapFlags(0x400))
		{
			auto pos = Vector3i(
				(GetRandomControl() & 0x1F) - 16,
				86,
				(GetRandomControl() & 0x1F) - 16);
			RomanStatueHitEffect(item, &pos, 10);
		}

		if (item->TestMeshSwapFlags(0x10))
		{
			auto pos = Vector3i(
				-40,
				(GetRandomControl() & 0x7F) + 148,
				(GetRandomControl() & 0x3F) - 32);
			RomanStatueHitEffect(item, &pos, 4);
		}

		if (item->TestMeshSwapFlags(0x100))
		{
			auto pos = Vector3i(
				(GetRandomControl() & 0x3F) + 54,
				-170,
				(GetRandomControl() & 0x1F) + 27);
			RomanStatueHitEffect(item, &pos, 8);
		}

		CreatureAnimation(itemNumber, headingAngle, 0);
	}

	void RomanStatueHit(ItemInfo& target, ItemInfo& source, std::optional<GameVector> pos, int damage, bool isExplosive, int jointIndex)
	{
		const auto& player = *GetLaraInfo(&source);
		const auto& object = Objects[target.ObjectNumber];

		if (object.hitEffect == HitEffect::Richochet && pos.has_value())
		{
			TriggerRicochetSpark(*pos, source.Pose.Orientation.y, false);
			SoundEffect(SFX_TR5_SWORD_GOD_HIT_METAL, &target.Pose);
		}

		if (pos.has_value() && (player.Control.Weapon.GunType == LaraWeaponType::Pistol ||
			player.Control.Weapon.GunType == LaraWeaponType::Shotgun ||
			player.Control.Weapon.GunType == LaraWeaponType::Uzi ||
			player.Control.Weapon.GunType == LaraWeaponType::HK ||
			player.Control.Weapon.GunType == LaraWeaponType::Revolver))
		{
			DoItemHit(&target, damage, isExplosive, false);
		}
		else if (player.Weapons[(int)LaraWeaponType::GrenadeLauncher].SelectedAmmo == WeaponAmmoType::Ammo2)
		{
			DoItemHit(&target, damage / ROMAN_STATUE_GRENADE_SUPER_AMMO_LIMITER, isExplosive, false);
		}
		else
		{
			DoItemHit(&target, damage * ROMAN_STATUE_EXPLOSIVE_DAMAGE_COEFF, isExplosive, false);
		}
	}
}
