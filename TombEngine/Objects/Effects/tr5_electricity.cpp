#include "framework.h"
#include "Objects/Effects/tr5_electricity.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/effects/item_fx.h"
#include "Game/effects/Ripple.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_collide.h"
#include "Game/Setup.h"
#include "Sound/sound.h"
#include "Specific/clock.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;
using namespace TEN::Effects::Items;
using namespace TEN::Effects::Ripple;

void TriggerElectricityWireSparks(int x, int z, byte objNum, byte node, bool glow)
{
	auto* spark = GetFreeParticle();
	spark->on = true;
	spark->sR = 255;
	spark->sG = 255;
	spark->sB = 255;
	spark->dR = 0;
	spark->dG = (GetRandomControl() & 0x7F) + 64;
	spark->dB = 255;

	if (glow)
	{
		spark->colFadeSpeed = 1;
		spark->fadeToBlack = 0;
		spark->life = spark->sLife = 4;
	}
	else
	{
		spark->colFadeSpeed = 3;
		spark->fadeToBlack = 4;
		spark->life = spark->sLife = 24;
	}

	spark->fxObj = objNum;
	spark->blendMode = BlendMode::Additive;
	spark->flags = SP_ITEM | SP_NODEATTACH | SP_SCALE | SP_DEF;
	spark->nodeNumber = node;
	spark->x = x;
	spark->z = z;
	spark->y = 0;

	if (glow)
	{
		spark->xVel = 0;
		spark->yVel = 0;
		spark->zVel = 0;
	}
	else
	{
		spark->xVel = (GetRandomControl() & 0x1F) - 0x0F;
		spark->yVel = (GetRandomControl() & 0x1F) - 0x0F;
		spark->zVel = (GetRandomControl() & 0x1F) - 0x0F;
		spark->y = (GetRandomControl() & 0x3F) - 0x1F;
	}

	spark->friction = 51;
	spark->maxYvel = 0;
	spark->gravity = 0;

	if (glow)
	{
		spark->scalar = 1;
		spark->SpriteSeqID = ID_DEFAULT_SPRITES;
		spark->SpriteID = SPR_LENS_FLARE_LIGHT;
		spark->size = spark->sSize = (GetRandomControl() & 0x1F) + 160;
	}
	else
	{
		spark->scalar = 0;
		spark->SpriteSeqID = ID_DEFAULT_SPRITES;
		spark->SpriteID = SPR_UNDERWATERDUST;
		spark->size = spark->sSize = (GetRandomControl() & 7) + 8;
	}

	spark->dSize = spark->size / 2;
}

void TriggerElectricitySparks(ItemInfo* item, int joint, int flame)
{
	auto* spark = GetFreeParticle();

	auto pos = GetJointPosition(item, joint);
	spark->on = 1;
	spark->dR = 0;
	spark->colFadeSpeed = 8;
	byte color = (GetRandomControl() & 0x3F) - 64;
	spark->sR = color;
	spark->sB = color;
	spark->sG = color;
	spark->dB = color;
	spark->dG = color / 2;
	spark->blendMode = BlendMode::Additive;
	spark->fadeToBlack = 4;
	spark->life = 12;
	spark->sLife = 12;
	spark->x = pos.x;
	spark->y = pos.y;
	spark->z = pos.z;
	spark->xVel = 2 * (GetRandomControl() & 0x1FF) - 512;
	spark->yVel = 2 * (GetRandomControl() & 0x1FF) - 512;
	spark->zVel = 2 * (GetRandomControl() & 0x1FF) - 512;
	spark->friction = 51;
	spark->maxYvel = 0;
	spark->gravity = 0;
	spark->flags = SP_NONE;

	if (flame)
		TriggerFireFlame(pos.x, pos.y, pos.z, FlameType::SmallFast, Vector3(0.2f, 0.5f, 1.0f), Vector3(0.2f, 0.8f, 1.0f));
}

bool ElectricityWireCheckDeadlyBounds(Vector3i* pos, short delta)
{
	if ((pos->x + delta) >= DeadlyBounds.X1 && (pos->x - delta) <= DeadlyBounds.X2 &&
		(pos->y + delta) >= DeadlyBounds.Y1 && (pos->y - delta) <= DeadlyBounds.Y2 &&
		(pos->z + delta) >= DeadlyBounds.Z1 && (pos->z - delta) <= DeadlyBounds.Z2)
	{
		return true;
	}

	return false;
}

void ElectricityWiresControl(short itemNumber)
{
	auto* item = &g_Level.Items[itemNumber];

	AnimateItem(*item);

	if (!TriggerActive(item))
		return;

	SoundEffect(SFX_TR5_ELECTRIC_WIRES, &item->Pose);

	auto* object = &Objects[item->ObjectNumber];

	auto cableBox = GameBoundingBox(item).ToBoundingOrientedBox(item->Pose);
	auto cableBottomPlane = cableBox.Center.y + cableBox.Extents.y - CLICK(1);

	int currentEndNode = 0;
	int flashingNode = GlobalCounter % 3;

	for (int i = 0; i < object->nmeshes; i++)
	{
		auto pos = GetJointPosition(item, i, Vector3i(0, 0, CLICK(1)));

		if (pos.y < cableBottomPlane)
			continue;

		if (((GetRandomControl() & 0x0F) < 8) && flashingNode == currentEndNode)
			SpawnDynamicLight(pos.x, pos.y, pos.z, 10, 0, ((GetRandomControl() & 0x3F) + 96) / 2, (GetRandomControl() & 0x3F) + 128);

		for (int s = 0; s < 3; s++)
		{
			if (GetRandomControl() & 1)
			{
				int x = (GetRandomControl() & 0x3F) - 0x1F;
				int z = (GetRandomControl() & 0x3F) - 0x1F;
				TriggerElectricityWireSparks(x, z, itemNumber, s + 2, false);
				TriggerElectricityWireSparks(x, z, itemNumber, s + 2, true);
			}
		}

		currentEndNode++;
	}

	if (GetRandomControl() & 1)
		return;

	auto collObjects = GetCollidedObjects(*item, true, false, BLOCK(2), ObjectCollectionMode::Items);
	for (auto* itemPtr : collObjects.Items)
	{
		const auto& object = Objects[itemPtr->ObjectNumber];

		if (itemPtr->ObjectNumber != ID_LARA && !object.intelligent)
			continue;

		bool isWaterNearby = false;
		auto npcBox = GameBoundingBox(itemPtr).ToBoundingOrientedBox(itemPtr->Pose);

		for (int i = 0; i < object.nmeshes; i++)
		{
			auto pos = GetJointPosition(item, i, Vector3i(0, 0, CLICK(1)));
			short roomNumber = item->RoomNumber;
			auto floor = GetFloor(pos.x, pos.y, pos.z, &roomNumber);

			bool isTouchingWater = false;
			if (TestEnvironment(ENV_FLAG_WATER, roomNumber))
			{
				isTouchingWater = true;

				if ((GetRandomControl() & 127) < 16)
				{
					SpawnRipple(
						Vector3(pos.x, floor->GetSurfaceHeight(pos, true), pos.z),
						roomNumber,
						Random::GenerateFloat(32.0f, 40.0f),
						(int)RippleFlags::LowOpacity);
				}
			}

			if (pos.y < cableBottomPlane)
				continue;

			for (int j = 0; j < object.nmeshes; j++)
			{
				auto collPos = GetJointPosition(itemPtr, j);
				auto pointCollJointRoom = GetPointCollision(collPos, itemPtr->RoomNumber).GetRoomNumber();

				if (!isWaterNearby && isTouchingWater && roomNumber == pointCollJointRoom)
					isWaterNearby = true;
			}

			bool instantKill = BoundingSphere(Vector3(pos.x, pos.y, pos.z), BLOCK(0.25f)).Intersects(npcBox);

			if (isWaterNearby || instantKill)
			{
				if (!isWaterNearby)
				{
					if (itemPtr->Effect.Type != EffectType::Smoke)
					{
						ItemBlueElectricBurn(itemPtr, 2 *FPS);
					}
					else
					{
						ItemSmoke(itemPtr, -1);
					}
				}

				if (instantKill)
				{
					DoDamage(itemPtr, INT_MAX);
				}
				else
				{
					DoDamage(itemPtr, 8);
				}

				for (int j = 0; j < object.nmeshes; j++)
				{
					if ((GetRandomControl() & 127) < 16)
						TriggerElectricitySparks(itemPtr, j, false);
				}

				SpawnDynamicLight(
					itemPtr->Pose.Position.x,
					itemPtr->Pose.Position.y,
					itemPtr->Pose.Position.z,
					5,
					0,
					(GetRandomControl() & 0x3F) + 0x2F,
					(GetRandomControl() & 0x3F) + 0x4F);

				break;
			}
		}
	}
}
