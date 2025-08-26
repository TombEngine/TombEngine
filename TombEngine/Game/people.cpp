#include "framework.h"
#include "Game/people.h"

#include "Game/animation.h"
#include "Game/collision/Point.h"
#include "Game/control/los.h"
#include "Game/effects/effects.h"
#include "Game/effects/debris.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Sound/sound.h"

using namespace TEN::Collision::Point;

bool ShotLara(ItemInfo* item, AI_INFO* AI, const CreatureBiteInfo& gun, short extraRotation, int damage)
{
	auto* creature = GetCreatureInfo(item);
	auto* enemy = creature->Enemy;

	if (enemy == nullptr)
		return false;

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
	auto pos = GameVector(
		LaraItem->Pose.Position.x + ((GetRandomControl() - 0x4000) << 9) / 0x7FFF,
		LaraItem->Floor - 128,
		LaraItem->Pose.Position.z + ((GetRandomControl() - 0x4000) << 9) / 0x7FFF,
		LaraItem->RoomNumber);

	pos.y = GetPointCollision(pos.ToVector3i(), pos.RoomNumber).GetFloorHeight();

	Ricochet(Pose(pos.ToVector3i()));
	SpawnDecal(pos.ToVector3(), pos.RoomNumber, DecalType::BulletHole);
	return GunShot(x, y, z, velocity, yRot, roomNumber);
}

short GunHit(int x, int y, int z, short velocity, short yRot, short roomNumber)
{
	auto pos = GetJointPosition(LaraItem, Random::GenerateInt(0, NUM_LARA_MESHES - 1));
	DoBloodSplat(pos.x, pos.y, pos.z, (GetRandomControl() & 3) + 3, LaraItem->Pose.Orientation.y, LaraItem->RoomNumber);
	return GunShot(x, y, z, velocity, yRot, roomNumber);
}

short GunShot(int x, int y, int z, short velocity, short yRot, short roomNumber)
{
	return -1;
}

bool Targetable(ItemInfo* item, AI_INFO* ai)
{
	// Discard it entity is not a creature (only creatures can use Targetable()) or if the target is not visible.
	if (!item->IsCreature() || !ai->ahead || ai->distance >= SQUARE(MAX_VISIBILITY_DISTANCE))
		return false;

	auto* creature = GetCreatureInfo(item);
	auto* enemy = creature->Enemy;

	if (creature->Enemy == nullptr)
		return false;

	// Only player or a creature may be targeted.
	if ((!enemy->IsCreature() && !enemy->IsLara()) || enemy->HitPoints <= 0)
		return false;

	const auto& bounds = GetBestFrame(*item).BoundingBox;
	const auto& boundsTarget = GetBestFrame(*enemy).BoundingBox;

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

	StaticMesh* mesh = nullptr;
	Vector3i vector = {};
	int losItemIndex = ObjectOnLOS2(&origin, &target, &vector, &mesh, GAME_OBJECT_ID::ID_NO_OBJECT, item->Index);
	if (losItemIndex == item->Index)
		losItemIndex = NO_LOS_ITEM; // Don't find itself.

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
		const auto& bounds = GetBestFrame(*enemy).BoundingBox;

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

void PerformFinalAttack(ItemInfo& item, const CreatureBiteInfo& bite, int headBoneNumber, int deathAnimNumber, int damage, SOUND_EFFECTS soundID)
{
	auto animNumber = item.Animation.AnimNumber - Objects[item.Animation.AnimObjectID].animIndex;
	if (animNumber != deathAnimNumber)
		return;

	// No more shots left.
	if (item.ItemFlags[FINAL_SHOT_FLAG_INDEX] <= 0)
		return;

	const auto& anim = GetAnimData(item);

	int frameCount = anim.frameEnd - anim.frameBase;
	int frameNumber = item.Animation.FrameNumber - anim.frameBase;

	// Calculate frame range when final attack may occur. It is limited to last third of the animation.
	int frameBase = frameCount - (frameCount / 3);

	// Based on final shot count, calculate interval on which shots will occur.
	int interval = (frameCount / 3) / FINAL_SHOT_COUNT - 1;

	bool doShot = false;

	for (int i = 0; i < FINAL_SHOT_COUNT; i++)
	{
		if (frameNumber == frameBase + interval * i)
		{
			// Decrease shot count.
			item.ItemFlags[FINAL_SHOT_FLAG_INDEX]--;

			doShot = true;
			break;
		}
	}

	if (!doShot)
		return;

	auto* creature = GetCreatureInfo(&item);

	if (creature->Enemy == nullptr || (!creature->Enemy->IsLara() && !creature->Enemy->IsCreature()))
		return;

	AI_INFO AI;
	CreatureAIInfo(&item, &AI);

	if (!Targetable(&item, &AI))
		return;

	if (!AI.ahead || AI.distance > SQUARE(BLOCK(6)) || abs(AI.verticalDistance) > BLOCK(1) || abs(AI.angle) > FINAL_SHOT_CONE_ANGLE)
		return;

	// Since death animation may not end up facing the enemy (e.g. SAS falls on the ground in the opposite direction), perform
	// additional dot product test to make sure that dying entity's head is facing enemy.

	auto origin = GetJointPosition(&item, headBoneNumber);
	auto target = GetJointPosition(&item, headBoneNumber, Vector3::Forward * BLOCK(1));

	auto coneDirection = (origin - target).ToVector3();
	coneDirection.Normalize();

	auto toTarget = (creature->Enemy->Pose.Position - origin).ToVector3();
	toTarget.Normalize();

	if (coneDirection.Dot(toTarget) < TO_RAD(FINAL_SHOT_CONE_ANGLE))
		return; // Enemy head is not within shooting angle.

	ShotLara(&item, &AI, bite, 0, damage);
	SoundEffect(soundID, &item.Pose);
	creature->MuzzleFlash[0].Bite = bite;
	creature->MuzzleFlash[0].Delay = 2;
}