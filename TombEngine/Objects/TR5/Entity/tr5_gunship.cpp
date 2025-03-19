#include "framework.h"
#include "Objects/TR5/Entity/tr5_gunship.h"

#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/control/los.h"
#include "Game/effects/debris.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Objects/Generic/Object/objects.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;

namespace TEN::Entities::Creatures::TR5
{
	int GunShipCounter = 0;

	void ControlGunShip(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		if (TriggerActive(item))
		{
			SoundEffect(SFX_TR4_HELICOPTER_LOOP, &item->Pose);

			auto pos = GameVector(
				GetJointPosition(
					LaraItem, 
					LM_TORSO,
					Vector3i(
						(GetRandomControl() & 0x1FF) - 255,
						(GetRandomControl() & 0x1FF) - 255,
						(GetRandomControl() & 0x1FF) - 255
					))
			);

			auto target = pos;

			if (!item->ItemFlags[0] && !item->ItemFlags[1] && !item->ItemFlags[2])
			{
				item->ItemFlags[0] = pos.x / 16;
				item->ItemFlags[1] = pos.y / 16;
				item->ItemFlags[2] = pos.z / 16;
			}

			pos.x = (pos.x + 80 * item->ItemFlags[0]) / 6;
			pos.y = (pos.y + 80 * item->ItemFlags[1]) / 6;
			pos.z = (pos.z + 80 * item->ItemFlags[2]) / 6;

			item->ItemFlags[0] = pos.x / 16;
			item->ItemFlags[1] = pos.y / 16;
			item->ItemFlags[2] = pos.z / 16;

			if (item->TriggerFlags == 1)
				item->Pose.Position.z += (pos.z - item->Pose.Position.z) / 32;
			else
				item->Pose.Position.x += (pos.x - item->Pose.Position.x) / 32;
			item->Pose.Position.y += (pos.y - item->Pose.Position.y - 256) / 32;

			GameVector origin;
			origin.x = GetRandomControl() + item->Pose.Position.x - 128;
			origin.y = GetRandomControl() + item->Pose.Position.y - 128;
			origin.z = GetRandomControl() + item->Pose.Position.z - 128;
			origin.RoomNumber = item->RoomNumber;
			bool los = LOS(&origin, &target);

			target.x = 3 * pos.x - 2 * origin.x;
			target.y = 3 * pos.y - 2 * origin.y;
			target.z = 3 * pos.z - 2 * origin.z;
			bool los2 = LOS(&origin, &target);

			if (los)
				GunShipCounter = 1;
			else
				GunShipCounter++;

			if (GunShipCounter <= 15)
				item->MeshBits |= 0x100;
			else
				item->MeshBits &= 0xFEFF;

			if (GunShipCounter < 15)
				SoundEffect(SFX_TR4_HK_FIRE, &item->Pose, SoundEnvironment::Land, 0.8f);

			if (!(GlobalCounter & 1))
				return AnimateItem(*item);

			Vector3i hitPos;
			MESH_INFO* hitMesh = nullptr;
			int objOnLos = ObjectOnLOS2(&origin, &target, &hitPos, &hitMesh, GAME_OBJECT_ID::ID_LARA);

			if (objOnLos == NO_LOS_ITEM || objOnLos < 0)
			{
				if (GunShipCounter >= 15)
					return AnimateItem(*item);

				SpawnDynamicLight(
					origin.x, origin.y, origin.z, 16,
					(GetRandomControl() & 0x3F) + 96,
					(GetRandomControl() & 0x1F) + 64,
					0);

				if (!los2)
				{
					TriggerRicochetSpark(target, 2 * GetRandomControl());
				}

				if (objOnLos < 0 && GetRandomControl() & 1)
				{
					if (Statics[hitMesh->staticNumber].shatterType != ShatterType::None)
					{
						ShatterObject(0, hitMesh, 64, target.RoomNumber, 0);
						TestTriggers(hitMesh->pos.Position.x, hitMesh->pos.Position.y, hitMesh->pos.Position.z, target.RoomNumber, true);
						SoundEffect(GetShatterSound(hitMesh->staticNumber), &hitMesh->pos);
					}

					TriggerRicochetSpark(GameVector(hitPos), 2 * GetRandomControl());
				}
			}
			else
			{
				auto* hitItem = &g_Level.Items[objOnLos];

				if (hitItem->ObjectNumber != ID_LARA)
				{
					if (hitItem->ObjectNumber >= ID_SMASH_OBJECT1 &&
						hitItem->ObjectNumber <= ID_SMASH_OBJECT16)
					{
						ExplodeItemNode(hitItem, 0, 0, 128);
						SmashObject(objOnLos);
						KillItem(objOnLos);
					}
				}
				else
				{
					SpawnDynamicLight(
						origin.x, origin.y, origin.z,
						16,
						(GetRandomControl() & 0x3F) + 96,
						(GetRandomControl() & 0x1F) + 64,
						0);

					DoBloodSplat(
						origin.x, origin.y, origin.z,
						(GetRandomControl() & 1) + 2,
						2 * GetRandomControl(),
						LaraItem->RoomNumber);

					DoDamage(LaraItem, 20);
				}
			}

			if (GunShipCounter < 15)
			{
				auto* spark = GetFreeParticle();

				spark->on = 1;
				spark->sR = spark->dR = (GetRandomControl() & 0x7F) + -128;
				spark->sG = (spark->dR / 2) + (GetRandomControl() & 0x7F);

				if (spark->sG > spark->sR)
					spark->sG = spark->sR;

				spark->sB = 0;
				spark->dB = 0;
				spark->dR = 0;
				spark->dG = 0;
				spark->colFadeSpeed = 12;
				spark->blendMode = BlendMode::Additive;
				spark->fadeToBlack = 0;
				spark->life = 12;
				spark->sLife = 12;
				spark->x = origin.x;
				spark->y = origin.y;
				spark->z = origin.z;
				spark->xVel = 4 * (target.x - origin.x);
				spark->yVel = 4 * (target.y - origin.y);
				spark->zVel = 4 * (target.z - origin.z);
				spark->friction = 0;
				spark->maxYvel = 0;
				spark->flags = SP_NONE;
			}

			AnimateItem(*item);
		}
	}
}
