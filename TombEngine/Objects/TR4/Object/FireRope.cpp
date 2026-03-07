#include "framework.h"
#include "Objects/TR4/Object/FireRope.h"

#include "Game/collision/collide_item.h"
#include "Game/control/trigger.h"
#include "Game/effects/debris.h"
#include "Game/effects/tomb4fx.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Setup.h"
#include "Specific/level.h"

constexpr auto FIRE_RADIUS = 64;
constexpr auto FIRE_OFFSET = Vector3i(-24, 48, 192);
constexpr auto FIRE_TIMEOUT = 5 * FPS;

enum FireRopeItemFlags
{
	LowerMesh = 0,
	UpperMesh = 1,
	StartMesh = 2,
	Timeout = 3
};

void TriggerRopeFlame(Vector3i pos)
{
	auto* spark = GetFreeParticle();

	spark->on = true;

	spark->sR = 255;
	spark->sG = (GetRandomControl() & 0x1F) + 48;
	spark->sB = 48;
	spark->dR = (GetRandomControl() & 0x3F) - 64;
	spark->dG = (GetRandomControl() & 0x3F) + 0x80;
	spark->dB = 32;

	spark->fadeToBlack = 4;
	spark->colFadeSpeed = (GetRandomControl() & 3) + 4;
	spark->blendMode = BlendMode::Additive;

	spark->life = spark->sLife = (GetRandomControl() & 3) + 24;

	spark->x = (GetRandomControl() & 0xF) + pos.x - 8;
	spark->y = pos.y;
	spark->z = (GetRandomControl() & 0xF) + pos.z - 8;

	spark->xVel = (GetRandomControl() & 0xFF) - 128;
	spark->zVel = (GetRandomControl() & 0xFF) - 128;

	spark->friction = 5;
	spark->flags = SP_SCALE | SP_DEF | SP_ROTATE | SP_EXPDEF;

	if (!(GetRandomControl() & 3))
		spark->flags |= SP_EXPLOSION;

	spark->rotAng = GetRandomControl() & 0xFFF;

	if (GetRandomControl() & 1)
		spark->rotAdd = -16 - (GetRandomControl() & 0xF);
	else
		spark->rotAdd = (GetRandomControl() & 0xF) + 16;

	if (GetRandomControl() & 0xF)
	{
		spark->yVel = -24 - (GetRandomControl() & 0xF);
		spark->gravity = -24 - (GetRandomControl() & 0x1F);
		spark->maxYvel = -16 - (GetRandomControl() & 7);
	}
	else
	{
		spark->yVel = (GetRandomControl() & 0xF) + 24;
		spark->gravity = (GetRandomControl() & 0x1F) + 24;
		spark->maxYvel = 0;
	}

	spark->scalar = 2;
	spark->size = (GetRandomControl() & 0xF) + 96;
	spark->sSize = spark->size;
	spark->dSize = spark->size / 8.0f;
}

void InitializeFireRope(short itemNumber)
{
	auto& item = g_Level.Items[itemNumber];

	item.ItemFlags[FireRopeItemFlags::LowerMesh] =
	item.ItemFlags[FireRopeItemFlags::UpperMesh] =
	item.ItemFlags[FireRopeItemFlags::StartMesh] = 0;

	item.ItemFlags[FireRopeItemFlags::Timeout] = item.TriggerFlags > 0 ? (item.TriggerFlags * FPS) : FIRE_TIMEOUT;
}

void FireRopeControl(short itemNumber)
{
	auto& item = g_Level.Items[itemNumber];

	if (!TriggerActive(&item))
		return;

	int meshCount = Objects[item.ObjectNumber].nmeshes - 1;

	if (!(GlobalCounter & 3))
	{
		auto spheres = item.GetSpheres();
		int prevMeshIndex = item.ItemFlags[FireRopeItemFlags::LowerMesh];

		for (int i = item.ItemFlags[FireRopeItemFlags::LowerMesh]; i < item.ItemFlags[FireRopeItemFlags::UpperMesh]; i++)
		{
			bool oddMesh = (i & 1);

			if (item.MeshBits.Test(i))
			{
				if (oddMesh && prevMeshIndex < spheres.size() && GlobalCounter & 4)
				{
					auto pos = Vector3((spheres[prevMeshIndex].Center + spheres[i].Center) / 2);
					TriggerRopeFlame(pos);
				}
				else if (!(GlobalCounter & 4))
				{
					TriggerRopeFlame(Vector3i(spheres[i].Center));
				}
			}

			if (oddMesh)
				prevMeshIndex++;
		}

		if (item.ItemFlags[FireRopeItemFlags::Timeout])
		{
			if (item.ItemFlags[FireRopeItemFlags::LowerMesh] > 0)
				item.ItemFlags[FireRopeItemFlags::LowerMesh]--;

			if (item.ItemFlags[FireRopeItemFlags::UpperMesh] < meshCount)
				item.ItemFlags[FireRopeItemFlags::UpperMesh]++;
		}
	}

	if (item.ItemFlags[FireRopeItemFlags::Timeout])
	{
		item.ItemFlags[FireRopeItemFlags::Timeout]--;

		if (!item.ItemFlags[FireRopeItemFlags::Timeout])
		{
			item.ItemFlags[FireRopeItemFlags::UpperMesh] =
			item.ItemFlags[FireRopeItemFlags::LowerMesh] = item.ItemFlags[FireRopeItemFlags::StartMesh];

			ExplodeItemNode(&item, item.ItemFlags[FireRopeItemFlags::StartMesh], 0, BODY_DO_EXPLOSION);
		}
	}
	else if (GlobalCounter & 1)
	{
		int passes = 0;

		if (item.ItemFlags[FireRopeItemFlags::LowerMesh] > 0)
		{
			item.ItemFlags[FireRopeItemFlags::LowerMesh]--;
			ExplodeItemNode(&item, item.ItemFlags[FireRopeItemFlags::LowerMesh], 0, BODY_DO_EXPLOSION);
		}
		else
			passes++;

		if (item.ItemFlags[FireRopeItemFlags::UpperMesh] < meshCount)
		{
			item.ItemFlags[FireRopeItemFlags::UpperMesh]++;
			ExplodeItemNode(&item, item.ItemFlags[FireRopeItemFlags::UpperMesh], 0, BODY_DO_EXPLOSION);
		}
		else
			passes++;

		if (passes == 2)
		{
			TestTriggers(item.Pose.Position.x, item.Pose.Position.y, item.Pose.Position.z, item.RoomNumber, true);
			KillItem(itemNumber);
		}
	}
}

void FireRopeCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
{
	auto& item = g_Level.Items[itemNumber];
	auto lara = GetLaraInfo(laraItem);

	if (item.Active || !lara->Torch.IsLit)
		return;

	if (Vector3i::Distance(item.Pose.Position, laraItem->Pose.Position) > BLOCK(4))
		return;

	auto jointPos = GetJointPosition(laraItem, LM_LHAND, FIRE_OFFSET).ToVector3();
	auto flameSphere = BoundingSphere(jointPos, FIRE_RADIUS);
	auto spheres = item.GetSpheres();

	for (int i = 0; i < spheres.size(); i++)
	{
		auto& sphere = spheres[i];

		if (!sphere.Intersects(flameSphere))
			continue;

		item.ItemFlags[FireRopeItemFlags::LowerMesh] =
		item.ItemFlags[FireRopeItemFlags::UpperMesh] =
		item.ItemFlags[FireRopeItemFlags::StartMesh] = i;

		AddActiveItem(itemNumber);
		item.Status = ITEM_ACTIVE;
		item.Flags |= IFLAG_ACTIVATION_MASK;
	}
}