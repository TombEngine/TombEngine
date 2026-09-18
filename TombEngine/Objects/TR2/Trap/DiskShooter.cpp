#include "framework.h"
#include "Objects/TR2/Trap/DiskShooter.h"

#include "Game/collision/collide_room.h"
#include "Game/effects/effects.h"
#include "Game/effects/spark.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Renderer/RendererEnums.h"
#include "Scripting/Internal/TEN/Properties/PropertyHandler.h"
#include "Scripting/Internal/TEN/Properties/PropertyNames.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Scripting::Properties;
using namespace TEN::Effects::Spark;

// NOTES:
// ItemFlags[0]: Delay between disks in frame time.
// ItemFlags[1]: Timer in frame time.

namespace TEN::Entities::Traps
{
	constexpr auto DISK_DEFAULT_HARM_DAMAGE	 = 45;
	constexpr auto DISK_DEFAULT_VELOCITY	 = BLOCK(0.25f);
	constexpr auto DISK_DEFAULT_DELAY		 = 32;

	void ControlDisk(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (item.TouchBits.TestAny())
		{
			if (PropertyHandler::Get(item, PropName_Poisonous, false))
				Lara.Status.Poison += 1;

			DoDamage(LaraItem, PropertyHandler::Get(item, PropName_Damage, DISK_DEFAULT_HARM_DAMAGE));
			DoBloodSplat(item.Pose.Position.x, item.Pose.Position.y, item.Pose.Position.z, (GetRandomControl() & 3) + 4, LaraItem->Pose.Orientation.y, LaraItem->RoomNumber);
			KillItem(itemNumber);
		}
		else
		{
			auto prevPos = item.Pose.Position;
			float vel = item.Animation.Velocity.z * phd_cos(item.Pose.Orientation.x);

			item.Pose.Position.x += vel * phd_sin(item.Pose.Orientation.y);
			item.Pose.Position.y -= item.Animation.Velocity.z * phd_sin(item.Pose.Orientation.x);
			item.Pose.Position.z += vel * phd_cos(item.Pose.Orientation.y);

			short roomNumber = item.RoomNumber;
			auto* floor = GetFloor(item.Pose.Position.x, item.Pose.Position.y, item.Pose.Position.z, &roomNumber);

			if (item.RoomNumber != roomNumber)
				ItemNewRoom(itemNumber, roomNumber);

			int height = GetFloorHeight(floor, item.Pose.Position.x, item.Pose.Position.y, item.Pose.Position.z);
			item.Floor = height;

			if (item.Pose.Position.y >= height)
			{
				for (int i = 0; i < 4; i++)
				{
					auto targetPos = GameVector(prevPos.x, item.Pose.Position.y, prevPos.z, item.RoomNumber);
					
					TriggerRicochetSpark(targetPos, 5, 1, Vector4(1.0f, 0.7f, 0.1f, 1.0f));
					SoundEffect(SFX_TR2_CIRCLE_BLADE_HIT, &item.Pose);
				}

				KillItem(itemNumber);
			}
		}
	}

	void ControlDiskShooter(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (TriggerActive(&item))
		{
			if (item.Active)
			{
				short delay = PropertyHandler::Get(item, PropName_Delay, DISK_DEFAULT_DELAY);

				if (item.ItemFlags[1] > 0)
				{
					item.ItemFlags[1]--;
					return;
				}
				else
				{
					item.ItemFlags[1] = delay;
				}
			}

			int diskItemNumber = CreateItem();
			if (diskItemNumber == NO_VALUE)
				return;

			auto& diskItem = g_Level.Items[diskItemNumber];
			diskItem.ObjectNumber = ID_DISK;
			diskItem.Pose.Position = GetJointPosition(item, 0, Vector3i(0, 0, CLICK(0.5f)));
			diskItem.Pose.Orientation = item.Pose.Orientation;
			diskItem.RoomNumber = item.RoomNumber;

			InitializeItem(diskItemNumber);

			diskItem.Animation.Velocity.z = DISK_DEFAULT_VELOCITY;
			
			diskItem.Properties.Set(PropName_Poisonous, PropertyHandler::Get(item, PropName_Poisonous, false));
			diskItem.Properties.Set(PropName_Damage, (float)PropertyHandler::Get(item, PropName_Damage, DISK_DEFAULT_HARM_DAMAGE));
			diskItem.Model.Color = item.Model.Color;

			AddActiveItem(diskItemNumber);
			diskItem.Status = ITEM_ACTIVE;

			SoundEffect(SFX_TR2_CIRCLE_BLADE, &diskItem.Pose);
		}
		else
		{
			item.Status = ITEM_NOT_ACTIVE;
			RemoveActiveItem(itemNumber, false);
			item.Active = false;
		}
	}

	void SpawnDiskShooter(const Vector3& pos, const Vector3& vel, bool isHit)
	{
		auto& part = *GetFreeParticle();

		part.on = true;
		
		part.sR = 16;
		part.sG = 8;
		part.sB = 4;
		
		part.dR = 64;
		part.dG = 48;
		part.dB = 32;

		part.colFadeSpeed = 8;
		part.fadeToBlack = 4;

		part.blendMode = BlendMode::Additive;

		part.life = part.sLife = (GetRandomControl() & 3) + 32;
	
		part.x = pos.x + ((GetRandomControl() & 31) - 16);
		part.y = pos.y + ((GetRandomControl() & 31) - 16);
		part.z = pos.z + ((GetRandomControl() & 31) - 16);
		
		if (isHit)
		{
			part.xVel = -vel.x + ((GetRandomControl() & 255) - 128);
			part.yVel = -(GetRandomControl() & 3) - 4;
			part.zVel = -vel.z + ((GetRandomControl() & 255) - 128);
			part.friction = 3;
		}
		else
		{
			if (vel.x != 0.0f)
			{
				part.xVel = -vel.x;
			}
			else
			{
				part.xVel = ((GetRandomControl() & 255) - 128);
			}

			part.yVel = -(GetRandomControl() & 3) - 4;
			if (vel.z != 0.0f)
			{
				part.zVel = -vel.z;
			}
			else
			{
				part.zVel = ((GetRandomControl() & 255) - 128);
			}

			part.friction = 3;
		}

		part.friction = 3;

		if (GetRandomControl() & 1)
		{
			part.flags = SP_EXPDEF | SP_ROTATE | SP_DEF | SP_SCALE;

			part.rotAng = GetRandomControl() & 0xFFF;
			if (GetRandomControl() & 1)
			{
				part.rotAdd = -16 - (GetRandomControl() & 0xF);
			}
			else
			{
				part.rotAdd = (GetRandomControl() & 0xF) + 16;
			}
		}
		else
		{
			part.flags = SP_EXPDEF | SP_DEF | SP_SCALE;
		}
	
		part.scalar = 1;

		int size = (GetRandomControl() & 63) + 72;
		if (isHit)
		{
			size /= 2;
			part.size =
			part.sSize = size *= 4;
			part.gravity = part.maxYvel = 0;
		}
		else
		{
			part.size = part.sSize = size >> 4;
			part.gravity = -(GetRandomControl() & 3) - 4;
			part.maxYvel = -(GetRandomControl() & 3) - 4;
		}

		part.dSize = size;
	}
}
