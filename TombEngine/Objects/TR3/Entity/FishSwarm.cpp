#include "framework.h"
#include "Objects/TR3/Entity/FishSwarm.h"

#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/control/box.h"
#include "Game/control/flipeffect.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Objects/TR3/Object/Corpse.h"
#include "Renderer/Renderer.h"
#include "Specific/clock.h"
#include "Specific/level.h"

using namespace TEN::Collision::Point;
using namespace TEN::Entities::TR3;
using namespace TEN::Math;
using namespace TEN::Renderer;

// NOTES:
// HitPoints	= Fish count on spawn.
// ItemFlags[0] = leader item number.
// ItemFlags[1] = target item number.
// ItemFlags[2] = OCB orientation when in patrol mode.
// ItemFlags[3] = Start OCB of AI_FOLLOW. NOTE: Cannot change.
// ItemFlags[4] = Check if target is a corpse.
// ItemFlags[5] = Fish count.
// ItemFlags[6] = Is patrolling.
// ItemFlags[7] = Distance to player.

namespace TEN::Entities::Creatures::TR3
{
	constexpr auto FISH_HARM_DAMAGE				 = 3;
	constexpr auto FISH_VELOCITY_MAX			 = 10.0f;
	constexpr auto FISH_COHESION_FACTOR			 = 100.1f;
	constexpr auto FISH_SPACING_FACTOR			 = 600.0f;
	constexpr auto FISH_CATCH_UP_FACTOR			 = 0.2f;
	constexpr auto FISH_TARGET_DISTANCE_MAX		 = SQUARE(BLOCK(0.01f));
	constexpr auto FISH_BASE_SEPARATION_DISTANCE = 210.0f;
	constexpr auto FISH_UPDATE_INTERVAL_TIME	 = 6;

	std::vector<FishData> FishSwarm = {};

	void InitializeFishSwarm(short itemNumber)
	{
		constexpr auto DEFAULT_FISH_COUNT = 24;

		auto& item = g_Level.Items[itemNumber];

		item.StartPose.Position = item.Pose.Position;
		item.Animation.Velocity.z = Random::GenerateFloat(32.0f, 160.0f);
		item.HitPoints = DEFAULT_FISH_COUNT;
		item.ItemFlags[0] = item.Index;
		item.ItemFlags[1] = item.Index;
		item.ItemFlags[5] = 0;

		if (item.AIBits)
			item.ItemFlags[6] = true;
	}

	static void SpawnFishSwarm(ItemInfo& item)
	{
		constexpr auto VEL_MAX				   = 48.0f;
		constexpr auto VEL_MIN				   = 16.0f;
		constexpr auto START_ORIENT_CONSTRAINT = std::pair<EulerAngles, EulerAngles>(
			EulerAngles(ANGLE(-3.0f), ANGLE(-6.0f), 0),
			EulerAngles(ANGLE(3.0f), ANGLE(6.0f), 0));

		// Create new fish.
		auto& fish = GetNewEffect(FishSwarm, FISH_COUNT_MAX);

		fish.MeshIndex = abs(item.TriggerFlags);
		fish.IsLethal = (item.TriggerFlags < 0) ? true : false;
		fish.IsPatrolling = item.ItemFlags[6];

		fish.Position = item.Pose.Position.ToVector3();
		fish.RoomNumber = item.RoomNumber;
		fish.Orientation.x = Random::GenerateAngle(START_ORIENT_CONSTRAINT.first.x, START_ORIENT_CONSTRAINT.second.x);
		fish.Orientation.y = (item.Pose.Orientation.y + ANGLE(180.0f)) + Random::GenerateAngle(START_ORIENT_CONSTRAINT.first.y, START_ORIENT_CONSTRAINT.second.y);
		fish.Velocity = Random::GenerateFloat(VEL_MIN, VEL_MAX);

		fish.Life = 1.0f;
		fish.Undulation = Random::GenerateFloat(0.0f, PI_MUL_2);

		fish.LeaderItemPtr = &g_Level.Items[item.ItemFlags[0]];
	}

	void ControlFishSwarm(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto& item = g_Level.Items[itemNumber];
		auto& creature = *GetCreatureInfo(&item);
		const auto& playerItem = *LaraItem;

		AI_INFO ai;
		CreatureAIInfo(&item, &ai);

		if (item.HitPoints != NOT_TARGETABLE)
		{
			int fishCount = item.HitPoints - item.ItemFlags[5];

			if (fishCount < 0)
			{
				int fishToTurnOff = -fishCount;
				for (auto& fish : FishSwarm)
				{
					if (fish.LeaderItemPtr == &item && fish.Life > 0.0f)
					{
						fish.Life = 0.0f;
						fishToTurnOff--;
						if (fishToTurnOff == 0)
							break;
					}
				}
			}
			else if (fishCount > 0)
			{
				for (int i = 0; i < fishCount; i++)
					SpawnFishSwarm(item);
			}

			item.ItemFlags[5] = item.HitPoints;
			item.HitPoints = NOT_TARGETABLE;
		}

		int dx = creature.Target.x - item.Pose.Position.x;
		int dz = creature.Target.z - item.Pose.Position.z;
		ai.distance = SQUARE(dx) + SQUARE(dz);

		item.Animation.Velocity.z = FISH_VELOCITY_MAX;

		auto& playerRoom = g_Level.Rooms[playerItem.RoomNumber];

		// Check if corpse is near.
		// TODO: In future also check for other enemies like sharks or crocodile.
		if (!item.ItemFlags[4] && TestGlobalTimeInterval(FISH_UPDATE_INTERVAL_TIME))
		{
			float closestDist = INFINITY;
			for (auto& targetItem : g_Level.Items)
			{
				if (!Objects.CheckID(targetItem.ObjectNumber, true) || targetItem.Index == itemNumber || targetItem.RoomNumber == NO_VALUE)
					continue;

				if (SameZone(&creature, &targetItem) && item.TriggerFlags < 0)
				{
					float dist = Vector3i::Distance(item.Pose.Position, targetItem.Pose.Position);
					if (dist < closestDist &&
						targetItem.ObjectNumber == ID_CORPSE &&
						targetItem.Active && TriggerActive(&targetItem) &&
						targetItem.ItemFlags[7] == (int)CorpseFlag::Grounded &&
						TestEnvironment(ENV_FLAG_WATER, targetItem.RoomNumber))
					{
						item.ItemFlags[4] = 1;
						closestDist = dist;
						item.ItemFlags[1] = targetItem.Index; // Target corpse.
					}
				}
			}
		}

		if (item.ItemFlags[7] < BLOCK(7) && TestEnvironment(ENV_FLAG_WATER, &playerRoom) &&
			item.TriggerFlags < 0 && !item.ItemFlags[4])
		{
			item.ItemFlags[1] = playerItem.Index;
			item.ItemFlags[4] = 0;
			item.ItemFlags[2] = 0;
		}
		// Circle around leader item.
		else if (!item.ItemFlags[4])
		{
			item.ItemFlags[1] = item.ItemFlags[0];
			item.ItemFlags[4] = 0;
		}

		// Follow path.
		if (item.AIBits && !item.ItemFlags[4])
		{
			FindAITargetObject(&creature, ID_AI_FOLLOW, item.ItemFlags[3] + item.ItemFlags[2], false);

			if (creature.AITarget->TriggerFlags == (item.ItemFlags[3] + item.ItemFlags[2]) &&
				creature.AITarget->ObjectNumber == ID_AI_FOLLOW)
			{
				item.ItemFlags[1] = creature.AITarget->Index;
			}
			else
			{
				item.ItemFlags[2] = 0;
			}

			item.ItemFlags[4] = 0;
		}

		for (auto& fish : FishSwarm)
		{
			if (fish.LeaderItemPtr == &item)
			{
				if (fish.Life <= 0.0f)
					continue;

				fish.RoomNumber = item.RoomNumber;
				fish.TargetItemPtr = &g_Level.Items[item.ItemFlags[1]];	
			}
		}
	}

	static Vector3 GetFishStartPosition(const ItemInfo& item)
	{
		constexpr auto BUFFER					= BLOCK(0.1f);
		constexpr auto SPHEROID_SEMI_MAJOR_AXIS = Vector3(BLOCK(2), BLOCK(1), BLOCK(5));

		auto pos = Random::GeneratePointInSpheroid(item.StartPose.Position.ToVector3(), EulerAngles::Identity, SPHEROID_SEMI_MAJOR_AXIS);

		// Get point collision.
		auto pointColl = GetPointCollision(pos, item.RoomNumber);

		// 1) Test for water room.
		if (!TestEnvironment(ENV_FLAG_WATER, pointColl.GetRoomNumber()))
			return Vector3::Zero;

		// 2) Assess point collision.
		if (pos.y >= (pointColl.GetFloorHeight() - BUFFER) ||
			pos.y <= (pointColl.GetWaterTopHeight() + BUFFER) ||
			pointColl.GetSector().IsWall(item.Pose.Position.x + BUFFER, item.Pose.Position.z + BUFFER) ||
			pointColl.GetSector().IsWall(item.Pose.Position.x - BUFFER, item.Pose.Position.z - BUFFER))
		{
			return Vector3::Zero;
		}

		return pos;
	}

	void UpdateFishSwarm()
	{
		constexpr auto WATER_SURFACE_OFFSET = CLICK(0.5f);
		constexpr auto FLEE_VEL				= 20.0f;
		constexpr auto TARGET_REACHED_TOLERANCE = BLOCK(0.5f);

		static const auto SPHERE = BoundingSphere(Vector3::Zero, BLOCK(1 / 8.0f));

		if (FishSwarm.empty())
			return;

		const auto& playerItem = *LaraItem;
		const auto& player = GetLaraInfo(playerItem);

		const FishData* closestFishPtr = nullptr;
		float minDistToTarget = INFINITY;
		int minDist = INT_MAX;

		int fishID = 0;
		for (auto& fish : FishSwarm)
		{
			if (fish.Life <= 0.0f)
				continue;

			fish.StoreInterpolationData();

			// Increase separation distance for each fish.
			float separationDist = FISH_BASE_SEPARATION_DISTANCE + (fishID * 3);
			fishID += 1;

			auto& leaderItem = *fish.LeaderItemPtr;
			if (!leaderItem.ItemFlags[2] && fish.TargetItemPtr == fish.LeaderItemPtr)
			{
				if (!fish.IsPatrolling)
				{
					fish.TargetItemPtr->Pose.Position = GetFishStartPosition(leaderItem);

					if (fish.TargetItemPtr->Pose.Position != Vector3::Zero)
						leaderItem.ItemFlags[2] = 1;
				}
			}

			int enemyVel = (fish.TargetItemPtr != fish.LeaderItemPtr) ? 16.0f : 26.0f;

			fish.PositionTarget = Random::GeneratePointInSphere(SPHERE);

			// Calculate desired position based on target object and random offsets.
			auto desiredPos = fish.TargetItemPtr->Pose.Position + fish.PositionTarget;
			auto dir = desiredPos - fish.Position;

			auto dirs = dir.ToVector3();
			dirs.Normalize();
			auto dirNorm = dirs;

			// Define cohesion factor to keep fish close together.
			float distToTarget = dirs.Length();

			float targetVel = (distToTarget * FISH_COHESION_FACTOR) + Random::GenerateFloat(3.0f, 5.0f);
			fish.Velocity = std::min(targetVel, fish.TargetItemPtr->Animation.Velocity.z - 21.0f); 

			// If fish is too far from target, increase velocity to catch up.
			if (distToTarget > FISH_TARGET_DISTANCE_MAX)
				fish.Velocity += FISH_CATCH_UP_FACTOR; 

			// Translate.
			auto moveDir = fish.Orientation.ToDirection();
			moveDir.Normalize(); 
			fish.Position += (moveDir * fish.Velocity) / enemyVel;
			fish.Position += (moveDir * FISH_SPACING_FACTOR) / enemyVel;

			auto orientTo = Geometry::GetOrientToPoint(fish.Position, desiredPos.ToVector3());
			fish.Orientation.Lerp(orientTo, 0.1f);

			for (const auto& otherFish : FishSwarm)
			{
				if (&fish == &otherFish)
					continue;

				float distToOtherFish = Vector3i::Distance(fish.Position, otherFish.Position);
				float distToPlayer = Vector3i::Distance(fish.Position, playerItem.Pose.Position);
				float distToTarget = Vector3i::Distance(fish.Position, otherFish.PositionTarget);

				leaderItem.ItemFlags[7] = distToPlayer;

				// Update the index of the nearest fish to the target
				if (distToTarget < minDistToTarget &&
					(fish.TargetItemPtr == fish.LeaderItemPtr || fish.TargetItemPtr->ObjectNumber == ID_AI_FOLLOW))
				{
					minDistToTarget = distToTarget;
					closestFishPtr = &otherFish;
				}

				if (fish.TargetItemPtr != fish.LeaderItemPtr && fish.TargetItemPtr->ObjectNumber != ID_AI_FOLLOW)
					separationDist = 80.0f;

				if (distToOtherFish < separationDist)
				{
					auto separationDir = fish.Position - otherFish.Position;
					separationDir.Normalize();

					fish.Position += separationDir * (separationDist - distToOtherFish);
				}
				else
				{
				    fish.Velocity += FISH_CATCH_UP_FACTOR;
				}

				// Orient to fish nearest to target. Prevents other fish from swimming forward but oriented elsewhere.
				if (closestFishPtr != nullptr &&
					fish.Orientation.x != closestFishPtr->Orientation.x && separationDist > 30.0f &&
					(fish.TargetItemPtr == fish.LeaderItemPtr || fish.TargetItemPtr->ObjectNumber == ID_AI_FOLLOW))
				{
					separationDist--;
					auto orientTo = Geometry::GetOrientToPoint(fish.Position, closestFishPtr->Position);
					fish.Velocity += FISH_CATCH_UP_FACTOR;
				}

				// If player is too close and fish are not lethal, flee.
				if ((distToPlayer < separationDist * 3) && fish.IsLethal == false)
				{
					auto separationDir = fish.Position - playerItem.Pose.Position.ToVector3();
					separationDir.Normalize();

					fish.Position += separationDir * FLEE_VEL;

					auto orientTo = Geometry::GetOrientToPoint(fish.Position, separationDir);
					fish.Orientation.Lerp(orientTo, 0.05f);

					fish.Velocity -= std::min(FLEE_VEL, fish.TargetItemPtr->Animation.Velocity.z - 1.0f);
				}
			}

			auto pointColl = GetPointCollision(fish.Position, fish.RoomNumber);
			const auto& room = g_Level.Rooms[fish.RoomNumber];

			// Update fish room number.
			if (pointColl.GetRoomNumber() != fish.RoomNumber && 
				pointColl.GetRoomNumber() != NO_VALUE &&
				TestEnvironment(ENV_FLAG_WATER, pointColl.GetRoomNumber()))
			{
				fish.RoomNumber = pointColl.GetRoomNumber();
			}

			// Clamp position to slightly below water surface.
			int waterHeight = pointColl.GetWaterTopHeight();
			if (fish.Position.y < (waterHeight + WATER_SURFACE_OFFSET))
				fish.Position.y = waterHeight + WATER_SURFACE_OFFSET;
			
			if (ItemNearTarget(fish.Position, fish.TargetItemPtr, CLICK(0.5f)) &&
				fish.LeaderItemPtr != fish.TargetItemPtr)
			{
				if (fish.TargetItemPtr->ObjectNumber != ID_AI_FOLLOW)
				{
					DoBloodSplat(
						fish.Position.x, fish.Position.y, fish.Position.z,
						Random::GenerateFloat(4.0f, 8.0f),
						fish.TargetItemPtr->Pose.Orientation.y, fish.TargetItemPtr->RoomNumber);
					DoDamage(fish.TargetItemPtr, FISH_HARM_DAMAGE);
				}
				else 
				{
					leaderItem.ItemFlags[2]++;
				}
			}
			else if (Vector3i::Distance(fish.Position, desiredPos) < TARGET_REACHED_TOLERANCE &&
				fish.LeaderItemPtr == fish.TargetItemPtr)
			{
				leaderItem.ItemFlags[2] = 0;
			}			
			
			// Calculate undulation angle based on sine wave and fish velocity.
			float movementValue = abs(moveDir.z);
			float undulationAngle = sin(fish.Undulation) * ANGLE(std::clamp(movementValue * 7.0f, 4.0f, 7.0f));

			// Apply undulation.
			fish.Orientation.y += undulationAngle;

			// Update undulation.
			fish.Undulation += std::clamp(movementValue / 2, 0.3f, 1.0f);
			if (fish.Undulation > PI_MUL_2)
				fish.Undulation -= PI_MUL_2;

			fish.Transform = fish.Orientation.ToRotationMatrix() * Matrix::CreateTranslation(fish.Position);
		}
	}

	void ClearFishSwarm()
	{
		FishSwarm.clear();
	}
}
