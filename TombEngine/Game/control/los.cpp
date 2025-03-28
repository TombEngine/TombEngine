#include "framework.h"
#include "Game/control/los.h"

#include "Game/animation.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Los.h"
#include "Game/collision/Point.h"
#include "Game/collision/Sphere.h"
#include "Game/effects/tomb4fx.h"
#include "Game/effects/debris.h"
#include "Game/items.h"
#include "Game/Lara/lara_fire.h"
#include "Game/Lara/lara_one_gun.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Objects/Generic/Object/objects.h"
#include "Objects/Generic/Switches/switch.h"
#include "Renderer/Renderer.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Include/Objects/ScriptInterfaceObjectsHandler.h"
#include "Scripting/Include/ScriptInterfaceGame.h"
#include "Sound/sound.h"
#include "Specific/Input/Input.h"
#include "Specific/trutils.h"

using namespace TEN::Collision::Los;
using namespace TEN::Collision::Point;
using namespace TEN::Collision::Sphere;
using namespace TEN::Math;
using namespace TEN::Utils;
using TEN::Renderer::g_Renderer;

// Globals
auto LosRoomNumbers = std::vector<int>{};
int ClosestItem;
int ClosestDist;
Vector3i ClosestCoord;

// Deprecated.
bool LOS(const GameVector* origin, GameVector* target)
{
	// FAILSAFE.
	if (origin->ToVector3i() == target->ToVector3i())
	{
		LosRoomNumbers.clear();
		LosRoomNumbers.push_back(origin->RoomNumber);
		return true;
	}

	auto dir = target->ToVector3() - origin->ToVector3();
	dir.Normalize();
	float dist = Vector3::Distance(origin->ToVector3(), target->ToVector3());

	auto roomLosColl = GetRoomLosCollision(origin->ToVector3(), origin->RoomNumber, dir, dist);
	if (roomLosColl.IsIntersected)
		*target = GameVector(roomLosColl.Position, roomLosColl.RoomNumber);

	// HACK: Transplant LOS room numbers to legacy global.
	LosRoomNumbers.clear();
	LosRoomNumbers.insert(LosRoomNumbers.end(), roomLosColl.RoomNumbers.begin(), roomLosColl.RoomNumbers.end());

	return !roomLosColl.IsIntersected;
}

// Deprecated.
bool LOSAndReturnTarget(GameVector* origin, GameVector* target, int push)
{
	int x = origin->x;
	int y = origin->y;
	int z = origin->z;
	short roomNumber = origin->RoomNumber;
	short roomNumber2 = roomNumber;
	int dx = (target->x - x) >> 3;
	int dy = (target->y - y) >> 3;
	int dz = (target->z - z) >> 3;
	bool flag = false;
	bool result = false;

	int i;
	for (i = 0; i < 8; ++i)
	{
		roomNumber2 = roomNumber;
		auto* floor = GetFloor(x, y, z, &roomNumber);

		if (g_Level.Rooms[roomNumber2].flags & ENV_FLAG_SWAMP)
		{
			flag = true;
			break;
		}

		int floorHeight = GetFloorHeight(floor, x, y, z);
		int ceilingHeight = GetCeiling(floor, x, y, z);
		if (floorHeight != NO_HEIGHT && ceilingHeight != NO_HEIGHT && ceilingHeight < floorHeight)
		{
			if (y > floorHeight)
			{
				if (y - floorHeight >= push)
				{
					flag = true;
					break;
				}

				y = floorHeight;
			}

			if (y < ceilingHeight)
			{
				if (ceilingHeight - y >= push)
				{
					flag = true;
					break;
				}

				y = ceilingHeight;
			}

			result = true;
		}
		else if (result)
		{
			flag = true;
			break;
		}

		x += dx;
		y += dy;
		z += dz;
	}

	if (i)
	{
		x -= dx;
		y -= dy;
		z -= dz;
	}

	GetFloor(x, y, z, &roomNumber2);
	target->x = x;
	target->y = y;
	target->z = z;
	target->RoomNumber = roomNumber2;

	return !flag;
}

bool GetTargetOnLOS(GameVector* origin, GameVector* target, bool drawTarget, bool isFiring)
{
	auto dir = target->ToVector3() - origin->ToVector3();
	dir.Normalize();

	auto target2 = *target;
	int result = LOS(origin, &target2);

	GetFloor(target2.x, target2.y, target2.z, &target2.RoomNumber);

	if (isFiring && Lara.Control.Look.IsUsingLasersight)
	{
		Lara.Control.Weapon.HasFired = true;
		Lara.Control.Weapon.Fired = true;
		Lara.RightArm.GunFlash = Weapons[(int)Lara.Control.Weapon.GunType].FlashTime;

		if (Lara.Control.Weapon.GunType == LaraWeaponType::Revolver)
			SoundEffect(SFX_TR4_REVOLVER_FIRE, nullptr);
	}

	bool hitProcessed = false;

	MESH_INFO* mesh = nullptr;
	auto vector = Vector3i::Zero;
	int itemNumber = ObjectOnLOS2(origin, target, &vector, &mesh);
	bool hasHit = (itemNumber != NO_LOS_ITEM);

	if (hasHit)
	{
		target2.x = vector.x - ((vector.x - origin->x) >> 5);
		target2.y = vector.y - ((vector.y - origin->y) >> 5);
		target2.z = vector.z - ((vector.z - origin->z) >> 5);

		GetFloor(target2.x, target2.y, target2.z, &target2.RoomNumber);

		if (isFiring)
		{
			if (Lara.Control.Weapon.GunType != LaraWeaponType::Crossbow)
			{
				if (itemNumber < 0)
				{
					if (Statics[mesh->staticNumber].shatterType != ShatterType::None)
					{
						const auto& weapon = Weapons[(int)Lara.Control.Weapon.GunType];
						mesh->HitPoints -= weapon.Damage;
						ShatterImpactData.impactDirection = dir;
						ShatterImpactData.impactLocation = Vector3(mesh->pos.Position.x, mesh->pos.Position.y, mesh->pos.Position.z);
						ShatterObject(nullptr, mesh, 128, target2.RoomNumber, 0);
						SoundEffect(GetShatterSound(mesh->staticNumber), (Pose*)mesh);
						hitProcessed = true;
					}

					TriggerRicochetSpark(target2, LaraItem->Pose.Orientation.y);
				}
				else
				{
					auto* item = &g_Level.Items[itemNumber];

					if (item->ObjectNumber < ID_SHOOT_SWITCH1 || item->ObjectNumber > ID_SHOOT_SWITCH4)
					{
						if ((Objects[item->ObjectNumber].explodableMeshbits & ShatterItem.bit) &&
							Lara.Control.Look.IsUsingLasersight)
						{
								item->MeshBits &= ~ShatterItem.bit;
								ShatterImpactData.impactDirection = dir;
								ShatterImpactData.impactLocation = ShatterItem.sphere.Center;
								ShatterObject(&ShatterItem, 0, 128, target2.RoomNumber, 0);
								TriggerRicochetSpark(target2, LaraItem->Pose.Orientation.y, false);
								hitProcessed = true;
						}
						else
						{
							auto* object = &Objects[item->ObjectNumber];

							if (drawTarget && (Lara.Control.Weapon.GunType == LaraWeaponType::Revolver ||
											   Lara.Control.Weapon.GunType == LaraWeaponType::HK))
							{
								if (object->intelligent || object->HitRoutine)
								{
									const auto& weapon = Weapons[(int)Lara.Control.Weapon.GunType];

									auto spheres = item->GetSpheres();
									auto ray = Ray(origin->ToVector3(), dir);
									float bestDistance = INFINITY;
									int bestJointIndex = NO_VALUE;

									for (int i = 0; i < spheres.size(); i++)
									{
										float dist = 0.0f;
										if (ray.Intersects(spheres[i], dist))
										{
											if (dist < bestDistance)
											{
												bestDistance = dist;
												bestJointIndex = i;
											}
										}
									}

									HitTarget(LaraItem, item, &target2, Weapons[(int)Lara.Control.Weapon.GunType].AlternateDamage, false, bestJointIndex);
									hitProcessed = true;
								}
								else
								{
									// TR5
									if (object->hitEffect == HitEffect::Richochet)
										TriggerRicochetSpark(target2, LaraItem->Pose.Orientation.y);
								}
							}
							else if (item->ObjectNumber >= ID_SMASH_OBJECT1 && item->ObjectNumber <= ID_SMASH_OBJECT8)
							{
								SmashObject(itemNumber);
								hitProcessed = true;
							}
						}
					}
					else
					{
						if (ShatterItem.bit == 1 << (Objects[item->ObjectNumber].nmeshes - 1))
						{
							if (!(item->Flags & 0x40))
							{
								if (item->ObjectNumber == ID_SHOOT_SWITCH1)
									ExplodeItemNode(item, Objects[item->ObjectNumber].nmeshes - 1, 0, 64);

								if (item->TriggerFlags == 444 && item->ObjectNumber == ID_SHOOT_SWITCH2)
								{
									// TR5 ID_SWITCH_TYPE_8/ID_SHOOT_SWITCH2
									ProcessExplodingSwitchType8(item);
								}
								else
								{
									/*if (item->objectNumber == ID_SHOOT_SWITCH3)
									{
									// TR4 ID_SWITCH_TYPE7
									ExplodeItemNode(item, Objects[item->objectNumber].nmeshes - 1, 0, 64);
									}*/

									if (item->Flags & IFLAG_ACTIVATION_MASK &&
										(item->Flags & IFLAG_ACTIVATION_MASK) != IFLAG_ACTIVATION_MASK)
									{
										TestTriggers(item->Pose.Position.x, item->Pose.Position.y - 256, item->Pose.Position.z, item->RoomNumber, true, item->Flags & IFLAG_ACTIVATION_MASK);
									}
									else
									{
										short triggerItems[8];
										for (int count = GetSwitchTrigger(item, triggerItems, 1); count > 0; --count)
										{
											AddActiveItem(triggerItems[count - 1]);
											g_Level.Items[triggerItems[count - 1]].Status = ITEM_ACTIVE;
											g_Level.Items[triggerItems[count - 1]].Flags |= IFLAG_ACTIVATION_MASK;
										}
									}
								}
							}

							if (item->Status != ITEM_DEACTIVATED)
							{
								AddActiveItem(itemNumber);
								item->Status = ITEM_ACTIVE;
								item->Flags |= IFLAG_ACTIVATION_MASK | 0x40;
							}

							hitProcessed = true;
						}

						TriggerRicochetSpark(target2, LaraItem->Pose.Orientation.y);
					}
				}
			}
			else
			{
				if (Lara.Control.Look.IsUsingLasersight && isFiring)
					FireCrossBowFromLaserSight(*LaraItem, origin, &target2);
			}
		}
	}
	else
	{
		if (Lara.Control.Weapon.GunType == LaraWeaponType::Crossbow)
		{
			if (isFiring && Lara.Control.Look.IsUsingLasersight)
				FireCrossBowFromLaserSight(*LaraItem, origin, &target2);
		}
		else
		{
			target2.x -= (target2.x - origin->x) >> 5;
			target2.y -= (target2.y - origin->y) >> 5;
			target2.z -= (target2.z - origin->z) >> 5;

			if (isFiring && !result)
				TriggerRicochetSpark(target2, LaraItem->Pose.Orientation.y);
		}
	}

	return hitProcessed;
}

static bool DoRayBox(const GameVector& origin, const GameVector& target, const GameBoundingBox& bounds,
	const Pose& objectPose, Vector3i& hitPos, int closestItemNumber)
{
	auto box = bounds.ToBoundingOrientedBox(objectPose);

	auto rayOrigin = origin.ToVector3();
	auto rayDir = (target - origin).ToVector3();
	rayDir.Normalize();

	// Don't test spheres if no intersection.
	float dist = 0.0f;
	if (rayDir == Vector3::Zero || !box.Intersects(rayOrigin, rayDir, dist))
		return false;

	// Get raw collision point.
	auto collidedPoint = Geometry::TranslatePoint(rayOrigin, rayDir, dist);
	hitPos.x = collidedPoint.x - objectPose.Position.x;
	hitPos.y = collidedPoint.y - objectPose.Position.y;
	hitPos.z = collidedPoint.z - objectPose.Position.z;

	// Test single spheres in case of items.
	int meshIndex = 0;
	int bit = 0;
	int sp = -2;
	float minDist = INFINITY;

	if (closestItemNumber < 0)
	{
		// Static meshes don't require further tests.
		sp = -1;
		minDist = dist;
	}
	else
	{
		// Test spheres instead for items.
		auto* item = &g_Level.Items[closestItemNumber];
		auto* object = &Objects[item->ObjectNumber];

		if (object->nmeshes <= 0)
			return false;

		meshIndex = object->meshIndex;

		auto spheres = item->GetSpheres();
		for (int i = 0; i < object->nmeshes; i++)
		{
			// If mesh is visible.
			if (item->MeshBits & (1 << i))
			{
				const auto& sphere = spheres[i];

				float dist = 0.0f;
				if (sphere.Intersects(rayOrigin, rayDir, dist))
				{
					// Test for minimum distance.
					if (dist < minDist)
					{
						minDist = dist;
						meshIndex = object->meshIndex + i;
						bit = 1 << i;
						sp = i;
					}
				}
			}
		}

		if (sp < -1)
			return false;
	}

	if (dist >= ClosestDist)
		return false;

	// Set up test result.
	ClosestCoord.x = hitPos.x + objectPose.Position.x;
	ClosestCoord.y = hitPos.y + objectPose.Position.y;
	ClosestCoord.z = hitPos.z + objectPose.Position.z;
	ClosestDist = dist;
	ClosestItem = closestItemNumber;

	// If collided object is item, set up shatter item data struct.
	if (sp >= 0)
	{
		auto* item = &g_Level.Items[closestItemNumber];

		auto spheres = item->GetSpheres();

		ShatterItem.yRot = item->Pose.Orientation.y;
		ShatterItem.meshIndex = meshIndex;
		ShatterItem.color = item->Model.Color;
		ShatterItem.sphere.Center = spheres[sp].Center;
		ShatterItem.bit = bit;
		ShatterItem.flags = 0;
	}

	return true;
}

int ObjectOnLOS2(GameVector* origin, GameVector* target, Vector3i* vec, MESH_INFO** staticObj, GAME_OBJECT_ID priorityObjectID)
{
	ClosestItem = NO_LOS_ITEM;
	ClosestDist = SQUARE(target->x - origin->x) + SQUARE(target->y - origin->y) + SQUARE(target->z - origin->z);

	for (int roomNumber : LosRoomNumbers)
	{
		auto& room = g_Level.Rooms[roomNumber];

		auto pose = Pose::Zero;

		if (staticObj != nullptr)
		{
			for (int m = 0; m < room.mesh.size(); m++)
			{
				auto& meshp = room.mesh[m];

				if (meshp.flags & StaticMeshFlags::SM_VISIBLE)
				{
					auto bounds = GetBoundsAccurate(meshp, false);
					pose = Pose(meshp.pos.Position, EulerAngles(0, meshp.pos.Orientation.y, 0));

					if (DoRayBox(*origin, *target, bounds, pose, *vec, -1 - meshp.staticNumber))
					{
						*staticObj = &meshp;
						target->RoomNumber = roomNumber;
					}
				}
			}
		}

		for (int linkNumber = room.itemNumber; linkNumber != NO_VALUE; linkNumber = g_Level.Items[linkNumber].NextItem)
		{
			const auto& item = g_Level.Items[linkNumber];

			if (item.Status == ITEM_DEACTIVATED || item.Status == ITEM_INVISIBLE)
				continue;

			if (priorityObjectID != GAME_OBJECT_ID::ID_NO_OBJECT && item.ObjectNumber != priorityObjectID)
				continue;

			if (item.ObjectNumber != ID_LARA && (Objects[item.ObjectNumber].collision == nullptr || !item.Collidable))
				continue;

			if (item.ObjectNumber == ID_LARA && priorityObjectID != ID_LARA)
				continue;

			auto bounds = GameBoundingBox(&item);
			pose = Pose(item.Pose.Position, EulerAngles(0, item.Pose.Orientation.y, 0));

			if (DoRayBox(*origin, *target, bounds, pose, *vec, linkNumber))
				target->RoomNumber = roomNumber;
		}
	}

	vec->x = ClosestCoord.x;
	vec->y = ClosestCoord.y;
	vec->z = ClosestCoord.z;

	return ClosestItem;
}
