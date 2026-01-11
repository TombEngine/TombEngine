#include "framework.h"
#include "Objects/TR5/Entity/tr5_laser_head.h"

#include "Game/Animation/Animation.h"
#include "Game/control/los.h"
#include "Game/effects/debris.h"
#include "Game/effects/effects.h"
#include "Game/effects/Electricity.h"
#include "Game/effects/item_fx.h"
#include "Game/effects/tomb4fx.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_collide.h"
#include "Game/Lara/lara_fire.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Objects/TR5/Entity/tr5_laserhead_info.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Effects::Electricity;
using namespace TEN::Effects::Items;
using namespace TEN::Math;

// NOTES:
// item.Animation.Velocity.z = death explosion timer.
// item.ItemFlags[0];		 = a state machine. 3 = trigger death, other values still unknown.
// item.ItemFlags[1]		 = vertical hover offset? Tentacle index?
// item.ItemFlags[2]		 = vertical hover translation angle?
// item.ItemFlags[3]		 = target Y orientation.
// item.TriggerFlags		 = target X orientation.

namespace TEN::Entities::Creatures::TR5
{
	constexpr auto GUARDIAN_EXPLOSION_TIME = 136.0f;
	const	  auto GUARDIAN_ORIENT_OFFSET  = EulerAngles(ANGLE(18.25f), 0, 0); // TODO: Asset issue?

	constexpr auto GuardianEyeJoints	   = std::array<int, 2>{ 1, 2 };
	const	  auto GuardianBasePosition	   = Vector3i(0, -640, 0);
	const	  auto GuardianChargePositions = std::array<Vector3i, 4>
	{
		Vector3i(-188, -832, 440),
		Vector3i(188, -832, -440),
		Vector3i(440, -832, 188),
		Vector3i(-440, -832, -188)
	};

	GuardianInfo& GetGuardianInfo(ItemInfo& item)
	{
		return (GuardianInfo&)item.Data;
	}

	void InitializeGuardian(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		item.Data = GuardianInfo();
		auto& guardian = GetGuardianInfo(item);

		int vPos = item.Pose.Position.y + GuardianBasePosition.y;

		item.Collidable = true;
		item.StartPose = item.Pose;
		item.Pose.Position.y = vPos;
		item.Animation.ActiveState = 0;
		item.ItemFlags[1] = vPos + GuardianBasePosition.y;
		item.ItemFlags[3] = item.Pose.Orientation.y;
		item.TriggerFlags = item.Pose.Orientation.x;
		guardian.trackSpeed = Random::GenerateInt(3, 5);

		// TODO: After initializing, the guardian must first see the player
		// before it can start moving around at all.

		// Initialize base.
		for (int i = 0; i < g_Level.NumItems; i++)
		{
			if (g_Level.Items[i].ObjectNumber == ID_LASERHEAD_BASE)
			{
				guardian.BaseItem = i;
				break;
			}
		}

		// Initialize tentacles.
		short currentYOrient = 0;
		for (int j = 0; j < GUARDIAN_TENTACLE_COUNT; j++)
		{
			for (int i = 0; i < g_Level.NumItems; i++)
			{
				if (g_Level.Items[i].ObjectNumber == ID_LASERHEAD_TENTACLE &&
					g_Level.Items[i].Pose.Orientation.y == currentYOrient)
				{
					guardian.Tentacles[j] = i;
					break;
				}
			}

			currentYOrient += ANGLE(45.0f);
		}
	}

	void ControlGuardian(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];
		auto* guardian = &GetGuardianInfo(*item);

		auto origin = GameVector::Zero;
		auto target = GameVector::Zero;
		int farAway;

		if (!item->ItemFlags[0])
		{
			if (item->ItemFlags[2] < GUARDIAN_TENTACLE_COUNT)
			{
				if (!(GlobalCounter & 7))
				{
					short tentacleItemNumber = guardian->Tentacles[item->ItemFlags[2]];
					auto& tentacleItem = g_Level.Items[tentacleItemNumber];

					AddActiveItem(tentacleItemNumber);
					tentacleItem.Status = ITEM_ACTIVE;
					tentacleItem.Flags |= IFLAG_ACTIVATION_MASK;
					item->ItemFlags[2]++;
				}
			}
			else
			{
				if (item->Pose.Position.y > item->ItemFlags[1])
				{
					item->Animation.Velocity.y += 3.0f;

					if (item->Animation.Velocity.y > 32.0f)
						item->Animation.Velocity.y = 32.0f;

					item->Pose.Position.y -= item->Animation.Velocity.y;
				}
				else
				{
					origin = GameVector(GetJointPosition(item, 0, Vector3i(0, 168, 248)), item->RoomNumber);
					origin.RoomNumber = item->RoomNumber;
					target = GameVector(GetJointPosition(LaraItem, LM_HEAD), LaraItem->RoomNumber);
					target.RoomNumber = LaraItem->RoomNumber;

					if (LOS(&origin, &target))
					{
						item->ItemFlags[0]++;
						item->ItemFlags[1] = item->Pose.Position.y;
						item->ItemFlags[2] = ANGLE(14.5f);
					}
				}
			}
		}
		else if (item->ItemFlags[0] <= 2)
		{
			item->TriggerFlags++;
			item->Pose.Position.y = item->ItemFlags[1] - (128 * phd_sin(item->ItemFlags[2]));
			item->ItemFlags[2] += ANGLE(3.0f);

			// Get guardian head's position.
			origin = GameVector(GetJointPosition(item, 0, Vector3i(0, 168, 248)), item->RoomNumber);
			origin.RoomNumber = item->RoomNumber;
			if (item->ItemFlags[0] == 1)
			{
				// Get player head position.
				target = GameVector(GetJointPosition(LaraItem, LM_HEAD), LaraItem->RoomNumber);
				target.RoomNumber = LaraItem->RoomNumber;

				// Calculate distance between guardian and player.
				float distance = Vector3::Distance(origin.ToVector3(), target.ToVector3());

				// Check for clear LOS between the guardian and player,
				// if the distance is less than 8 blocks, the player is alive and not burning
				if (LOS(&origin, &target) &&
					distance <= MAX_VISIBILITY_DISTANCE &&
					LaraItem->HitPoints > 0 &&
					LaraItem->Effect.Type == EffectType::None &&
					(guardian->target.x || guardian->target.y || guardian->target.z))
				{
					// Lock attack target.
					target = GameVector(GetJointPosition(LaraItem, LM_HIPS), target.RoomNumber);
					target.RoomNumber = LaraItem->RoomNumber;
					guardian->target.x = target.x;
					guardian->target.y = target.y;
					guardian->target.z = target.z;
					guardian->trackSpeed = 3;
					guardian->trackLara = true;
				}
				else
				{
					// Randomly turn head.
					farAway = !(GetRandomControl() & 0x7F) && item->TriggerFlags > 150;
					item->ItemFlags[3]--;
					if (item->ItemFlags[3] <= 0 || farAway)
					{
						short xRot = (GetRandomControl() / 4) - ANGLE(22.5f);

						short yRot = 0;
						if (farAway)
							yRot = item->Pose.Orientation.y + (GetRandomControl() & 0x3FFF) + ANGLE(135.0f);
						else
							yRot = Random::GenerateAngle();

						int v = Random::GenerateInt(-MAX_VISIBILITY_DISTANCE, MAX_VISIBILITY_DISTANCE);
						int cosX = v * phd_cos(-xRot);

						target.x = origin.x + cosX * phd_sin(yRot);
						target.y = origin.y + v * phd_sin(-xRot);
						target.z = origin.z + cosX * phd_cos(yRot);

						if (farAway)
						{
							guardian->trackSpeed = 2;
							item->TriggerFlags = 0;
						}
						else
						{
							guardian->trackSpeed = Random::GenerateInt(3, 5);
						}

						item->ItemFlags[3] = guardian->trackSpeed * ((GetRandomControl() & 3) + 8);

						guardian->target.x = target.x;
						guardian->target.y = target.y;
						guardian->target.z = target.z;
					}

					guardian->trackLara = false;
				}
			}
			else
			{
				guardian->trackSpeed = 3;

				if (JustLoaded)
				{
					int cosX = phd_cos(item->Pose.Orientation.x + GUARDIAN_ORIENT_OFFSET.x) * ANGLE(45.0f);

					target.x = guardian->target.x = origin.x + cosX * phd_sin(item->Pose.Orientation.y);
					target.y = guardian->target.y = origin.y + BLOCK(8) * phd_sin(GUARDIAN_ORIENT_OFFSET.x - item->Pose.Orientation.x);
					target.z = guardian->target.z = origin.z + cosX * phd_cos(item->Pose.Orientation.y);
				}
			}

			auto targetOrient = Geometry::GetOrientToPoint(origin.ToVector3(), guardian->target.ToVector3()) + GUARDIAN_ORIENT_OFFSET;
			auto deltaOrient = item->Pose.Orientation - targetOrient;

			item->Pose.Orientation.Lerp(targetOrient, 1 / pow(2, guardian->trackSpeed));
			guardian->xRot = deltaOrient.x;
			guardian->yRot = deltaOrient.y;

			if (item->ItemFlags[0] == 1)
			{
				if (guardian->trackLara)
				{
					if (!(GetRandomControl() & 0x1F) &&
						abs(guardian->xRot) <= ANGLE(5.6f) &&
						abs(guardian->yRot) <= ANGLE(5.6f) &&
						!LaraItem->Animation.Velocity.y ||
						!(GetRandomControl() & 0x1FF))
					{
						item->ItemFlags[0]++;
						item->ItemFlags[3] = 0;
					}
				}
				else if (!(GetRandomControl() & 0x3F) && item->TriggerFlags > 300)
				{
					item->ItemFlags[0]++;
					item->TriggerFlags = 0;
					item->ItemFlags[3] = 0;
				}
			}
			else
			{
				if (item->ItemFlags[3] <= 90)
				{
					SoundEffect(SFX_TR5_GOD_HEAD_CHARGE, &item->Pose);
					DoGuardianLaserAttack(item);
					item->ItemFlags[3]++;
				}

				if (item->ItemFlags[3] >= 90)
				{
					auto color = Vector3(
						0.0f,
						(GetRandomControl() & 0x1F) + 128,
						(GetRandomControl() & 0x1F) + 64);

					auto colorSparks = Vector3(
						(GetRandomControl() & 0x1F) + 220,
						(GetRandomControl() & 0x1F) + 200,
						0.0f);

					auto* arc = guardian->fireArcs[0];
					if (guardian->fireArcs[0] == nullptr)
						arc = guardian->fireArcs[1];

					if ((item->ItemFlags[3] <= 90 || !arc || arc->life) &&
						LaraItem->HitPoints > 0 &&
						LaraItem->Effect.Type == EffectType::None)
					{
						if (item->ItemFlags[3] > 90 &&
							arc &&
							arc->life < 16)
						{
							color.y = (arc->life * color.y)  / 16;
							color.z = (arc->life * color.z)  / 16;
						}

						for (int i = 0; i < GUARDIAN_FIRE_ARC_COUNT; i++)
						{
							// If eye has not been destroyed, fire from it.
							if (!(item->MeshBits.Test(GuardianEyeJoints[i])))
							{
								if (item->ItemFlags[3] > 90 &&
									guardian->fireArcs[i])
								{
									guardian->fireArcs[i]->life = 0;
									guardian->fireArcs[i] = nullptr;
								}
							}
							else
							{
								auto origin1 = GameVector(GetJointPosition(item, GuardianEyeJoints[i]));
								origin1.RoomNumber = item->RoomNumber;
								auto eye = GameVector::Zero;
								eye.RoomNumber = item->RoomNumber;

								int cosX = MAX_VISIBILITY_DISTANCE * phd_cos(targetOrient.x - ANGLE(23.45f));
								
								eye.x = origin1.x + (cosX * phd_sin(item->Pose.Orientation.y));
								eye.y = origin1.y + (MAX_VISIBILITY_DISTANCE * phd_sin(-(targetOrient.x - GUARDIAN_ORIENT_OFFSET.x)));
								eye.z = origin1.z + (cosX * phd_cos(item->Pose.Orientation.y));
								
								if (item->ItemFlags[3] != 90 && guardian->fireArcs[i])
								{
									// Eye is already firing.
									SoundEffect(SFX_TR5_GOD_HEAD_LASER_LOOPS, &item->Pose);
									guardian->fireArcs[i]->pos1.x = origin1.x;
									guardian->fireArcs[i]->pos1.y = origin1.y;
									guardian->fireArcs[i]->pos1.z = origin1.z;

									// Reset fireArcs and let head follow player.
									if (guardian->fireArcs[i]->life > 0)
									{
										guardian->fireArcs[i]->life -= 2;
									}
									else
									{
										guardian->fireArcs[i]->life = 0;
										guardian->fireArcs[i] = nullptr;
										item->ItemFlags[0] = 1;
										item->TriggerFlags = 0;
									}
								}
								else
								{
									// Start firing from eye.
									origin1.RoomNumber = item->RoomNumber;														
									guardian->LOS[i] = LOS(&origin1, &eye);
									SpawnElectricity(origin1.ToVector3(), eye.ToVector3(), (GetRandomControl() & 1) + 3, color.x, color.y, color.z, 46, (int)ElectricityFlags::Spline | (int)ElectricityFlags::ThinIn | (int)ElectricityFlags::ThinOut, 6, 10);
									guardian->fireArcs[i] = &ElectricityArcs.back();
									StopSoundEffect(SFX_TR5_GOD_HEAD_CHARGE);
									SoundEffect(SFX_TR5_GOD_HEAD_BLAST, &item->Pose);																		
								}

								if (GlobalCounter & 1)
								{
									SpawnGuardianSparks(origin1.ToVector3(), colorSparks, 3);
									SpawnElectricityGlow(origin1.ToVector3(), (GetRandomControl() & 3) + 32, color.x, color.y, color.z);
									SpawnDynamicLight(origin1.x, origin1.y, origin1.z, (GetRandomControl() & 3) + 16, color.x, color.y, color.z);

									if (!guardian->LOS[i] && guardian->fireArcs[i] != nullptr)
									{
										SpawnElectricityGlow(guardian->fireArcs[i]->pos4, (GetRandomControl() & 3) + 16, color.x, color.y, color.z);
										SpawnDynamicLight(guardian->fireArcs[i]->pos4.x, guardian->fireArcs[i]->pos4.y, guardian->fireArcs[i]->pos4.z, (GetRandomControl() & 3) + 6, color.x, color.y, color.z);
										SpawnGuardianSparks(guardian->fireArcs[i]->pos4, colorSparks, 3);
									}
								}

								// Check if player was hit by energy arcs.
								if (LaraItem->Effect.Type == EffectType::None && guardian->fireArcs[i] != nullptr)
								{
									int adx = guardian->fireArcs[i]->pos4.x - origin1.x;
									int ady = guardian->fireArcs[i]->pos4.y - origin1.y;
									int adz = guardian->fireArcs[i]->pos4.z - origin1.z;

									farAway = 0;
									for (int j = 0; j < 32; j++)
									{
										if (farAway)
										{
											farAway--;
											if (!farAway)
												break;
										}

										if (adx < 280 && ady < 280 && adz < 280)
											farAway = 2;

										auto hitPos = Vector3i::Zero;

										auto start = GameVector(guardian->fireArcs[i]->pos1.x, guardian->fireArcs[i]->pos1.y, guardian->fireArcs[i]->pos1.z);
										start.RoomNumber = item->RoomNumber;
										auto end = GameVector(guardian->fireArcs[i]->pos4.x, guardian->fireArcs[i]->pos4.y, guardian->fireArcs[i]->pos4.z, 0);

										if (ObjectOnLOS2(&start, &end, &hitPos, nullptr, ID_LARA) == LaraItem->Index)
										{
											if (LaraItem->Effect.Type != EffectType::Smoke)
											{
												ItemCustomBurn(LaraItem, Vector3(0.0f, 0.8f, 0.1f), Vector3(0.0f, 0.9f, 0.8f), 1 * FPS);
											}
											else
											{
												ItemSmoke(LaraItem, -1);
											}

											DoDamage(LaraItem, INT_MAX);
											break;
										}
									}
								}
							}				
						}
					}
					else
					{
						if (arc)
						{
							guardian->fireArcs[0] = nullptr;
							guardian->fireArcs[1] = nullptr;
						}

						item->ItemFlags[0] = 1;
						item->TriggerFlags = 0;
					}
				}
			}
		}
		else
		{
			// Sequentially activate tentacles.
			if (!(GlobalCounter & 7) && item->Animation.ActiveState < GUARDIAN_TENTACLE_COUNT)
			{
				short tentacleNumber = guardian->Tentacles[item->Animation.ActiveState];
				g_Level.Items[tentacleNumber].Animation.TargetState = 2;
				item->Animation.ActiveState++;
			}

			// Destroy tentacles.
			if (item->Animation.ActiveState > 0)
			{
				for (int i = 0; i < GUARDIAN_TENTACLE_COUNT; i++)
				{
					auto& tentacleItem = g_Level.Items[guardian->Tentacles[i]];

					if (tentacleItem.Animation.AnimNumber == 1 &&
						TestLastFrame(*&tentacleItem) && tentacleItem.MeshBits.Test(1))
					{
						SoundEffect(SFX_TR5_SMASH_ROCK2, &item->Pose);
						ExplodeItemNode(&tentacleItem, 0, 0, 128);
						KillItem(guardian->Tentacles[i]);
					}
				}
			}

			item->Pose.Position.y = item->ItemFlags[1] - (192 - item->Animation.Velocity.z) * phd_sin(item->ItemFlags[2]);
			item->ItemFlags[2] += ANGLE(item->Animation.Velocity.z);

			// Set random target orientation.
			if (!(GlobalCounter & 7))
			{
				item->ItemFlags[3] = Random::GenerateAngle();
				item->TriggerFlags = Random::GenerateAngle(-ANGLE(12.0f), ANGLE(12.0f));
			}

			auto targetOrient = EulerAngles(item->TriggerFlags, item->ItemFlags[3], 0);
			item->Pose.Orientation.Lerp(targetOrient, 0.25f);

			// Death sequence.
			item->Animation.Velocity.z += 1.0f;
			if (item->Animation.Velocity.z >= GUARDIAN_EXPLOSION_TIME)
				DoGuardianDeath(itemNumber, *item);
		}

		if (item->ItemFlags[0] < 3)
		{
			int i = 0;
			for (i = 0; i < GUARDIAN_TENTACLE_COUNT; i++)
			{
				short tentacleItemNumber = guardian->Tentacles[i];
				auto& tentacleItem = g_Level.Items[tentacleItemNumber];

				if (tentacleItem.Animation.AnimNumber == 0 && !TestLastFrame(*&tentacleItem))
				{
					break;
				}
			}

			// If all tentacle animations are done and both eyes are destroyed, initiate death sequence.
			if (i == GUARDIAN_TENTACLE_COUNT && !(item->MeshBits & 6))
			{
				if (guardian->fireArcs[0])
					guardian->fireArcs[0]->life = 2;

				if (guardian->fireArcs[1])
					guardian->fireArcs[1]->life = 2;

				guardian->fireArcs[0] = nullptr;
				guardian->fireArcs[1] = nullptr;

				item->Animation.Velocity.z = 3.0f;
				item->ItemFlags[0] = 3;
				item->ItemFlags[3] = item->Pose.Orientation.y + Random::GenerateAngle(-ANGLE(11.25f), ANGLE(11.25f));
				item->TriggerFlags = item->Pose.Orientation.x + Random::GenerateAngle(-ANGLE(11.25f), ANGLE(11.25f));
			}
		}
	}

	void DoGuardianLaserAttack(ItemInfo* item)
	{
		auto* guardian = &GetGuardianInfo(*item);

		byte size = item->ItemFlags[3];

		auto color = Vector3(
			0.0f,
			((GetRandomControl() & 0x1F) + 128),
			((GetRandomControl() & 0x1F) + 64));

		auto colorSparks = Vector3(
			(GetRandomControl() & 0x1F) + 220,
			(GetRandomControl() & 0x1F) + 200,
			0.0f);

		if (size <= 32)
		{
			color.y = (size * color.y) / 32;
			color.z = (size * color.z) / 32;
		}
		else
		{
			size = 32;
		}

		auto origin = Vector3::Zero;
		auto target = GetJointPosition(&g_Level.Items[guardian->BaseItem], 0, GuardianBasePosition).ToVector3();

		for (int i = 0; i < GUARDIAN_CHARGE_ARC_COUNT; i++)
		{
			auto* arc = guardian->chargeArcs[i];

			if (item->ItemFlags[3] & 15 && arc != nullptr)
			{
				// NOTE: x and z inverted?
				arc->r = color.z;
				arc->g = color.y;
				arc->b = color.x;
				arc->life = 50;
				continue;
			}

			origin = GetJointPosition(&g_Level.Items[guardian->BaseItem], 0, GuardianChargePositions[i]).ToVector3();
			SpawnElectricity(origin, target, Random::GenerateInt(8, 16), color.x, color.y, color.z, 16, (int)ElectricityFlags::Spline | (int)ElectricityFlags::ThinOut | (int)ElectricityFlags::ThinIn, 6, 5);
		}

		if (GlobalCounter & 1)
		{
			for (int i = 0; i < GUARDIAN_FIRE_ARC_COUNT; i++)
			{
				if (item->MeshBits.Test(GuardianEyeJoints[i]))
				{
					origin = GetJointPosition(item, GuardianEyeJoints[i]).ToVector3();
					SpawnElectricityGlow(origin, size + (GetRandomControl() & 3), color.x, color.y, color.z);
					SpawnGuardianSparks(origin, colorSparks, 3);
				}
			}

			SpawnElectricityGlow(target, (GetRandomControl() & 3) + size + 8, color.x, color.y, color.z);
			SpawnDynamicLight(target.x, target.y, target.z, (GetRandomControl() & 3) + 16, color.x, color.y, color.z);
		}

		if (!(GlobalCounter & 3))
			SpawnElectricity(target, item->Pose.Position.ToVector3(), Random::GenerateInt(8, 16), color.x, color.y, color.z, 16, (int)ElectricityFlags::Spline | (int)ElectricityFlags::ThinOut | (int)ElectricityFlags::ThinIn, 6, 5);
			
		SpawnGuardianSparks(target, colorSparks, 3, 1);
	}

	void DoGuardianDeath(int itemNumber, ItemInfo& item)
	{
		const auto& guardian = GetGuardianInfo(item);
		
		if (g_Level.Items[guardian.BaseItem].ObjectNumber == ID_LASERHEAD_BASE)
		{
			ExplodeItemNode(&g_Level.Items[guardian.BaseItem], 0, 0, 128);
			KillItem(guardian.BaseItem);
		}

		ExplodeItemNode(&item, 0, 0, 128);

		item.Pose.Position.y -= CLICK(1);
		TriggerExplosionSparks(item.Pose.Position.x, item.Pose.Position.y, item.Pose.Position.z, 3, -2, 2, item.RoomNumber);
		TriggerExplosionSparks(item.Pose.Position.x, item.Pose.Position.y, item.Pose.Position.z, 2, 0, 2, item.RoomNumber);

		TriggerShockwave(&item.Pose, 32, 160, 64, 0, 128, 64, 36, EulerAngles::Identity, 0, true, false, false, (int)ShockwaveStyle::Normal);
		TriggerShockwave(&item.Pose, 32, 160, 64, 0, 128, 64, 36, EulerAngles(0x3000, 0.0f, 0.0f), 0, true, false, false, (int)ShockwaveStyle::Normal);
		TriggerShockwave(&item.Pose, 32, 160, 64, 0, 128, 64, 36, EulerAngles(0x6000, 0.0f, 0.0f), 0, true, false, false, (int)ShockwaveStyle::Normal);

		TestTriggers(&item, true);

		SoundEffect(SFX_TR5_GOD_HEAD_BLAST, &item.Pose, SoundEnvironment::Land, 0.5f);
		SoundEffect(SFX_TR4_EXPLOSION2, &item.Pose, SoundEnvironment::Land, 1.25f);
		SoundEffect(SFX_TR4_EXPLOSION1, &item.Pose);
		SoundEffect(SFX_TR4_EXPLOSION1, &item.Pose, SoundEnvironment::Land, 0.25f);

		KillItem(itemNumber);
	}

	void SpawnGuardianSparks(const Vector3& pos, const Vector3& color, unsigned int count, int unk)
	{
		for (int i = 0; i < count; i++)
		{
			auto* spark = GetFreeParticle();

			auto sphere = BoundingSphere(Vector3::Zero, BLOCK(2));
			auto vel = Random::GeneratePointInSphere(sphere) * pow(2, unk);

			spark->on = true;
			spark->sR = color.x;
			spark->sG = color.y;
			spark->sB = color.z;
			spark->dB = 0;
			spark->dG = 0;
			spark->dR = 0;
			spark->colFadeSpeed = 9 * pow(2, unk);
			spark->fadeToBlack = 0;
			spark->life = 9 * pow(2, unk);
			spark->sLife = 9 * pow(2, unk);
			spark->blendMode = BlendMode::Additive;
			spark->x = pos.x;
			spark->y = pos.y;
			spark->z = pos.z;
			spark->gravity = (GetRandomControl() / 128) & 0x1F;
			spark->yVel = vel.x;
			spark->xVel = vel.y;
			spark->zVel = vel.z;
			spark->flags = SP_NONE;
			spark->maxYvel = 0;
			spark->friction = 34 * pow(2, unk);
		}
	}
}

