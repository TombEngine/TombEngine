#include "framework.h"
#include "Objects/TR5/Emitter/tr5_spider_emitter.h"

#include "Game/collision/collide_room.h"
#include "Game/control/flipeffect.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Sound/sound.h"
#include "Specific/level.h"

int NextSpider;
SpiderData Spiders[NUM_SPIDERS];

short GetNextSpider()
{
	short spiderNumber = NextSpider;
	auto* spider = &Spiders[NextSpider];

	int i = 0;
	while (spider->On)
	{
		if (spiderNumber == (NUM_SPIDERS - 1))
		{
			spider = &Spiders[0];
			spiderNumber = 0;
		}
		else
		{
			++spiderNumber;
			++spider;
		}

		if (++i >= NUM_SPIDERS)
			return NO_VALUE;
	}

	NextSpider = (spiderNumber + 1) & (NUM_SPIDERS - 1);
	return spiderNumber;
}

void ClearSpiders()
{
	if (Objects[ID_SPIDERS_EMITTER].loaded)
	{
		ZeroMemory(Spiders, NUM_SPIDERS * sizeof(SpiderData));
		NextSpider = 0;
		FlipEffect = NO_VALUE;
	}
}

void InitializeSpiders(short itemNumber)
{
	auto* item = &g_Level.Items[itemNumber];

	short flags = item->TriggerFlags / -24;

	item->Pose.Orientation.x = ANGLE(45.0f);
	item->ItemFlags[1] = flags & 2;
	item->ItemFlags[2] = flags & 4;
	item->ItemFlags[0] = flags & 1;
	item->TriggerFlags = flags % 1000;

	if (flags & 1)
	{
		ClearSpiders();
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

	ClearSpiders();
}

void SpidersEmitterControl(short itemNumber)
{
	auto* item = &g_Level.Items[itemNumber];

	if (item->TriggerFlags)
	{
		if (!item->ItemFlags[2] || !(GetRandomControl() & 0xF))
		{
			item->TriggerFlags--;

			if (item->ItemFlags[2] && GetRandomControl() & 1)
				item->ItemFlags[2]--;

			short spiderNumber = GetNextSpider();
			if (spiderNumber != NO_VALUE)
			{
				auto* spider = &Spiders[spiderNumber];

				spider->Pose.Position = item->Pose.Position;
				spider->RoomNumber = item->RoomNumber;

				if (item->ItemFlags[0])
				{
					spider->Pose.Orientation.y = 2 * GetRandomControl();
					spider->VerticalVelocity = -16 - (GetRandomControl() & 0x1F);
				}
				else
				{
					spider->Pose.Orientation.y = item->Pose.Orientation.y + (GetRandomControl() & 0x3FFF) - ANGLE(45.0f);
					spider->VerticalVelocity = 0;
				}

				spider->Pose.Orientation.x = 0;
				spider->Pose.Orientation.z = 0;
				spider->On = true;
				spider->Flags = 0;
				spider->Velocity = (GetRandomControl() & 0x1F) + 1;
			}
		}
	}
}

void UpdateSpiders()
{
	if (Objects[ID_SPIDERS_EMITTER].loaded)
	{
		for (int i = 0; i < NUM_SPIDERS; i++)
		{
			auto* spider = &Spiders[i];

			if (spider->On)
			{
				spider->StoreInterpolationData();

				int x = spider->Pose.Position.x;
				int y = spider->Pose.Position.y;
				int z = spider->Pose.Position.z;

				spider->Pose.Position.x += spider->Velocity * phd_sin(spider->Pose.Orientation.y);
				spider->Pose.Position.y += spider->VerticalVelocity;
				spider->Pose.Position.z += spider->Velocity * phd_cos(spider->Pose.Orientation.y);
				spider->VerticalVelocity += g_GameFlow->GetSettings()->Physics.Gravity;

				int dx = LaraItem->Pose.Position.x - spider->Pose.Position.x;
				int dy = LaraItem->Pose.Position.y - spider->Pose.Position.y;
				int dz = LaraItem->Pose.Position.z - spider->Pose.Position.z;

				short angle = phd_atan(dz, dx) - spider->Pose.Orientation.y;

				if (abs(dx) < 85 && abs(dy) < 85 && abs(dz) < 85)
				{
					DoDamage(LaraItem, 3);
					TriggerBlood(spider->Pose.Position.x, spider->Pose.Position.y, spider->Pose.Position.z, spider->Pose.Orientation.y, 1);
				}

				if (spider->Flags)
				{
					if (abs(dx) + abs(dz) <= CLICK(3))
					{
						if (spider->Velocity & 1)
							spider->Pose.Orientation.y += ANGLE(2.8f);
						else
							spider->Pose.Orientation.y -= ANGLE(2.8f);

						spider->Velocity = 48 - (abs(angle) / ANGLE(5.6f));
					}
					else
					{
						if (spider->Velocity < (i & 0x1F) + 24)
							spider->Velocity++;

						if (abs(angle) >= ANGLE(11.25f))
						{
							if (angle >= 0)
								spider->Pose.Orientation.y += ANGLE(5.6f);
							else
								spider->Pose.Orientation.y -= ANGLE(5.6f);
						}
						else
							spider->Pose.Orientation.y += 8 * (Wibble - i);
					}
				}

				FloorInfo* floor = GetFloor(spider->Pose.Position.x, spider->Pose.Position.y, spider->Pose.Position.z,&spider->RoomNumber);
				int height = GetFloorHeight(floor, spider->Pose.Position.x, spider->Pose.Position.y, spider->Pose.Position.z);

				if (height >= spider->Pose.Position.y - CLICK(5) || height == -BLOCK(31.75f))
				{
					if (height >= spider->Pose.Position.y - 64)
					{
						if (spider->Pose.Position.y <= height)
						{
							if (spider->VerticalVelocity >= 500)
							{
								spider->On = false;
								NextSpider = 0;
							}
							else
								spider->Pose.Orientation.x = -128 * spider->VerticalVelocity;
						}
						else
						{
							spider->Pose.Position.y = height;
							spider->VerticalVelocity = 0;
							spider->Flags = 1;
						}
					}
					else
					{
						spider->Pose.Position.x = x;
						spider->Pose.Position.y = y - 8;
						spider->Pose.Position.z = z;
						spider->Pose.Orientation.x = ANGLE(78.75f);
						spider->VerticalVelocity = 0;

						if (!(GetRandomControl() & 0x1F))
							spider->Pose.Orientation.y += -ANGLE(180.0f);
					}
				}
				else
				{
					if (angle <= 0)
						spider->Pose.Orientation.y -= ANGLE(90.0f);
					else
						spider->Pose.Orientation.y += ANGLE(90.0f);

					spider->Pose.Position.x = x;
					spider->Pose.Position.y = y;
					spider->Pose.Position.z = z;
					spider->VerticalVelocity = 0;
				}

				if (spider->Pose.Position.y < g_Level.Rooms[spider->RoomNumber].TopHeight + 50)
				{
					spider->Pose.Position.y = g_Level.Rooms[spider->RoomNumber].TopHeight + 50;
					spider->Pose.Orientation.y += -ANGLE(180.0f);
					spider->VerticalVelocity = 1;
				}

				if (!i && !(GetRandomControl() & 4))
					SoundEffect(SFX_TR5_INSECTS,&spider->Pose);

				auto tMatrix = Matrix::CreateTranslation(spider->Pose.Position.ToVector3());
				auto rotMatrix = spider->Pose.Orientation.ToRotationMatrix();
				spider->Transform = rotMatrix * tMatrix;
			}
		}
	}
}
