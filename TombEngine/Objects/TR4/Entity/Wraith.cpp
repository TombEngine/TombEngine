#include "framework.h"
#include "Objects/TR4/Entity/Wraith.h"

#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/control/flipeffect.h"
#include "Game/effects/effects.h"
#include "Game/effects/Electricity.h"
#include "Game/effects/item_fx.h"
#include "Game/effects/Streamer.h"
#include "Game/effects/tomb4fx.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/people.h"
#include "Game/room.h"
#include "Math/Math.h"
#include "Objects/TR4/Entity/WraithInfo.h"
#include "Objects/objectslist.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Collision::Point;
using namespace TEN::Effects::Items;
using namespace TEN::Effects::Electricity;
using namespace TEN::Effects::Streamer;
using namespace TEN::Math;

namespace TEN::Entities::TR4
{
	constexpr auto WRAITH_COUNT				= 8;
	constexpr auto WRAITH_VELOCITY			= 64.0f;
	constexpr auto WRAITH_TRAP_DISTANCE_MAX = SQUARE(BLOCK(2));

	static WraithInfo& GetWraithInfo(ItemInfo& item)
	{
		return *(WraithInfo*)item.Data;
	}

	static void SpawnWraithTails(const ItemInfo& item)
	{
		constexpr auto OFFSET	 = Vector3(0.0f, -10.0f, -50.0f);
		constexpr auto COLOR_END = Color(0.0f, 0.0f, 0.0f, 0.0f);
		constexpr auto WIDTH	 = 8.0f;
		constexpr auto LIFE_MAX	 = 0.5f;
		constexpr auto VEL		 = 4.0f;
		constexpr auto EXP_RATE	 = 1.0f;

		enum class TailTag
		{
			First,
			Second,
			Third
		};

		auto colorStart = Vector4::Zero;
		switch (item.ObjectNumber)
		{
		default:
		case ID_WRAITH1:
			colorStart = Vector4(1.0f, 0.6f, 0.0f, 1.0f);
			break;

		case ID_WRAITH2:
			colorStart = Vector4(0.0f, 0.5f, 1.0f, 1.0f);
			break;

		case ID_WRAITH3:
			colorStart = Vector4(1.0f);
			break;
		}

		auto posBase = item.Pose.Position.ToVector3();
		auto rotMatrix = item.Pose.Orientation.ToRotationMatrix();
		auto pos = posBase + Vector3::Transform(OFFSET, rotMatrix);

		auto dir0 = Geometry::RotatePoint(posBase, EulerAngles(ANGLE(50.0f), 0, 0));
		auto dir1 = Geometry::RotatePoint(posBase, EulerAngles(ANGLE(-50.0f), 0, 0));
		auto dir2 = Geometry::RotatePoint(posBase, EulerAngles(0, ANGLE(50.0f), 0));

		short orient2D = item.Pose.Orientation.z;

		// Spawn first tail.
		StreamerEffect.Spawn(
			item.Index, (int)TailTag::First,
			pos, dir0, orient2D, colorStart, COLOR_END,
			WIDTH, LIFE_MAX, VEL, EXP_RATE, 0,
			StreamerFeatherMode::Center, BlendMode::Additive);

		// Spawn second tail.
		StreamerEffect.Spawn(
			item.Index, (int)TailTag::Second,
			pos, dir1, orient2D, colorStart, COLOR_END,
			WIDTH, LIFE_MAX, VEL, EXP_RATE, 0,
			StreamerFeatherMode::Center, BlendMode::Additive);

		// Spawn third tail.
		StreamerEffect.Spawn(
			item.Index, (int)TailTag::Third,
			pos, dir2, orient2D, colorStart, COLOR_END,
			WIDTH, LIFE_MAX, VEL, EXP_RATE, 0,
			StreamerFeatherMode::Center, BlendMode::Additive);
	}

	static void WraithWallEffect(Vector3i pos, short yRot, int objectNumber)
	{
		byte sR, sG, sB, dR, dG, dB;

		if (objectNumber == ID_WRAITH1)
		{
			sR = (GetRandomControl() & 0x1F) + -128;
			sB = 24;
			sG = (GetRandomControl() & 0x1F) + 48;
			dR = (GetRandomControl() & 0x1F) + -128;
			dB = 24;
			dG = (GetRandomControl() & 0x1F) + 64;
		}
		else if (objectNumber == ID_WRAITH2)
		{
			sB = (GetRandomControl() & 0x1F) + -128;
			sR = 24;
			sG = (GetRandomControl() & 0x1F) + -128;
			dB = (GetRandomControl() & 0x1F) + -128;
			dR = 24;
			dG = (GetRandomControl() & 0x1F) + 64;
		}
		else
		{
			short color = (GetRandomControl() & 0x1F) + 64;
			dG = color;
			dR = color;
			sB = color;
			sG = color;
			sR = color;
			dB = sB + (GetRandomControl() & 0x1F);
		}

		for (int i = 0; i < 15; i++)
		{
			auto& spark = *GetFreeParticle();

			spark.on = true;
			spark.sR = dR;
			spark.sG = dG;
			spark.sB = dB;
			spark.dR = dR;
			spark.dG = dG;
			spark.dB = dB;
			spark.colFadeSpeed = 4;
			spark.fadeToBlack = 7;
			spark.blendMode = BlendMode::Additive;
			short life = (GetRandomControl() & 7) + 32;
			spark.life = life;
			spark.sLife = life;
			spark.x = (GetRandomControl() & 0x1F) + pos.x - 16;
			spark.y = (GetRandomControl() & 0x1F) + pos.y - 16;
			spark.z = (GetRandomControl() & 0x1F) + pos.z - 16;
			short rot = yRot + GetRandomControl() - ANGLE(90.0f);
			short velocity = ((GetRandomControl() & 0x3FF) + 1024);
			spark.xVel = velocity * phd_sin(rot);
			spark.yVel = (GetRandomControl() & 0x7F) - 64;
			spark.zVel = velocity * phd_cos(rot);
			spark.friction = 4;
			spark.flags = SP_EXPDEF | SP_DEF | SP_SCALE;
			spark.maxYvel = 0;
			spark.scalar = 3;
			spark.gravity = (GetRandomControl() & 0x7F) - 64;
			short size = (GetRandomControl() & 0x1F) + 48;
			spark.sSize = size;
			spark.size = size;
			spark.dSize = size / 4;
		}
	}

	static void SpawnWraithExplosion(ItemInfo& item, const Vector3& byteColor, float vel)
	{
		short inner = vel >= 0 ? 32 : 640;
		short outer = vel >= 0 ? 160 : 512;

		TriggerShockwave(&item.Pose, inner, outer, vel, byteColor.x, byteColor.y, byteColor.z, 24, EulerAngles::Identity, 0, true, false, false, (int)ShockwaveStyle::Normal);
		TriggerShockwave(&item.Pose, inner, outer, vel, byteColor.x, byteColor.y, byteColor.z, 24, EulerAngles(ANGLE(45.0f), 0.0f, 0.0f), 0, true, false, false, (int)ShockwaveStyle::Normal);
		TriggerShockwave(&item.Pose, inner, outer, vel, byteColor.x, byteColor.y, byteColor.z, 24, EulerAngles(ANGLE(90.0f), 0.0f, 0.0f), 0, true, false, false, (int)ShockwaveStyle::Normal);
		TriggerShockwave(&item.Pose, inner, outer, vel, byteColor.x, byteColor.y, byteColor.z, 24, EulerAngles(ANGLE(135.0f), 0.0f, 0.0f), 0, true, false, false, (int)ShockwaveStyle::Normal);
	}

	void InitializeWraith(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		item.Data = WraithInfo();
		auto* wraithPtr = &GetWraithInfo(item);

		item.Animation.Velocity.z = WRAITH_VELOCITY;
		item.ItemFlags[0] = 0;
		item.ItemFlags[6] = 0;

		for (int i = 0; i < WRAITH_COUNT; i++)
		{
			wraithPtr->Position = Vector3i(0, 0, item.Pose.Position.z);
			wraithPtr->Velocity.z = 0;
			wraithPtr->r = 0;
			wraithPtr->g = 0;
			wraithPtr->b = 0;

			wraithPtr++;
		}
	}

	void WraithControl(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		SoundEffect(SFX_TR4_WRAITH_WHISPERS, &item.Pose);

		// HACK: HitPoints stores the wraith's target. NOTE: ``auto*`` makes some errors with wraith find its target in Release version. It works without ``*``.
		auto target = item.ItemFlags[6] ? &g_Level.Items[item.ItemFlags[6]] : LaraItem;

		auto prevPos = item.Pose.Position;
		int x, y, z, xl, yl, zl;
		int dy;
		int distance;
		int distancePlayer;

		if (target->IsLara() || target->ObjectNumber == ID_WRAITH_TRAP)
		{
			x = target->Pose.Position.x - item.Pose.Position.x;
			y = target->Pose.Position.y;
			z = target->Pose.Position.z - item.Pose.Position.z;
			distance = SQUARE(x) + SQUARE(z);
			dy = abs((distance / BLOCK(8)) - BLOCK(0.5f));
		}
		else
		{
			const auto& room = g_Level.Rooms[LaraItem->RoomNumber];

			x = room.Position.x + room.XSize * BLOCK(1) / 2 - item.Pose.Position.x;
			z = room.Position.z + room.ZSize * BLOCK(1) / 2 - item.Pose.Position.z;

			distance = SQUARE(x) + SQUARE(z);
			dy = abs((distance / MAX_VISIBILITY_DISTANCE) - CLICK(1));
			//Prevent Wraiths to go below floor level
			y = room.Position.y + ((room.TopHeight - room.BottomHeight) / 4);
		}

		dy = y - item.Pose.Position.y - dy - CLICK(0.5f);
		short angleH = phd_atan(z, x) - item.Pose.Orientation.y;

		short angleV = 0;
		if (abs(x) <= abs(z))
			angleV = phd_atan(abs(x) + (abs(z)), dy);
		else
			angleV = phd_atan(abs(z) + (abs(x)), dy);

		angleV -= item.Pose.Orientation.x;
		int velocity = (WRAITH_VELOCITY / item.Animation.Velocity.z) * 8;

		if (abs(angleH) < item.ItemFlags[2] && angleH > 0 == item.ItemFlags[2] > 0)
		{
			item.Pose.Orientation.y += angleH;
		}
		else if (angleH >= 0)
		{
			if (item.ItemFlags[2] <= 0)
			{
				item.ItemFlags[2] = 1;
			}
			else
			{
				item.ItemFlags[2] += velocity;
				item.Pose.Orientation.y += item.ItemFlags[2];
			}
		}
		else if (item.ItemFlags[2] >= 0)
		{
			item.ItemFlags[2] = -1;
		}
		else
		{
			item.ItemFlags[2] -= velocity;
			item.Pose.Orientation.y += item.ItemFlags[2];
		}

		if (abs(angleV) >= item.ItemFlags[3] || angleV > 0 != item.ItemFlags[3] > 0)
		{
			if (angleV >= 0)
			{
				if (item.ItemFlags[3] <= 0)
				{
					item.ItemFlags[3] = 1;
				}
				else
				{
					item.ItemFlags[3] += velocity;
					item.Pose.Orientation.x += item.ItemFlags[3];
				}
			}
			else if (item.ItemFlags[3] >= 0)
			{
				item.ItemFlags[3] = -1;
			}
			else
			{
				item.ItemFlags[3] -= velocity;
				item.Pose.Orientation.x += item.ItemFlags[3];
			}
		}
		else
		{
			item.Pose.Orientation.x += angleV;
		}

		auto pointColl = GetPointCollision(item);

		bool hasHitWall = false;
		if (pointColl.GetFloorHeight() < item.Pose.Position.y ||
			pointColl.GetCeilingHeight() > item.Pose.Position.y)
		{
			hasHitWall = true;
		}

		// Translate wraith.
		item.Pose.Position.x += item.Animation.Velocity.z * phd_sin(item.Pose.Orientation.y);
		item.Pose.Position.y += item.Animation.Velocity.z * phd_sin(item.Pose.Orientation.x);
		item.Pose.Position.z += item.Animation.Velocity.z * phd_cos(item.Pose.Orientation.y);

		if (pointColl.GetRoomNumber() != item.RoomNumber)
			ItemNewRoom(itemNumber, pointColl.GetRoomNumber());

		for (int linkItemNumber = g_Level.Rooms[item.RoomNumber].itemNumber; linkItemNumber != NO_VALUE; linkItemNumber = g_Level.Items[linkItemNumber].NextItem)
		{
			auto& targetItem = g_Level.Items[linkItemNumber];

			if (!targetItem.Active)
				continue;

			if ((item.ObjectNumber == ID_WRAITH1 && targetItem.ObjectNumber == ID_WRAITH2) ||
				(item.ObjectNumber == ID_WRAITH2 && targetItem.ObjectNumber == ID_WRAITH1) ||
				(item.ObjectNumber == ID_WRAITH3 && targetItem.ObjectNumber == ID_WRAITH_TRAP))
			{
				if (item.ObjectNumber == ID_WRAITH3 && targetItem.ObjectNumber == ID_WRAITH_TRAP)
				{
					x = targetItem.Pose.Position.x - item.Pose.Position.x;
					y = targetItem.Pose.Position.y;
					z = targetItem.Pose.Position.z - item.Pose.Position.z;
					distance = SQUARE(x) + SQUARE(z);

					xl = targetItem.Pose.Position.x - LaraItem->Pose.Position.x;
					yl = targetItem.Pose.Position.y;
					zl = targetItem.Pose.Position.z - LaraItem->Pose.Position.z;
					distancePlayer = SQUARE(xl) + SQUARE(zl);

					// Wraith 3 attacks the wraith trap only if it and the player are close enough.
					if (distance < WRAITH_TRAP_DISTANCE_MAX &&
						distancePlayer < WRAITH_TRAP_DISTANCE_MAX)
					{
						item.ItemFlags[6] = linkItemNumber;
						targetItem.ItemFlags[6] = 1;
					}
					else
					{
						item.ItemFlags[6] = 0;
						targetItem.ItemFlags[6] = 0;
						x = target->Pose.Position.x - item.Pose.Position.x;
						y = target->Pose.Position.y;
						z = target->Pose.Position.z - item.Pose.Position.z;
						distance = SQUARE(x) + SQUARE(z);
					}

					continue;
				}
				else if ((item.ObjectNumber == ID_WRAITH1 && targetItem.ObjectNumber == ID_WRAITH2) ||
					(item.ObjectNumber == ID_WRAITH2 && targetItem.ObjectNumber == ID_WRAITH1))
				{
					item.ItemFlags[6] = linkItemNumber;
					x = target->Pose.Position.x - item.Pose.Position.x;
					y = target->Pose.Position.y;
					z = target->Pose.Position.z - item.Pose.Position.z;
					distance = SQUARE(x) + SQUARE(z);
				}
				else
				{
					item.ItemFlags[6] = 0;
					x = target->Pose.Position.x - item.Pose.Position.x;
					y = target->Pose.Position.y;
					z = target->Pose.Position.z - item.Pose.Position.z;
					distance = SQUARE(x) + SQUARE(z);
				}
			}
		}
		
		if ((target->ObjectNumber == ID_WRAITH1 && !target->Active) ||
			(target->ObjectNumber == ID_WRAITH2 && !target->Active))
		{
			item.ItemFlags[6] = 0;
			x = target->Pose.Position.x - item.Pose.Position.x;
			y = target->Pose.Position.y;
			z = target->Pose.Position.z - item.Pose.Position.z;
			distance = SQUARE(x) + SQUARE(z);
		}

		if (item.ObjectNumber != ID_WRAITH3)
		{
			// WRAITH1 AND WRAITH2 can die on contact with water.
			// WRAITH1 simply dies, WRAITH2 triggers flipmap to make ice.
			if (TestEnvironment(ENV_FLAG_WATER, item.RoomNumber))
			{
				TriggerExplosionSparks(item.Pose.Position.x, item.Pose.Position.y, item.Pose.Position.z, 2, -2, 1, item.RoomNumber);

				item.ItemFlags[1]--;
				if (item.ItemFlags[1] < -1)
				{
					if (item.ItemFlags[1] < 30)
					{
						if (item.ObjectNumber == ID_WRAITH2)
						{
							if (item.TriggerFlags)
							{
								if (!FlipStats[item.TriggerFlags])
								{
									DoFlipMap(item.TriggerFlags);
									FlipStats[item.TriggerFlags] = true;
								}
							}
						}

						KillItem(itemNumber);
					}
				}
				else
				{
					item.ItemFlags[1] = -1;
				}
			}
			else
			{
				item.ItemFlags[1]--;
				if (item.ItemFlags[1] < 0)
					item.ItemFlags[1] = 0;
			}
		}
		else
		{
			// Spawn effects if WRAITH3 is being sucked by trap object.
			if (target->ObjectNumber == ID_WRAITH_TRAP)
			{
				xl = target->Pose.Position.x - LaraItem->Pose.Position.x;
				yl = target->Pose.Position.y;
				zl = target->Pose.Position.z - LaraItem->Pose.Position.z;
				distancePlayer = SQUARE(xl) + SQUARE(zl);

				// WRAITH3 can escape it is not close to the trap and if lara is 2 Block away from the trap
				if (distance < WRAITH_TRAP_DISTANCE_MAX && distancePlayer < WRAITH_TRAP_DISTANCE_MAX)
				{
					if (target->TriggerFlags > 0)
					{
						auto arcOrigin = item.Pose.Position.ToVector3();
						auto arcTarget = GetJointPosition(target, 0).ToVector3();

						int amplitude = Random::GenerateInt(1, 16);

						SoundEffect(SFX_TR4_ELECTRIC_ARCING_LOOP, &Pose(Vector3i(arcOrigin)));

						SpawnElectricity(arcOrigin, arcTarget, amplitude, 255, 255, 255, 10, (int)ElectricityFlags::ThinIn, 12, 10);
						SpawnElectricity(arcOrigin, arcTarget, amplitude, 255, 255, 255, 10, (int)ElectricityFlags::ThinIn, 4, 10);
						SpawnElectricity(arcOrigin, arcTarget, amplitude, 255, 100, 0, 10, (int)ElectricityFlags::ThinIn, 3, 10);

						// Trigger attack sparks on WraithTrap.
						target->ItemFlags[6] = 1;
					}
				}
				else
				{
					item.ItemFlags[6] = 0;
					target->ItemFlags[6] = 0;
					target = LaraItem;
				}
			}
		}

		if (distance < BLOCK(28.25f) &&
			(abs(item.Pose.Position.y - target->Pose.Position.y + CLICK(1.5f))) < CLICK(1))
		{
			if (item.Animation.Velocity.z > 32)
				item.Animation.Velocity.z -= 12;

			if (target->IsLara())
			{
				DoDamage(target, distance / BLOCK(1));

				// WRAITH1 can burn player.
				if (item.ObjectNumber == ID_WRAITH1 && !TestEnvironment(ENV_FLAG_WATER, target->RoomNumber))
				{
					item.ItemFlags[1] += 400;
					if (item.ItemFlags[1] > 8000)
						ItemBurn(LaraItem);
				}
			}
			else if (target->ObjectNumber == ID_WRAITH_TRAP)
			{
				// ID_WRAITH_TRAP can kill WRAITH3.
				item.ItemFlags[7]++;

				if (item.ItemFlags[7] > 10)
				{
					item.Pose.Position = target->Pose.Position;
					item.Pose.Position.y -= CLICK(1.5f);

					SpawnWraithExplosion(item, Vector3(96.0f), -32.0f);
					SpawnWraithExplosion(item, Vector3(48.0f), 48.0f);

					if (target->TriggerFlags > 0)
						target->Animation.FrameNumber = 0;

					target->ItemFlags[6] = 0;
					DoDamage(target, INT_MAX);
					KillItem(itemNumber);
				}
			}
			else
			{
				// Target is another wraith (fire vs ice), they fight to the death.

				target->ItemFlags[7] = 10;

				if (item.ItemFlags[7])
				{
					if (item.ObjectNumber == ID_WRAITH1)
						SpawnWraithExplosion(item, Vector3(1.0f * UCHAR_MAX, 0.6f * UCHAR_MAX, 0.0f * UCHAR_MAX), 48.0f);
					else
						SpawnWraithExplosion(item, Vector3(0.0f * UCHAR_MAX, 0.5f * UCHAR_MAX, 1.0f * UCHAR_MAX), 48.0f);

					TriggerExplosionSparks(item.Pose.Position.x, item.Pose.Position.y, item.Pose.Position.z, 2, -2, 1, item.RoomNumber);

					target->ItemFlags[6] = 0;
					target->ItemFlags[7] = 0;
					item.ItemFlags[6] = 0;
					target = LaraItem;
					item.ItemFlags[7] = 0;
					DoDamage(&item, INT_MAX);
					KillItem(itemNumber);
				}
			}
		}
		else
		{
			if (Wibble & 10)
			{
				if (item.Animation.Velocity.z < WRAITH_VELOCITY)
					item.Animation.Velocity.z++;

				if (item.ItemFlags[6])
				{
					target->ItemFlags[7]--;
				}
			}
		}

		// Check if WRAITH is below floor or above ceiling and spawn wall effect
		pointColl = GetPointCollision(item);

		if (pointColl.GetFloorHeight() < item.Pose.Position.y ||
			pointColl.GetCeilingHeight() > item.Pose.Position.y)
		{
			if (!hasHitWall)
				WraithWallEffect(prevPos, item.Pose.Orientation.y - ANGLE(180.0f), item.ObjectNumber);
		}
		else if (hasHitWall)
		{
			WraithWallEffect(item.Pose.Position, item.Pose.Orientation.y, item.ObjectNumber);
		}

		// Update WRAITH nodes.
		auto* wraithPtr = &GetWraithInfo(item);

		int j = 0;
		for (int i = WRAITH_COUNT - 1; i > 0; i--)
		{
			wraithPtr[i - 1].Position += (wraithPtr[i - 1].Velocity / 16);
			wraithPtr[i - 1].Velocity -= (wraithPtr[i - 1].Velocity / 16);

			wraithPtr[i].Position = wraithPtr[i - 1].Position;
			wraithPtr[i].Velocity = wraithPtr[i - 1].Velocity;

			if (item.ObjectNumber == ID_WRAITH1)
			{
				wraithPtr[i].r = (GetRandomControl() & 0x3F) - 64;
				wraithPtr[i].g = 16 * (j + 1) + (GetRandomControl() & 0x3F);
				wraithPtr[i].b = GetRandomControl() & 0xF;
			}
			else if (item.ObjectNumber == ID_WRAITH2)
			{
				wraithPtr[i].r = GetRandomControl() & 0xF;
				wraithPtr[i].g = 16 * (j + 1) + (GetRandomControl() & 0x3F);
				wraithPtr[i].b = (GetRandomControl() & 0x3F) - 64;
			}
			else
			{
				wraithPtr[i].r = 8 * (j + 2) + (GetRandomControl() & 0x3F);
				wraithPtr[i].g = wraithPtr[i].r;
				wraithPtr[i].b = wraithPtr[i].r + (GetRandomControl() & 0xF);
			}

			j++;
		}

		wraithPtr[0].Position = item.Pose.Position;
		wraithPtr[0].Velocity = (item.Pose.Position - prevPos) * 4;

		// Standard wraith drawing code.
		DrawWraith(
			item.Pose.Position,
			wraithPtr[0].Velocity,
			item.ObjectNumber);

		DrawWraith(
			(prevPos + item.Pose.Position) / 2,
			wraithPtr[0].Velocity,
			item.ObjectNumber);

		SpawnWraithTails(item);

		// Lighting for wraith.
		byte r, g, b;
		if (item.ObjectNumber == ID_WRAITH3)
		{
			r = wraithPtr[5].r;
			g = wraithPtr[5].g;
			b = wraithPtr[5].b;
		}
		else
		{
			r = wraithPtr[1].r;
			g = wraithPtr[1].g;
			b = wraithPtr[1].b;
		}

		SpawnDynamicLight(
			wraithPtr[0].Position.x,
			wraithPtr[0].Position.y,
			wraithPtr[0].Position.z,
			16,
			r, g, b);
	}

	void DrawWraith(Vector3i pos, Vector3i velocity, int objectNumber)
	{
		auto& spark = *GetFreeParticle();

		spark.on = true;

		byte color;

		if (objectNumber == ID_WRAITH1)
		{
			spark.sR = (GetRandomControl() & 0x1F) + -128;
			spark.sB = 24;
			spark.sG = (GetRandomControl() & 0x1F) + 48;
			spark.dR = (GetRandomControl() & 0x1F) + -128;
			spark.dB = 24;
			spark.dG = (GetRandomControl() & 0x1F) + 64;
		}
		else if (objectNumber == ID_WRAITH2)
		{
			spark.sB = (GetRandomControl() & 0x1F) + -128;
			spark.sR = 24;
			spark.sG = (GetRandomControl() & 0x1F) + -128;
			spark.dB = (GetRandomControl() & 0x1F) + -128;
			spark.dR = 24;
			spark.dG = (GetRandomControl() & 0x1F) + 64;
		}
		else
		{
			color = (GetRandomControl() & 0x1F) + 64;
			spark.dG = color;
			spark.dR = color;
			spark.sB = color;
			spark.sG = color;
			spark.sR = color;
			spark.dB = spark.sB + (GetRandomControl() & 0x1F);
		}

		spark.colFadeSpeed = 4;
		spark.fadeToBlack = 7;
		spark.blendMode = BlendMode::Additive;
		unsigned char life = (GetRandomControl() & 7) + 12;
		spark.life = life;
		spark.sLife = life;
		spark.x = (GetRandomControl() & 0x1F) + pos.x - 16;
		spark.y = pos.y;
		spark.friction = 85;
		spark.flags = SP_EXPDEF | SP_DEF | SP_SCALE;
		spark.z = (GetRandomControl() & 0x1F) + pos.z - 16;
		spark.xVel = velocity.x;
		spark.yVel = velocity.y;
		spark.zVel = velocity.z;
		spark.gravity = 0;
		spark.maxYvel = 0;
		spark.scalar = 2;
		spark.dSize = 2;
		unsigned char size = (GetRandomControl() & 0x1F) + 48;
		spark.sSize = size;
		spark.size = size;
	}

	void KillWraith(ItemInfo* item)
	{
		ItemInfo* item2 = nullptr;

		if (NextItemActive != NO_VALUE)
		{
			for (; NextItemActive != NO_VALUE;)
			{
				auto* item2 = &g_Level.Items[NextItemActive];
				if (item2->ObjectNumber == ID_WRAITH3 && !item2->HitPoints)
					break;

				if (item2->NextActive == NO_VALUE)
				{
					FlipEffect = NO_VALUE;
					return;
				}
			}

			item2->HitPoints = item->Index;
		}

		FlipEffect = NO_VALUE;
	}
}
