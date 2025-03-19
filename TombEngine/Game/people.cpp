#include "framework.h"
#include "Game/people.h"

#include "Game/Animation/Animation.h"
#include "Game/control/los.h"
#include "Game/effects/effects.h"
#include "Game/effects/debris.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Sound/sound.h"

using namespace TEN::Animation;

bool ShotLara(ItemInfo* item, AI_INFO* AI, const CreatureBiteInfo& gun, short extraRotation, int damage)
{
	auto* creature = GetCreatureInfo(item);
	auto* enemy = creature->Enemy;

	bool hasHit = false;
	bool isTargetable = false;

	if (AI->distance <= SQUARE(MAX_VISIBILITY_DISTANCE) && Targetable(item, AI))
	{
		int distance = phd_sin(AI->enemyFacing) * enemy->Animation.Velocity.z * pow(MAX_VISIBILITY_DISTANCE, 2) / 300;
		distance = SQUARE(distance) + AI->distance;
		if (distance <= SQUARE(MAX_VISIBILITY_DISTANCE))
		{
			int random = (SQUARE(MAX_VISIBILITY_DISTANCE) - AI->distance) / (SQUARE(MAX_VISIBILITY_DISTANCE) / 0x5000) + 8192;
			hasHit = GetRandomControl() < random;
		}
		else
		{
			hasHit = false;
		}
		
		isTargetable = true;
	}
	else
	{
		hasHit = false;
		isTargetable = false;
	}

	if (damage)
	{
		if (enemy->IsLara())
		{
			if (hasHit)
			{
				DoDamage(enemy, damage);
				CreatureEffect(item, gun, &GunHit);
			}
			else if (isTargetable)
				CreatureEffect(item, gun, &GunMiss);
		}
		else
		{
			CreatureEffect(item, gun, &GunShot);
			if (hasHit)
			{
				enemy->HitStatus = true;
				enemy->HitPoints += damage / -10;

				int random = GetRandomControl() & 0xF;
				if (random > 14)
					random = 0;

				auto pos = GetJointPosition(enemy, random);
				DoBloodSplat(pos.x, pos.y, pos.z, (GetRandomControl() & 3) + 4, enemy->Pose.Orientation.y, enemy->RoomNumber);
			}
		}
	}

	// TODO: smash objects

	return isTargetable;
}

short GunMiss(int x, int y, int z, short velocity, short yRot, short roomNumber)
{
	// TODO: Remove -128 and fix ricochet effect going on floor. -- TokyoSU 2023.04.28
	auto pos = GameVector(
		LaraItem->Pose.Position.x + ((GetRandomControl() - 0x4000) << 9) / 0x7FFF,
		LaraItem->Floor - 128,
		LaraItem->Pose.Position.z + ((GetRandomControl() - 0x4000) << 9) / 0x7FFF,
		LaraItem->RoomNumber);

	Ricochet(Pose(pos.ToVector3i()));
	return GunShot(x, y, z, velocity, yRot, roomNumber);
}

short GunHit(int x, int y, int z, short velocity, short yRot, short roomNumber)
{
	auto pos = GetJointPosition(LaraItem, (25 * GetRandomControl()) >> 15);
	DoBloodSplat(pos.x, pos.y, pos.z, (GetRandomControl() & 3) + 3, LaraItem->Pose.Orientation.y, LaraItem->RoomNumber);
	return GunShot(x, y, z, velocity, yRot, roomNumber);
}

short GunShot(int x, int y, int z, short velocity, short yRot, short roomNumber)
{
	return -1;
}

bool Targetable(ItemInfo* item, AI_INFO* ai)
{
	// Discard it entity is not a creature (only creatures can use Targetable())
	// or if the target is not visible.
	if (!item->IsCreature() || !ai->ahead || ai->distance >= SQUARE(MAX_VISIBILITY_DISTANCE))
		return false;

	auto* creature = GetCreatureInfo(item);
	auto* enemy = creature->Enemy;

	if (creature->Enemy == nullptr)
		return false;

	// Only Lara or a creature may be targeted.
	if ((!enemy->IsCreature() && !enemy->IsLara()) || enemy->HitPoints <= 0)
		return false;

	const auto& bounds = GetClosestKeyframe(*item).BoundingBox;
	const auto& boundsTarget = GetClosestKeyframe(*enemy).BoundingBox;

	auto origin = GameVector(
		item->Pose.Position.x,
		(item->ObjectNumber == ID_SNIPER) ? (item->Pose.Position.y - CLICK(3)) : (item->Pose.Position.y + ((bounds.Y2 + 3 * bounds.Y1) / 4)),
		item->Pose.Position.z,
		item->RoomNumber);
	auto target = GameVector(
		enemy->Pose.Position.x,
		enemy->Pose.Position.y + ((boundsTarget.Y2 + 3 * boundsTarget.Y1) / 4),
		enemy->Pose.Position.z,
		enemy->RoomNumber); // TODO: Check why this line didn't exist in the first place. -- TokyoSU 2022.08.05

	MESH_INFO* mesh = nullptr;
	Vector3i vector = {};
	int losItemIndex = ObjectOnLOS2(&origin, &target, &vector, &mesh);
	if (losItemIndex == item->Index)
		losItemIndex = NO_LOS_ITEM; // Don't find itself

	return (LOS(&origin, &target) && losItemIndex == NO_LOS_ITEM && mesh == nullptr);
}

bool TargetVisible(ItemInfo* item, AI_INFO* ai, float maxAngleInDegrees)
{
	if (!item->IsCreature() || ai->distance >= SQUARE(MAX_VISIBILITY_DISTANCE))
		return false;

	// Check just in case.
	auto* creature = GetCreatureInfo(item);
	if (creature == nullptr)
		return false;

	auto* enemy = creature->Enemy;
	if (enemy == nullptr || enemy->HitPoints == 0)
		return false;

	short angle = ai->angle - creature->JointRotation[2];
	if (angle > ANGLE(-maxAngleInDegrees) && angle < ANGLE(maxAngleInDegrees))
	{
		const auto& bounds = GetClosestKeyframe(*enemy).BoundingBox;

		auto origin = GameVector(
			item->Pose.Position.x,
			item->Pose.Position.y - CLICK(3),
			item->Pose.Position.z,
			item->RoomNumber);
		auto target = GameVector(
			enemy->Pose.Position.x,
			enemy->Pose.Position.y + ((((bounds.Y1 * 2) + bounds.Y1) + bounds.Y2) / 4),
			enemy->Pose.Position.z);

		return LOS(&origin, &target);
	}

	return false;
}
