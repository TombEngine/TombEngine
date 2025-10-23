#include "framework.h"
#include "Objects/TR5/Emitter/tr5_rats_emitter.h"

#include "Game/collision/collide_room.h"
#include "Game/control/flipeffect.h"
#include "Game/effects/effects.h"
#include "Game/effects/Ripple.h"
#include "Game/effects/tomb4fx.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Effects::Ripple;

int NextRat;
RatData Rats[NUM_RATS];

short GetNextRat()
{
	short ratNumber = NextRat;
	auto* rat = &Rats[NextRat];

	int i = 0;
	while (rat->On)
	{
		if (ratNumber == NUM_RATS - 1)
		{
			rat = &Rats[0];
			ratNumber = 0;
		}
		else
		{
			ratNumber++;
			rat++;
		}

		i++;

		if (i >= NUM_RATS)
			return NO_VALUE;
	}

	NextRat = (ratNumber + 1) & 0x1F;
	return ratNumber;
}

void LittleRatsControl(short itemNumber)
{
	auto* item = &g_Level.Items[itemNumber];

	if (item->TriggerFlags)
	{
		if (!item->ItemFlags[2] || !(GetRandomControl() & 0xF))
		{
			item->TriggerFlags--;

			if (item->ItemFlags[2] && GetRandomControl() & 1)
				item->ItemFlags[2]--;

			short ratNumber = GetNextRat();
			if (ratNumber != -1)
			{
				auto* rat = &Rats[ratNumber];

				rat->Pose.Position.x = item->Pose.Position.x;
				rat->Pose.Position.y = item->Pose.Position.y;
				rat->Pose.Position.z = item->Pose.Position.z;
				rat->RoomNumber = item->RoomNumber;

				if (item->ItemFlags[0])
				{
					rat->Pose.Orientation.y = 2 * GetRandomControl();
					rat->VerticalVelocity = -16 - (GetRandomControl() & 31);
				}
				else
				{
					rat->VerticalVelocity = 0;
					rat->Pose.Orientation.y = item->Pose.Orientation.y + (GetRandomControl() & 0x3FFF) - ANGLE(45);
				}

				rat->Pose.Orientation.x = 0;
				rat->Pose.Orientation.z = 0;
				rat->On = true;
				rat->Flags = GetRandomControl() & 30;
				rat->Velocity = (GetRandomControl() & 31) + 1;
			}
		}
	}
}

void ClearRats()
{
	if (Objects[ID_RATS_EMITTER].loaded)
	{
		ZeroMemory(Rats, NUM_RATS * sizeof(RatData));
		NextRat = 0;
		FlipEffect = NO_VALUE;
	}
}

void InitializeLittleRats(short itemNumber)
{
	auto* item = &g_Level.Items[itemNumber];

	char flags = item->TriggerFlags / 1000;

	item->Pose.Orientation.x = ANGLE(45.0f);
	item->ItemFlags[1] = flags & 2;
	item->ItemFlags[2] = flags & 4;
	item->ItemFlags[0] = flags & 1;
	item->TriggerFlags = item->TriggerFlags % 1000;

	if (flags & 1)
	{
		ClearRats();
		return;
	}

	if (item->Pose.Orientation.y > -ANGLE(157.5f) && item->Pose.Orientation.y < -ANGLE(22.5f))
		item->Pose.Position.x += CLICK(2);
	else if (item->Pose.Orientation.y > ANGLE(22.5f) && item->Pose.Orientation.y < ANGLE(157.5f))
		item->Pose.Position.x -= CLICK(2);

	if (item->Pose.Orientation.y > -ANGLE(45.0f) && item->Pose.Orientation.y < ANGLE(45.0f))
		item->Pose.Position.z -= CLICK(2);
	else if (item->Pose.Orientation.y < -ANGLE(112.5f) || item->Pose.Orientation.y > ANGLE(112.5f))
		item->Pose.Position.z += CLICK(2);

	ClearRats();
}

void UpdateRats()
{
	if (Objects[ID_RATS_EMITTER].loaded)
	{
		for (int i = 0; i < NUM_RATS; i++)
		{
			auto* rat = &Rats[i];

			if (rat->On)
			{
				rat->StoreInterpolationData();

				int oldX = rat->Pose.Position.x;
				int oldY = rat->Pose.Position.y;
				int oldZ = rat->Pose.Position.z;

				rat->Pose.Position.x += rat->Velocity * phd_sin(rat->Pose.Orientation.y);
				rat->Pose.Position.y += rat->VerticalVelocity;
				rat->Pose.Position.z += rat->Velocity * phd_cos(rat->Pose.Orientation.y);

				rat->VerticalVelocity += g_GameFlow->GetSettings()->Physics.Gravity;

				int dx = LaraItem->Pose.Position.x - rat->Pose.Position.x;
				int dy = LaraItem->Pose.Position.y - rat->Pose.Position.y;
				int dz = LaraItem->Pose.Position.z - rat->Pose.Position.z;

				short angle;
				if (rat->Flags >= 170)
					angle = rat->Pose.Orientation.y - (short)phd_atan(dz, dx);
				else
					angle = (short)phd_atan(dz, dx) - rat->Pose.Orientation.y;

				if (abs(dx) < 85 && abs(dy) < 85 && abs(dz) < 85)
				{
					LaraItem->HitPoints--;
					LaraItem->HitStatus = true;
				}

				// if life is even
				if (rat->Flags & 1)
				{
					// if rat is very near
					if (abs(dz) + abs(dx) <= BLOCK(1))
					{
						if (rat->Velocity & 1)
							rat->Pose.Orientation.y += ANGLE(2.8f);
						else
							rat->Pose.Orientation.y -= ANGLE(2.8f);
						rat->Velocity = 48 - (abs(angle) / ANGLE(5.6f));
					}
					else
					{
						if (rat->Velocity < (i & 31) + 24)
							rat->Velocity++;

						if (abs(angle) >= ANGLE(11.25f))
						{
							if (angle >= 0)
								rat->Pose.Orientation.y += ANGLE(5.6f);
							else
								rat->Pose.Orientation.y -= ANGLE(5.6f);
						}
						else
							rat->Pose.Orientation.y += 8 * (Wibble - i);
					}
				}

				short oldRoomNumber = rat->RoomNumber;

				FloorInfo* floor = GetFloor(rat->Pose.Position.x, rat->Pose.Position.y, rat->Pose.Position.z,&rat->RoomNumber);
				int height = GetFloorHeight(floor, rat->Pose.Position.x, rat->Pose.Position.y, rat->Pose.Position.z);

				// if height is higher than 5 clicks 
				if (height < rat->Pose.Position.y - 1280 ||
					height == NO_HEIGHT)
				{
					// if timer is higher than 170 time to disappear 
					if (rat->Flags > 170)
					{
						rat->On = 0;
						NextRat = 0;
					}

					if (angle <= 0)
						rat->Pose.Orientation.y -= ANGLE(90.0f);
					else
						rat->Pose.Orientation.y += ANGLE(90.0f);

					// reset rat to old Poseition and disable fall
					rat->Pose.Position.x = oldX;
					rat->Pose.Position.y = oldY;
					rat->Pose.Position.z = oldZ;
					rat->VerticalVelocity = 0;
				}
				else
				{
					// if height is lower than Y + 64
					if (height >= rat->Pose.Position.y - 64)
					{
						// if rat is higher than floor
						if (height >= rat->Pose.Position.y)
						{
							// if VerticalVelocity is too much or life is ended then kill rat
							if (rat->VerticalVelocity >= 500 ||
								rat->Flags >= 200)
							{
								rat->On = 0;
								NextRat = 0;
							}
							else
								rat->Pose.Orientation.x = -128 * rat->VerticalVelocity;
						}
						else
						{
							rat->Pose.Position.y = height;
							rat->VerticalVelocity = 0;
							rat->Flags |= 1;
						}
					}
					else
					{
						// if block is higher than rat Poseition then run vertically
						rat->Pose.Orientation.x = ANGLE(78.75f);
						rat->Pose.Position.x = oldX;
						rat->Pose.Position.y = oldY - 24;
						rat->Pose.Position.z = oldZ;
						rat->VerticalVelocity = 0;
					}
				}

				if (!(Wibble & 60))
					rat->Flags += 2;

				auto* room = &g_Level.Rooms[rat->RoomNumber];

				if (TestEnvironment(ENV_FLAG_WATER, room))
				{
					rat->Pose.Position.y = room->TopHeight + 50;
					rat->Velocity = 16;
					rat->VerticalVelocity = 0;

					if (TestEnvironment(ENV_FLAG_WATER, oldRoomNumber))
					{
						if (!(GetRandomControl() & 0xF))
							SpawnRipple(
								Vector3(rat->Pose.Position.x, room->TopHeight, rat->Pose.Position.z),
								rat->RoomNumber,
								Random::GenerateFloat(48.0f, 52.0f),
								(int)RippleFlags::SlowFade);
					}
					else
					{
						AddWaterSparks(rat->Pose.Position.x, room->TopHeight, rat->Pose.Position.z, 16);
						SpawnRipple(
							Vector3(rat->Pose.Position.x, room->TopHeight, rat->Pose.Position.z),
							rat->RoomNumber,
							Random::GenerateFloat(48.0f, 52.0f),
							(int)RippleFlags::SlowFade);
						
						SoundEffect(SFX_TR5_RATS_SPLASH,&rat->Pose);
					}
				}

				if (!i && !(GetRandomControl() & 4))
					SoundEffect(SFX_TR5_RATS,&rat->Pose);

				Matrix translation = Matrix::CreateTranslation(rat->Pose.Position.x, rat->Pose.Position.y, rat->Pose.Position.z);
				Matrix rotation = rat->Pose.Orientation.ToRotationMatrix();
				rat->Transform = rotation * translation;
			}
		}
	}
}
