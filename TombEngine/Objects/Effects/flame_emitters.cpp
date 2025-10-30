#include "framework.h"
#include "Objects/Effects/flame_emitters.h"

#include "Game/animation.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/effects/effects.h"
#include "Game/effects/Electricity.h"
#include "Game/effects/item_fx.h"
#include "Game/effects/tomb4fx.h"
#include "Game/effects/weather.h"
#include "Game/Hud/Hud.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_tests.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "Specific/clock.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Collision::Point;
using namespace TEN::Effects::Electricity;
using namespace TEN::Effects::Environment;
using namespace TEN::Effects::Items;
using namespace TEN::Hud;
using namespace TEN::Input;

namespace TEN::Entities::Effects
{
	constexpr int FLAME_RADIUS = CLICK(0.5f);
	constexpr int FLAME_OFFSET = CLICK(1 / 4.0f);
	constexpr int FLAME_BIG_RADIUS = CLICK(2.33f);
	constexpr int FLAME_ITEM_BURN_TIMEOUT = 3 * FPS;

	byte Flame3xzoffs[16][2] =
	{
		{ 9, 9 },
		{ 24, 9 },
		{ 40, 9	},
		{ 55, 9 },
		{ 9, 24 },
		{ 24, 24 },
		{ 40, 24 },
		{ 55, 24 },
		{ 9, 40 },
		{ 24, 40 },
		{ 40, 40 },
		{ 55, 40 },
		{ 9, 55	 },
		{ 24, 55 },
		{ 40, 55 },
		{ 55, 55 }
	};

	ObjectCollisionBounds FireBounds =
	{
		GameBoundingBox::Zero,
		std::pair(
			EulerAngles(ANGLE(-10.0f), ANGLE(-30.0f), ANGLE(-10.0f)),
			EulerAngles(ANGLE(10.0f), ANGLE(30.0f), ANGLE(10.0f))
		)
	};

	static void BurnNearbyItems(ItemInfo* item, int radius)
	{
		auto collObjects = GetCollidedObjects(*item, true, false, radius, ObjectCollectionMode::Items);
		for (auto* itemPtr : collObjects.Items)
		{
			if (TestEnvironment(ENV_FLAG_WATER, itemPtr->RoomNumber))
				continue;

			if ((!itemPtr->IsCreature() && !itemPtr->IsLara()) || itemPtr->HitPoints <= 0)
				continue;

			if (itemPtr->IsLara() && GetLaraInfo(item)->Control.WaterStatus == WaterStatus::FlyCheat)
				continue;

			if (item->Model.Color == Vector4::One)
			{
				ItemBurn(itemPtr, itemPtr->IsLara() ? NO_VALUE : FLAME_ITEM_BURN_TIMEOUT);
			}
			else
			{
				auto secondaryColor = Vector3(std::max(item->Model.Color.x - 0.2f, 0.0f),
											  std::max(item->Model.Color.y - 0.2f, 0.0f),
											  std::max(item->Model.Color.z - 0.2f, 0.0f));

				ItemCustomBurn(itemPtr, (Vector3)item->Model.Color, secondaryColor, NO_VALUE);
			}
		}
	}

	static bool CalculateActiveState(ItemInfo* item)
	{
		bool active = TriggerActive(item);

		// Modify fade value in case flame emitter is being activated or deactivated.
		// Additionally return activity status depending on whether fade is finished or not.

		if (active)
		{
			if (item->ItemFlags[3] > 0)
			{
				item->ItemFlags[3] -= 16;
				item->ItemFlags[3] = std::max(0, (int)item->ItemFlags[3]);
			}
		}
		else
		{
			if (item->ItemFlags[3] < UCHAR_MAX)
			{
				item->ItemFlags[3] += 32;
				item->ItemFlags[3] = std::min(UCHAR_MAX, (int)item->ItemFlags[3]);
				active = true;
			}
			else
				active = false;
		}

		return active;
	}

	static Vector4 GetFlameColor(Vector4 sourceColor)
	{
		if (sourceColor == Vector4::One)
			return Vector4(1.0f, Random::GenerateFloat(0.3f, 0.4f), 0.1f, 1.0f) * UCHAR_MAX;

		return sourceColor / 2.0f * Random::GenerateFloat(0.85f, 1.0f) * UCHAR_MAX;
	}

	void FlameEmitterControl(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];
		bool active = (item->TriggerFlags < 0) ? TriggerActive(item) : CalculateActiveState(item);

		if (active)
		{
			// Jet flame.
			if (item->TriggerFlags < 0)
			{
				short ocb = -item->TriggerFlags;
				if ((ocb & 7) == 2 || (ocb & 7) == 7)
				{
					// Constant flames.
					SoundEffect(SFX_TR4_FLAME_EMITTER, &item->Pose);
					TriggerSuperJetFlame(item, -256 - (3072 * GlobalCounter & 0x1C00), GlobalCounter & 1);

					auto color = GetFlameColor(item->Model.Color);
					SpawnDynamicLight(item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z,
						(GetRandomControl() & 3) + 20,
						color.x, color.y, color.z);
				}
				else
				{
					// Intermittent flames.
					auto& pauseTimer = item->ItemFlags[0];
					auto& jetFlameVel = item->ItemFlags[1];
					auto& smallFlameAlpha = item->ItemFlags[2];
					auto& flameTimer = item->ItemFlags[3];

					if (pauseTimer)
					{
						if (jetFlameVel)
							jetFlameVel = jetFlameVel - (jetFlameVel >> 2);

						if (smallFlameAlpha > 0)
						{
							smallFlameAlpha -= 8;
						}
						else
						{
							smallFlameAlpha = 0;
						}

						pauseTimer--;
						if (pauseTimer == 1)
							flameTimer = (GetRandomControl() & 0x3F) + 150;
					}
					else
					{
						if (!--flameTimer)
						{
							pauseTimer = ((ocb % 8) != 0) ?
								Random::GenerateInt(2 * FPS, 4 * FPS) :						   // Pause of 2 to 4 seconds.
								Random::GenerateInt((ocb / 8) * FPS, FPS + ((ocb / 8 * FPS))); // Pause of 1 second * (ocb / 8) to 1 second plus 1 second * (ocb / 8).
						}

						if (smallFlameAlpha < UCHAR_MAX)
						{
							smallFlameAlpha += 8;
						}
						else
						{
							smallFlameAlpha = UCHAR_MAX;
						}

						if (jetFlameVel > -BLOCK(8))
							jetFlameVel -= BLOCK(0.5f);
					}

					if (smallFlameAlpha < UCHAR_MAX)
						AddFire(item->Pose.Position, item->RoomNumber, item->Model.Color, 0.5f, smallFlameAlpha);

					if (jetFlameVel)
					{
						SoundEffect(SFX_TR4_FLAME_EMITTER, &item->Pose);

						if (jetFlameVel <= -BLOCK(8))
						{
							TriggerSuperJetFlame(item, -BLOCK(0.25f) - (BLOCK(3) * GlobalCounter & 0x1C00), GlobalCounter & 1);
						}
						else
						{
							TriggerSuperJetFlame(item, jetFlameVel, GlobalCounter & 1);
						}

						auto color = GetFlameColor(item->Model.Color);
						SpawnDynamicLight(item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z,
							(-jetFlameVel >> 10) - (GetRandomControl() & 1) + 16,
							color.x, color.y, color.z);
					}
					else
					{
						auto color = GetFlameColor(item->Model.Color);
						SpawnDynamicLight(item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z,
							10 - (GetRandomControl() & 1),
							color.x, color.y, color.z);
					}
				}

				SoundEffect(SFX_TR4_LOOP_FOR_SMALL_FIRES, &item->Pose);
			}
			else
			{
				// Normal flames.
				AddFire(item->Pose.Position, item->RoomNumber, item->Model.Color, 2.0f, item->ItemFlags[3]);

				float multiplier = (float)(UCHAR_MAX - item->ItemFlags[3]) / float(UCHAR_MAX);
				auto color = GetFlameColor(item->Model.Color);

				SpawnDynamicLight(item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z,
					(16 - (GetRandomControl() & 1)) * multiplier,
					color.x, color.y, color.z);

				SoundEffect(SFX_TR4_LOOP_FOR_SMALL_FIRES, &item->Pose);

				if ((Wibble & 0x04) && Random::TestProbability(1 / 2.0f))
					BurnNearbyItems(item, FLAME_RADIUS);
			}
		}
	}

	void FlameEmitter2Control(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];
		bool active = (item->TriggerFlags < 0) ? TriggerActive(item) : CalculateActiveState(item);

		if (active)
		{
			// If not an emitter for flipmaps
			if (item->TriggerFlags >= 0)
			{
				// If not a moving flame
				if (item->TriggerFlags != 2)
				{
					float size = 1.0f;
					switch (item->TriggerFlags)
					{
					default:
					case 0:
						size = 2.0f;
						break;

					case 1:
						size = 1.0f;
						break;

					case 3:
						size = 0.5f;
						break;

					case 4:
					case 123:
						size = 0.25f;
						break;
					}

					AddFire(item->Pose.Position, item->RoomNumber, item->Model.Color, size, item->ItemFlags[3]);
				}

				if (item->TriggerFlags == 0 || item->TriggerFlags == 2)
				{
					float multiplier = (float)(UCHAR_MAX - item->ItemFlags[3]) / float(UCHAR_MAX);
					auto color = GetFlameColor(item->Model.Color);

					SpawnDynamicLight(item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z,
						10 * multiplier,
						color.x, color.y, color.z);
				}

				if (item->TriggerFlags == 2)
				{
					item->Pose.Position.x += phd_sin(item->Pose.Orientation.y - ANGLE(180)) * (CLICK(1) / FPS);
					item->Pose.Position.z += phd_cos(item->Pose.Orientation.y - ANGLE(180)) * (CLICK(1) / FPS);

					auto pointColl = GetPointCollision(*item);

					if (TestEnvironment(ENV_FLAG_WATER, pointColl.GetRoomNumber()) ||
						pointColl.GetFloorHeight() - item->Pose.Position.y > CLICK(2) ||
						pointColl.GetFloorHeight() == NO_HEIGHT)
					{
						Weather.Flash(255, 128, 0, 0.03f);
						KillItem(itemNumber);
						return;
					}

					if (item->RoomNumber != pointColl.GetRoomNumber())
						ItemNewRoom(itemNumber, pointColl.GetRoomNumber());

					item->Pose.Position.y = pointColl.GetFloorHeight();

					if (Wibble & 7)
						TriggerFireFlame(item->Pose.Position.x, item->Pose.Position.y - 32, item->Pose.Position.z, FlameType::Medium);
				}

				SoundEffect(SFX_TR4_LOOP_FOR_SMALL_FIRES, &item->Pose);
			}
			else if (item->ItemFlags[0] == 0)
			{
				DoFlipMap(-item->TriggerFlags);
				FlipMap[-item->TriggerFlags] ^= CODE_BITS;
				item->ItemFlags[0] = 1;
			}
		}
	}

	void InitializeFlameEmitter(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		item->Pose.Position.y -= FLAME_OFFSET;

		if (item->TriggerFlags < 0)
		{
			item->ItemFlags[0] = (GetRandomControl() & 0x3F) + 90;
			item->ItemFlags[2] = UCHAR_MAX;

			if (((-item->TriggerFlags) & 7) == 7)
			{
				switch (item->Pose.Orientation.y)
				{
				case 0:
					item->Pose.Position.z += 512;
					break;

				case 0x4000:
					item->Pose.Position.x += 512;
					break;

				case -0x8000:
					item->Pose.Position.z -= 512;
					break;

				case -0x4000:
					item->Pose.Position.x -= 512;
					break;
				}
			}
		}
		else
		{
			item->ItemFlags[3] = UCHAR_MAX;
		}
	}

	void InitializeFlameEmitter2(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		item->Pose.Position.y -= FLAME_OFFSET;

		if (item->TriggerFlags >= 0)
			item->ItemFlags[3] = UCHAR_MAX;

		if (item->TriggerFlags != 123)
		{
			switch (item->Pose.Orientation.y)
			{
			case 0:
				if (item->TriggerFlags == 2)
					item->Pose.Position.z += 80;
				else
					item->Pose.Position.z += 256;

				break;

			case 0x4000:
				if (item->TriggerFlags == 2)
					item->Pose.Position.x += 80;
				else
					item->Pose.Position.x += 256;

				break;

			case -0x8000:
				if (item->TriggerFlags == 2)
					item->Pose.Position.z -= 80;
				else
					item->Pose.Position.z -= 256;

				break;

			case -0x4000:
				if (item->TriggerFlags == 2)
					item->Pose.Position.x -= 80;
				else
					item->Pose.Position.x -= 256;

				break;
			}
		}
	}

	void InitializeFlameEmitter3(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		if (item->TriggerFlags >= 3)
		{
			for (int i = 0; i < g_Level.NumItems; i++)
			{
				auto* currentItem = &g_Level.Items[i];

				if (currentItem->ObjectNumber == ID_ANIMATING3)
				{
					if (currentItem->TriggerFlags == item->TriggerFlags)
						item->ItemFlags[2] = i;
					else if (currentItem->TriggerFlags == 0)
						item->ItemFlags[3] = i;
				}
			}
		}
	}

	void FlameEmitter3Control(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		if (TriggerActive(item))
		{
			if (item->TriggerFlags)
			{
				SoundEffect(SFX_TR4_ELECTRIC_ARCING_LOOP, &item->Pose);

				byte g = (GetRandomControl() & 0x3F) + 192;
				byte b = (GetRandomControl() & 0x3F) + 192;

				auto origin = item->Pose.Position.ToVector3();
				auto target = Vector3::Zero;

				if (!(GlobalCounter & 3))
				{
					if (item->TriggerFlags == 2 || item->TriggerFlags == 4)
					{
						target.x = item->Pose.Position.x + 2048 * phd_sin(item->Pose.Orientation.y + ANGLE(180.0f));
						target.y = item->Pose.Position.y;
						target.z = item->Pose.Position.z + 2048 * phd_cos(item->Pose.Orientation.y + ANGLE(180.0f));

						if (GetRandomControl() & 3)
							SpawnElectricity(origin, target, (GetRandomControl() & 0x1F) + 64, 0, g, b, 24, 0, 32, 3);
						else
							SpawnElectricity(origin, target, (GetRandomControl() & 0x1F) + 96, 0, g, b, 32, (int)ElectricityFlags::Spline, 32, 3);
					}
				}

				if (item->TriggerFlags >= 3 && !(GlobalCounter & 1))
				{
					short targetItemNumber = item->ItemFlags[((GlobalCounter >> 2) & 1) + 2];
					auto* targetItem = &g_Level.Items[targetItemNumber];

					target = GetJointPosition(targetItem, 0, Vector3i(0, -64, 20)).ToVector3();

					if (!(GlobalCounter & 3))
					{
						if (GetRandomControl() & 3)
							SpawnElectricity(origin, target, (GetRandomControl() & 0x1F) + 64, 0, g, b, 24, 0, 32, 5);
						else
							SpawnElectricity(origin, target, (GetRandomControl() & 0x1F) + 96, 0, g, b, 32, (int)ElectricityFlags::Spline, 32, 5);
					}

					if (item->TriggerFlags != 3 || targetItem->TriggerFlags)
						SpawnElectricityGlow(target, 64, 0, g, b);
				}

				if ((GlobalCounter & 3) == 2)
				{
					origin = item->Pose.Position.ToVector3();
					target = Vector3(
						(GetRandomControl() & 0x1FF) + origin.x - 256,
						(GetRandomControl() & 0x1FF) + origin.y - 256,
						(GetRandomControl() & 0x1FF) + origin.z - 256);

					SpawnElectricity(origin, target, (GetRandomControl() & 0xF) + 16, 0, g, b, 24, (int)ElectricityFlags::Spline | (int)ElectricityFlags::MoveEnd, 32, 3);
					SpawnElectricityGlow(target, 64, 0, g, b);
				}
			}
			else
			{
				// Small fires
				if (item->ItemFlags[0] != 0)
				{
					item->ItemFlags[0]--;
				}
				else
				{
					item->ItemFlags[0] = (GetRandomControl() & 3) + 8;
					short random = GetRandomControl() & 0x3F;
					if (item->ItemFlags[1] == random)
						random = (random + 13) & 0x3F;
					item->ItemFlags[1] = random;
				}

				int x, z, i;

				if (!(Wibble & 4))
				{
					i = item->ItemFlags[1] & 7;
					x = 16 * (Flame3xzoffs[i][0] - 32);
					z = 16 * (Flame3xzoffs[i][1] - 32);
				}
				else
				{
					i = item->ItemFlags[1] >> 3;
					x = 16 * (Flame3xzoffs[i + 8][0] - 32);
					z = 16 * (Flame3xzoffs[i + 8][1] - 32);
				}

				SoundEffect(SFX_TR4_LOOP_FOR_SMALL_FIRES, &item->Pose);
				TriggerFireFlame(x + item->Pose.Position.x, item->Pose.Position.y, z + item->Pose.Position.z, FlameType::Small);
				SpawnDynamicLight(x, item->Pose.Position.y, z, 12, (GetRandomControl() & 0x3F) + 192, ((GetRandomControl() >> 4) & 0x1F) + 96, 0);

				auto pos = item->Pose.Position;
				if (ItemNearLara(pos, FLAME_BIG_RADIUS))
				{
					// Burn Lara only in case she is very close to the fire.
					if (LaraItem->Effect.Type != EffectType::Fire && 
						Lara.Control.WaterStatus != WaterStatus::FlyCheat)
					{
						DoDamage(LaraItem, 5);

						int dx = LaraItem->Pose.Position.x - item->Pose.Position.x;
						int dz = LaraItem->Pose.Position.z - item->Pose.Position.z;

						if (SQUARE(dx) + SQUARE(dz) < SQUARE(FLAME_BIG_RADIUS - FLAME_RADIUS))
							ItemBurn(LaraItem);
					}
				}
				else
				{
					// Burn other items as usual.
					if ((Wibble & 0x04) && Random::TestProbability(1 / 8.0f))
						BurnNearbyItems(item, FLAME_RADIUS);
				}
			}
		}
	}

	void FlameEmitterCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* item = &g_Level.Items[itemNumber];

		g_Hud.InteractionHighlighter.Test(*laraItem, *item, InteractionMode::Custom);

		if (Lara.Control.Weapon.GunType != LaraWeaponType::Torch ||
			Lara.Control.HandStatus != HandStatus::WeaponReady ||
			Lara.LeftArm.Locked ||
			item->Timer == -1 ||
			!TestLaraTorchFlame(laraItem, item) ||
			!IsHeld(In::Action) ||
			laraItem->Animation.ActiveState != LS_IDLE ||
			laraItem->Animation.AnimNumber != LA_STAND_IDLE ||
			laraItem->Animation.IsAirborne)
		{
			if (item->ObjectNumber == ID_BURNING_ROOTS)
				ObjectCollision(itemNumber, laraItem, coll);
		}
		else
		{
			switch (item->ObjectNumber)
			{
			case ID_FLAME_EMITTER:
				FireBounds.BoundingBox.X1 = -256;
				FireBounds.BoundingBox.X2 = 256;
				FireBounds.BoundingBox.Y1 = 0;
				FireBounds.BoundingBox.Y2 = 1024;
				FireBounds.BoundingBox.Z1 = -800;
				FireBounds.BoundingBox.Z2 = 800;
				break;

			case ID_FLAME_EMITTER2:
				FireBounds.BoundingBox.X1 = -256;
				FireBounds.BoundingBox.X2 = 256;
				FireBounds.BoundingBox.Y1 = 0;
				FireBounds.BoundingBox.Y2 = 1024;
				FireBounds.BoundingBox.Z1 = -600;
				FireBounds.BoundingBox.Z2 = 600;
				break;

			case ID_BURNING_ROOTS:
				FireBounds.BoundingBox.X1 = -384;
				FireBounds.BoundingBox.X2 = 384;
				FireBounds.BoundingBox.Y1 = 0;
				FireBounds.BoundingBox.Y2 = 2048;
				FireBounds.BoundingBox.Z1 = -384;
				FireBounds.BoundingBox.Z2 = 384;
				break;
			}

			short oldYrot = item->Pose.Orientation.y;
			item->Pose.Orientation.y = laraItem->Pose.Orientation.y;

			if (TestLaraPosition(FireBounds, item, laraItem))
			{
				if (item->ObjectNumber == ID_BURNING_ROOTS)
					laraItem->Animation.AnimNumber = LA_TORCH_LIGHT_5;
				else
				{
					if (item->Status == ITEM_ACTIVE)
					{
						Lara.Torch.NextColor = (Vector3)item->Model.Color;
						Lara.Torch.Fade = Lara.Torch.FADE_TIMEOUT * FPS;
					}

					Lara.Torch.State = TorchState::JustLit;
					int dy = abs(laraItem->Pose.Position.y - item->Pose.Position.y);
					laraItem->ItemFlags[3] = 1;
					laraItem->Animation.AnimNumber = (dy >> 8) + LA_TORCH_LIGHT_1;
				}

				laraItem->Animation.ActiveState = LS_MISC_CONTROL;
				laraItem->Animation.FrameNumber = GetAnimData(laraItem).frameBase;
				Lara.Flare.ControlLeft = false;
				Lara.LeftArm.Locked = true;
				Lara.Context.InteractedItem = itemNumber;
			}

			item->Pose.Orientation.y = oldYrot;
		}

		if (Lara.Context.InteractedItem == itemNumber &&
			item->Status != ITEM_ACTIVE &&
			laraItem->Animation.ActiveState == LS_MISC_CONTROL)
		{
			if (laraItem->Animation.AnimNumber >= LA_TORCH_LIGHT_1 && laraItem->Animation.AnimNumber <= LA_TORCH_LIGHT_5)
			{
				if (laraItem->Animation.FrameNumber - GetAnimData(laraItem).frameBase == 40)
				{
					TestTriggers(item, true, item->Flags & IFLAG_ACTIVATION_MASK);

					const auto& lara = *GetLaraInfo(laraItem);

					item->Flags |= CODE_BITS;
					item->ItemFlags[3] = (item->ObjectNumber != ID_FLAME_EMITTER3) ? UCHAR_MAX : 0;
					item->Status = ITEM_ACTIVE;
					item->Model.Color = Vector4(lara.Torch.CurrentColor.x, lara.Torch.CurrentColor.y, lara.Torch.CurrentColor.z, 1.0f);

					AddActiveItem(itemNumber);
				}
			}
		}
	}
}
