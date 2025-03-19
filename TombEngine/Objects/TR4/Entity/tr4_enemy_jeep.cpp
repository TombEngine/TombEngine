#include "framework.h"
#include "Objects/TR4/Entity/tr4_enemy_jeep.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/control/box.h"
#include "Game/control/lot.h"
#include "Game/control/trigger.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/itemdata/creature_info.h"
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
	void EnemyJeepLaunchGrenade(ItemInfo* item)
	{
		short grenadeItemNumber = CreateItem();

		if (grenadeItemNumber != NO_VALUE)
		{
			auto* grenadeItem = &g_Level.Items[grenadeItemNumber];

			grenadeItem->Model.Color = Vector4(0.5f, 0.5f, 0.5f, 1.0f);
			grenadeItem->ObjectNumber = ID_GRENADE;
			grenadeItem->RoomNumber = item->RoomNumber;

			InitializeItem(grenadeItemNumber);

			grenadeItem->Pose.Orientation.x = item->Pose.Orientation.x;
			grenadeItem->Pose.Orientation.y = item->Pose.Orientation.y - ANGLE(180.0f);
			grenadeItem->Pose.Orientation.z = 0;

			grenadeItem->Pose.Position.x = item->Pose.Position.x + BLOCK(1) * phd_sin(grenadeItem->Pose.Orientation.y);
			grenadeItem->Pose.Position.y = item->Pose.Position.y - CLICK(3);
			grenadeItem->Pose.Position.z = item->Pose.Position.x + BLOCK(1) * phd_cos(grenadeItem->Pose.Orientation.y);

			for (int i = 0; i < 5; i++)
				TriggerGunSmoke(item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z, 0, 0, 0, 1, LaraWeaponType::GrenadeLauncher, 32);

			if (Random::TestProbability(0.75f))
				grenadeItem->ItemFlags[0] = 1;
			else
				grenadeItem->ItemFlags[0] = 2;

			grenadeItem->Animation.ActiveState = grenadeItem->Pose.Orientation.x;
			grenadeItem->Animation.TargetState = grenadeItem->Pose.Orientation.y;
			grenadeItem->Animation.RequiredState = NO_VALUE;
			grenadeItem->Animation.Velocity.z = 32;
			grenadeItem->Animation.Velocity.y = -32 * phd_sin(grenadeItem->Pose.Orientation.x);
			grenadeItem->HitPoints = 120;

			AddActiveItem(grenadeItemNumber);
		}
	}

	void InitializeEnemyJeep(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		item->ItemFlags[0] = -80;

		if (g_Level.NumItems > 0)
		{
			for (int i = 0; i < g_Level.NumItems; i++)
			{
				auto* other = &g_Level.Items[i];

				if (other == item || other->TriggerFlags != item->TriggerFlags)
					continue;

				item->ItemFlags[1] = i;
				other->ItemFlags[0] = -80;
				other->Pose.Position.y = item->Pose.Position.y - BLOCK(1);
			}
		}
	}

	void EnemyJeepControl(short itemNumber)
	{
		if (CreatureActive(itemNumber))
		{
			auto* item = &g_Level.Items[itemNumber];
			auto* creature = GetCreatureInfo(item);

			int x = item->Pose.Position.x;
			int y = item->Pose.Position.y;
			int z = item->Pose.Position.z;

			int dx = 682 * phd_sin(item->Pose.Orientation.y);
			int dz = 682 * phd_cos(item->Pose.Orientation.y);

			int height1 = GetPointCollision(Vector3i(x - dz, y, z - dx), item->RoomNumber).GetFloorHeight();
			if (abs(item->Pose.Position.y - height1) > CLICK(3))
			{
				item->Pose.Position.x += dz / 64;
				item->Pose.Position.z += dx / 64;
				item->Pose.Orientation.y += ANGLE(2.0f);
				height1 = y;
			}

			int height2 = GetPointCollision(Vector3i(x + dz, y, z - dx), item->RoomNumber).GetFloorHeight();
			if (abs(item->Pose.Position.y - height2) > CLICK(3))
			{
				item->Pose.Orientation.y -= ANGLE(2.0f);
				item->Pose.Position.x -= dz / 64;
				item->Pose.Position.z += dx / 64;
				height2 = y;
			}

			short zRot = phd_atan(1364, height2 - height1);

			int height3 = GetPointCollision(Vector3i(x + dx, y, z + dz), item->RoomNumber).GetFloorHeight();
			if (abs(y - height3) > CLICK(3))
				height3 = y;

			int height4 = GetPointCollision(Vector3i(x - dx, y, z - dz), item->RoomNumber).GetFloorHeight();
			if (abs(y - height4) > CLICK(3))
				height4 = y;

			short xRot = phd_atan(1364, height4 - height3);

			AI_INFO AI;
			CreatureAIInfo(item, &AI);

			creature->Enemy = creature->AITarget;

			auto* target = creature->AITarget;

			dx = LaraItem->Pose.Position.x - item->Pose.Position.x;
			dz = LaraItem->Pose.Position.z - item->Pose.Position.z;
			short angle = phd_atan(dz, dx) - item->Pose.Orientation.y;

			int distance;
			if (dx > BLOCK(31.25f) || dx < -BLOCK(31.25f) ||
				dz > BLOCK(31.25f) || dz < -BLOCK(31.25f))
			{
				distance = INT_MAX;
			}
			else
				distance = pow(dx, 2) + pow(dz, 2);

			auto pos = Vector3i::Zero;
			switch (item->Animation.ActiveState)
			{
			case 0:
			case 2:
				item->ItemFlags[0] -= 128;
				item->MeshBits = -98305;

				pos = GetJointPosition(item, 11, Vector3i(0, -144, -1024));
				SpawnDynamicLight(pos.x, pos.y, pos.z, 10, 64, 0, 0);

				if (item->ItemFlags[0] < 0)
					item->ItemFlags[0] = 0;

				if (item->Animation.RequiredState != NO_VALUE)
					item->Animation.TargetState = item->Animation.RequiredState;
				else if (AI.distance > pow(BLOCK(1), 2) || Lara.Location >= item->ItemFlags[3])
					item->Animation.TargetState = 1;

				break;

			case 1:
				item->ItemFlags[0] += 37;
				item->MeshBits = 0xFFFDBFFF;
				creature->MaxTurn = item->ItemFlags[0] / 16;

				if (item->ItemFlags[0] > 8704)
					item->ItemFlags[0] = 8704;

				if (AI.angle <= ANGLE(1.4f))
				{
					if (AI.angle < -ANGLE(1.4f))
						item->Animation.TargetState = 3;
				}
				else
					item->Animation.TargetState = 4;

				break;

			case 3:
			case 4:
				item->Animation.TargetState = 1;
				item->ItemFlags[0] += 18;

				if (item->ItemFlags[0] > 8704)
					item->ItemFlags[0] = 8704;

				break;

			case 5:
				if (item->ItemFlags[0] < 1184)
					item->ItemFlags[0] = 1184;

				break;

			default:
				break;
			}

			if (height3 <= (item->Floor + CLICK(2)))
			{
				if (height4 > (item->Floor + CLICK(2)) && item->Animation.ActiveState != 5)
				{
					item->Animation.AnimNumber = 8;
					item->Animation.FrameNumber = 0;
					item->Animation.ActiveState = 5;
					item->Animation.TargetState = 1;
					item->ItemFlags[1] = 0;
				}
			}
			else
			{
				creature->LOT.RequiredBox |= 8;

				if (item->ItemFlags[1] > 0)
				{
					item->ItemFlags[1] -= 8;
					item->Pose.Position.y += item->ItemFlags[1] / 64;

					if (item->ItemFlags[1] < 0)
						creature->LOT.RequiredBox &= ~8;
				}
				else
				{
					item->ItemFlags[1] = 2 * xRot;
					creature->LOT.RequiredBox |= 8u;
				}

				if (creature->LOT.RequiredBox & 8)
				{
					item->Animation.TargetState = 1;
					creature->MaxTurn = 0;
				}
			}

			if (AI.distance < pow(BLOCK(1.5f), 2) || item->ItemFlags[3] == -2)
				creature->ReachedGoal = true;

			if (creature->ReachedGoal)
			{
				TestTriggers(target, true);

				if (Lara.Location < item->ItemFlags[3] && item->Animation.ActiveState != 2 && item->Animation.TargetState != 2)
				{
					item->Animation.AnimNumber = 1;
					item->Animation.FrameNumber = 0;
					item->Animation.TargetState = 2;
					item->Animation.ActiveState = 2;

					if (target->Flags & 4)
					{
						item->Pose = target->Pose;

						if (item->RoomNumber != target->RoomNumber)
							ItemNewRoom(itemNumber, target->RoomNumber);
					}
				}

				if (distance > pow(BLOCK(2), 2) &&
					distance < pow(BLOCK(10), 2) &&
					!item->ItemFlags[2] &&
					(angle < -ANGLE(112.5f) || angle > ANGLE(112.5f)))
				{
					EnemyJeepLaunchGrenade(item);
					item->ItemFlags[2] = 150;
				}

				if (target->Flags == 62)
				{
					item->Status = ITEM_INVISIBLE;
					RemoveActiveItem(itemNumber);
					DisableEntityAI(itemNumber);
				}

				if (Lara.Location >= item->ItemFlags[3] || !(target->Flags & 4))
				{
					creature->ReachedGoal = false;
					item->ItemFlags[3]++;

					creature->Enemy = nullptr;
					AI_OBJECT* aiObject = nullptr;

					for (int i = 0; i < g_Level.AIObjects.size(); i++)
					{
						aiObject = &g_Level.AIObjects[i];

						if (g_Level.AIObjects[i].triggerFlags == item->ItemFlags[3] && g_Level.AIObjects[i].roomNumber != NO_VALUE)
						{
							aiObject = &g_Level.AIObjects[i];
							break;
						}
					}

					if (aiObject != nullptr)
					{
						creature->Enemy = nullptr;
						target->ObjectNumber = aiObject->objectNumber;
						target->RoomNumber = aiObject->roomNumber;
						target->Pose.Position = aiObject->pos.Position;
						target->Pose.Orientation.y = aiObject->pos.Orientation.y;
						target->Flags = aiObject->flags;
						target->TriggerFlags = aiObject->triggerFlags;
						target->BoxNumber = aiObject->boxNumber;

						if (!(aiObject->flags & 0x20))
						{
							target->Pose.Position.x += CLICK(1) * phd_sin(target->Pose.Orientation.y);
							target->Pose.Position.z += CLICK(1) * phd_cos(target->Pose.Orientation.y);
						}
					}
				}
			}

			item->ItemFlags[2]--;
			if (item->ItemFlags[2] < 0)
				item->ItemFlags[2] = 0;

			if (abs(xRot - item->Pose.Orientation.x) < ANGLE(1.4f))
				item->Pose.Orientation.x = xRot;
			else if (xRot < item->Pose.Orientation.x)
				item->Pose.Orientation.x -= ANGLE(1.4f);
			else
				item->Pose.Orientation.x += ANGLE(1.4f);

			if (abs(zRot - item->Pose.Orientation.z) < ANGLE(1.4f))
				item->Pose.Orientation.z = zRot;
			else if (zRot < item->Pose.Orientation.z)
				item->Pose.Orientation.z -= ANGLE(1.4f);
			else
				item->Pose.Orientation.z += ANGLE(1.4f);

			item->ItemFlags[0] += -2 - xRot / 512;
			if (item->ItemFlags[0] < 0)
				item->ItemFlags[0] = 0;

			dx = item->ItemFlags[0] * phd_sin(-2 - xRot / 512);
			dz = item->ItemFlags[0] * phd_cos(-2 - xRot / 512);

			item->Pose.Position.x += dx / 64;
			item->Pose.Position.z += dz / 64;

			for (int i = 0; i < 4; i++)
				creature->JointRotation[i] -= item->ItemFlags[0];

			if (!creature->ReachedGoal)
				ClampRotation(item->Pose, AI.angle, item->ItemFlags[0] / 16);

			creature->MaxTurn = 0;
			AnimateItem(*item);

			auto probe = GetPointCollision(*item);
			item->Floor = probe.GetFloorHeight();
			if (item->RoomNumber != probe.GetRoomNumber())
				ItemNewRoom(itemNumber, probe.GetRoomNumber());

			if (item->Pose.Position.y < item->Floor)
			{
				item->Animation.IsAirborne = true;
			}
			else
			{
				item->Pose.Position.y = item->Floor;
				item->Animation.IsAirborne = false;
				item->Animation.Velocity.y = 0;
			}

			SoundEffect(SFX_TR4_VEHICLE_JEEP_MOVING, &item->Pose, SoundEnvironment::Land, 1.0f + (float)item->ItemFlags[0] / BLOCK(8)); // TODO: Check actual sound!
		}
	}
}
