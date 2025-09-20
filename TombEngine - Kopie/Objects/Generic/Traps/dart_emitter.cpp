#include "framework.h"
#include "Objects/Generic/Traps/dart_emitter.h"

#include "Game/collision/collide_room.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Renderer/RendererEnums.h"
#include "Sound/sound.h"
#include "Specific/level.h"

// NOTES:
// ItemFlags[0]: Delay between darts in frame time.
// ItemFlags[1]: Timer in frame time.

namespace TEN::Entities::Traps
{
	constexpr auto DART_DEFAULT_HARM_DAMAGE	 = 25;
	constexpr auto DART_DEFAULT_VELOCITY	 = BLOCK(0.25f);
	constexpr auto DART_DEFAULT_DELAY		 = 32;
	constexpr auto DART_DEFAULT_HOMING_DELAY = 24;

	void InitializeDartEmitter(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		auto& delay = item.ItemFlags[0];

		if (item.ObjectNumber == ID_HOMING_DART_EMITTER)
		{
			if (delay == 0)
				delay = DART_DEFAULT_HOMING_DELAY;
		}
		else
		{
			if (delay == 0)
				delay = DART_DEFAULT_DELAY;
		}
	}

	void ControlDart(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (item.TouchBits.TestAny())
		{
			if (item.TriggerFlags < 0)
				Lara.Status.Poison += 1;

			DoDamage(LaraItem, item.TriggerFlags ? abs(item.TriggerFlags) : DART_DEFAULT_HARM_DAMAGE);
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
					SpawnDartSmoke(Vector3(prevPos.x, item.Pose.Position.y, prevPos.z), Vector3::Zero, true);

				KillItem(itemNumber);
			}
		}
	}

	void ControlDartEmitter(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (TriggerActive(&item))
		{
			if (item.Active)
			{
				auto& delay = item.ItemFlags[0];
				auto& timer = item.ItemFlags[1];

				if (timer > 0)
				{
					timer--;
					return;
				}
				else
				{
					timer = delay;
				}
			}

			int dartItemNumber = CreateItem();
			if (dartItemNumber == NO_VALUE)
				return;

			auto& dartItem = g_Level.Items[dartItemNumber];
			dartItem.ObjectNumber = ID_DARTS;
			dartItem.Pose.Position = item.Pose.Position + Vector3i(0, -CLICK(0.9f), 0);
			dartItem.Pose.Orientation = item.Pose.Orientation + EulerAngles(0, ANGLE(180.0f), 0);
			dartItem.RoomNumber = item.RoomNumber;

			InitializeItem(dartItemNumber);

			dartItem.Animation.Velocity.z = DART_DEFAULT_VELOCITY;
			dartItem.TriggerFlags = item.TriggerFlags;
			dartItem.Model.Color = item.Model.Color;

			for (int i = 0; i < 4; i++)
				SpawnDartSmoke(dartItem.Pose.Position.ToVector3(), Vector3::Zero, false);

			AddActiveItem(dartItemNumber);
			dartItem.Status = ITEM_ACTIVE;

			SoundEffect(SFX_TR4_DART_SPIT, &dartItem.Pose);
		}
		else
		{
			item.Status = ITEM_NOT_ACTIVE;
			RemoveActiveItem(itemNumber, false);
			item.Active = false;
		}
	}

	void SpawnDartSmoke(const Vector3& pos, const Vector3& vel, bool isHit)
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
