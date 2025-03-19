#include "framework.h"
#include "Objects/TR4/Object/tr4_clockwork_beetle.h"

#include "Game/items.h"
#include "Specific/level.h"
#include "Game/Lara/lara.h"
#include "Game/Animation/Animation.h"
#include "Sound/sound.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/effects/debris.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;

void ClockworkBeetleControl(short itemNumber)
{
	auto* beetle = &g_Level.Items[itemNumber];

	int flag = 0;

	if (LaraItem->Animation.AnimNumber == LA_MECHANICAL_BEETLE_USE)
	{
		if (LaraItem->Animation.FrameNumber < 14)
		{
			beetle->Status = ITEM_INVISIBLE;
			return;
		}

		if (LaraItem->Animation.FrameNumber < 104)
		{
			auto pos = GetJointPosition(LaraItem, LM_RHAND, Vector3i(0, 0, -32));

			beetle->Pose.Position = pos;
			beetle->Status = ITEM_ACTIVE;
			beetle->Pose.Orientation.y = LaraItem->Pose.Orientation.y;
			beetle->Pose.Orientation.z = ANGLE(-70.0f);
			return;
		}

		if (LaraItem->Animation.FrameNumber == 104)
		{
			short roomNumber = beetle->RoomNumber;
			FloorInfo* floor = GetFloor(beetle->Pose.Position.x, beetle->Pose.Position.y, beetle->Pose.Position.z, &roomNumber);
			int height = GetFloorHeight(floor, beetle->Pose.Position.x, beetle->Pose.Position.y, beetle->Pose.Position.z);

			if (abs(LaraItem->Pose.Position.y - height) > 64)
			{
				beetle->Pose.Position.x = LaraItem->Pose.Position.x;
				beetle->Pose.Position.y = LaraItem->Pose.Position.y;
				beetle->Pose.Position.z = LaraItem->Pose.Position.z;
			}

			return;
		}
	}

	SoundEffect(SFX_TR4_CLOCKWORK_BEETLE_WINDUP, &beetle->Pose);

	beetle->Animation.Velocity.y += 12;
	beetle->Pose.Position.y += beetle->Animation.Velocity.y;

	short roomNumber = beetle->RoomNumber;
	FloorInfo* floor = GetFloor(beetle->Pose.Position.x, beetle->Pose.Position.y - 20, beetle->Pose.Position.z, &roomNumber);
	int height = GetFloorHeight(floor, beetle->Pose.Position.x, beetle->Pose.Position.y, beetle->Pose.Position.z);

	if (beetle->Pose.Position.y > height)
	{
		beetle->Pose.Position.y = height;

		if (beetle->Animation.Velocity.y <= 32)
			beetle->Animation.Velocity.y = 0;
		else
			beetle->Animation.Velocity.y = -beetle->Animation.Velocity.y / 2;

		flag = 1;
	}

	TestTriggers(beetle, false);

	if (roomNumber != beetle->RoomNumber)
		ItemNewRoom(itemNumber, roomNumber);

	if (beetle->ItemFlags[0])
	{
		beetle->Pose.Orientation.z = ANGLE(22.5f) * phd_sin(ANGLE(22.5f) * (GlobalCounter & 0xF));

		switch (beetle->ItemFlags[2])
		{
		case 0:
		{
			int x, z;

			x = ((beetle->Pose.Position.x & -CLICK(2)) | CLICK(2)) - beetle->Pose.Position.x;
			z = ((beetle->Pose.Position.z & -CLICK(2)) | CLICK(2)) - beetle->Pose.Position.z;

			if (x <= -8 || z <= -8 || x >= 8 || z >= 8)
			{
				int atan = phd_atan(z, x);
				short rot = atan - beetle->Pose.Orientation.y;

				if (abs(rot) > ANGLE(180.0f))
					rot = beetle->Pose.Orientation.y - atan;

				if (abs(rot) < ANGLE(1.4f))
				{
					beetle->Pose.Orientation.y = atan;
					beetle->ItemFlags[2] = 1;
				}
				else if (rot < 0)
					beetle->Pose.Orientation.y -= ANGLE(1.4f);
				else
					beetle->Pose.Orientation.y += ANGLE(1.4f);
			}
			else
			{
				beetle->Pose.Position.z &= -CLICK(2);
				beetle->Pose.Position.z &= -CLICK(2);
				beetle->ItemFlags[2] = 2;
			}

			break;
		}

		case 1:
		case 4:
		{
			int x, z;

			x = (beetle->Pose.Position.x & -CLICK(2)) | CLICK(2);
			z = (beetle->Pose.Position.z & -CLICK(2)) | CLICK(2);
			x -= beetle->Pose.Position.x;
			z -= beetle->Pose.Position.z;

			if (x <= -8 || z <= -8 || x >= 8 || z >= 8)
			{
				int atan = phd_atan(z, x);
				beetle->Pose.Orientation.y = atan;

				if (pow(x, 2) + pow(z, 2) >= 0x19000)
				{
					if (beetle->Animation.Velocity.z < 32)
						beetle->Animation.Velocity.z++;
				}
				else
				{
					if (beetle->Animation.Velocity.z <= 4)
					{
						if (beetle->Animation.Velocity.z < 4)
							beetle->Animation.Velocity.z++;
					}
					else
						beetle->Animation.Velocity.z = beetle->Animation.Velocity.z - (beetle->ItemFlags[2] == 4) - 1;
				}

				beetle->Pose.Position.x += beetle->Animation.Velocity.z * phd_sin(beetle->Pose.Orientation.y);
				beetle->Pose.Position.z += beetle->Animation.Velocity.z * phd_cos(beetle->Pose.Orientation.y);
			}
			else
			{
				beetle->Pose.Position.x = (beetle->Pose.Position.x & -512) | 0x200;
				beetle->Pose.Position.z = (beetle->Pose.Position.z & -512) | 0x200;

				if (beetle->ItemFlags[2] == 1)
					beetle->ItemFlags[2] = 2;
				else
				{
					Lara.Inventory.BeetleLife--;
					beetle->ItemFlags[2] = 5;
					short itemRoom = g_Level.Rooms[beetle->RoomNumber].itemNumber;

					if (itemRoom != NO_VALUE)
					{
						ItemInfo* item;
						short nextItem;

						while (true)
						{
							item = &g_Level.Items[itemRoom];
							nextItem = item->NextItem;

							if (item->ObjectNumber == ID_MAPPER)
							{
								int dx = beetle->Pose.Position.x - item->Pose.Position.x;
								int dy = beetle->Pose.Position.y - item->Pose.Position.y;
								int dz = beetle->Pose.Position.z - item->Pose.Position.z;

								if (dx > -BLOCK(1) && dx < BLOCK(1) &&
									dz > -BLOCK(1) && dz < BLOCK(1) &&
									dy > -BLOCK(1) && dy < BLOCK(1))
								{
									break;
								}
							}

							itemRoom = nextItem;

							if (itemRoom == NO_VALUE)
								return;
						}

						item->ItemFlags[0] = 1;
					}
				}
			}

			break;
		}

		case 2:
		{
			int rotation = beetle->ItemFlags[1] - beetle->Pose.Orientation.y;

			if (abs(rotation) > ANGLE(180.0f))
				rotation = beetle->Pose.Orientation.y - beetle->ItemFlags[1];

			if (abs(rotation) < ANGLE(1.4f))
			{
				beetle->ItemFlags[2] = 3;
				beetle->Pose.Orientation.y = beetle->ItemFlags[1];
			}
			else
			{
				if (rotation < 0)
					beetle->Pose.Orientation.y -= ANGLE(1.4f);
				else
					beetle->Pose.Orientation.y += ANGLE(1.4f);
			}

			break;
		}

		case 3:
		{
			if (beetle->Animation.Velocity.z < 32)
				beetle->Animation.Velocity.z++;

			beetle->Pose.Position.x += beetle->Animation.Velocity.z * phd_sin(beetle->Pose.Orientation.y);
			beetle->Pose.Position.z += beetle->Animation.Velocity.z * phd_cos(beetle->Pose.Orientation.y);

			if (!floor->Flags.MarkBeetle)
				beetle->ItemFlags[3] = 1;
			else
			{
				if (beetle->ItemFlags[3])
					beetle->ItemFlags[2] = 4;
			}

			break;
		}

		default:
			break;
		}
	}
	else
	{
		beetle->Pose.Orientation.z = ANGLE(45.0f) * phd_sin(ANGLE(45.0f) * (GlobalCounter & 0x7));

		if (beetle->ItemFlags[3])
			beetle->ItemFlags[3]--;

		if (Lara.Inventory.BeetleLife)
		{
			int val;

			if (beetle->ItemFlags[3] <= 75)
				val = beetle->ItemFlags[3];
			else
				val = 150 - beetle->ItemFlags[3];

			beetle->Pose.Orientation.y += 32 * val;
			val >>= 1;

			if (flag && beetle->ItemFlags[3] > 30 && val)
			{
				beetle->Animation.Velocity.y = -((val >> 1) + GetRandomControl() % val);
				return;
			}
		}
		else
		{
			beetle->Pose.Orientation.z *= 2;
			int val = (150 - beetle->ItemFlags[3]) >> 1;
			beetle->Pose.Orientation.y += val << 7;

			if (flag && val)
			{
				beetle->Animation.Velocity.y = -((val >> 1) + GetRandomControl() % val);
				return;
			}

			if (beetle->ItemFlags[3] < 30)
			{
				SoundEffect(102, &beetle->Pose);
				ExplodeItemNode(beetle, 0, 0, 128);
				KillItem(itemNumber);
			}
		}
	}
}

void UseClockworkBeetle(short flag)
{
	if (flag ||
		LaraItem->Animation.ActiveState == LS_IDLE &&
		LaraItem->Animation.AnimNumber == LA_STAND_IDLE &&
		!LaraItem->HitStatus &&
		Lara.Control.HandStatus == HandStatus::Free)
	{
		short itemNumber = CreateItem();

		if (itemNumber != NO_VALUE)
		{
			auto* item = &g_Level.Items[itemNumber];

			Lara.Inventory.BeetleComponents &= 0xFE;
			item->Model.Color = Vector4(0.5f, 0.5f, 0.5f, 1.0f);
			item->ObjectNumber = ID_CLOCKWORK_BEETLE;
			item->RoomNumber = LaraItem->RoomNumber;
			item->Pose.Position = LaraItem->Pose.Position;

			InitializeItem(itemNumber);
			item->Pose.Orientation.x = 0;
			item->Pose.Orientation.y = LaraItem->Pose.Orientation.y;
			item->Pose.Orientation.z = 0;

			if (Lara.Inventory.BeetleLife)
				item->ItemFlags[0] = GetPointCollision(*item).GetSector().Flags.MarkBeetle;
			else
				item->ItemFlags[0] = 0;

			item->Animation.Velocity.z = 0;
			AddActiveItem(itemNumber);

			if (item->ItemFlags[0])
			{
				ItemInfo* item2;
				short itemRoom = g_Level.Rooms[item->RoomNumber].itemNumber;

				if (itemRoom != NO_VALUE)
				{
					while (true)
					{
						item2 = &g_Level.Items[itemRoom];
						short nextItem = item2->NextItem;

						if (item2->ObjectNumber == ID_MAPPER)
						{
							int dx = item->Pose.Position.x - item2->Pose.Position.x;
							int dy = item->Pose.Position.y - item2->Pose.Position.y;
							int dz = item->Pose.Position.z - item2->Pose.Position.z;

							if (dx > -BLOCK(1) && dx < BLOCK(1) &&
								dz > -BLOCK(1) && dz < BLOCK(1) &&
								dy > -BLOCK(1) && dy < BLOCK(1))
							{
								break;
							}
						}

						itemRoom = nextItem;

						if (itemRoom == NO_VALUE)
						{
							if (!item->ItemFlags[0])
								item->ItemFlags[3] = 150;

							return;
						}
					}

					item->ItemFlags[1] = item2->Pose.Orientation.y + ANGLE(180.0f);

					if (item2->ItemFlags[0])
						item->ItemFlags[0] = 0;
					else
						item2->ItemFlags[0] = 1;
				}
			}

			if (!item->ItemFlags[0])
				item->ItemFlags[3] = 150;
		}
	}
}
