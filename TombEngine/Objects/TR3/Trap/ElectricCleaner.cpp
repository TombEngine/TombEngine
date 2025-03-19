#include "framework.h"
#include "Objects/TR3/Trap/ElectricCleaner.h"

#include "Game/collision/collide_item.h"
#include "Game/collision/floordata.h"
#include "Game/collision/Point.h"
#include "Game/control/box.h"
#include "Game/effects/item_fx.h"
#include "Game/effects/spark.h"
#include "Game/effects/tomb4fx.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/misc.h"
#include "Game/Setup.h"

using namespace TEN::Collision::Point;
using namespace TEN::Collision::Floordata;
using namespace TEN::Effects::Items;
using namespace TEN::Effects::Spark;

// NOTES
// ItemFlags[0] = rotation rate.
// ItemFlags[1] = behaviour flags.
//					0: flagTurnRight
//					1: flagPriorityForward
//					2: flagCounterClockwiseOrder
//					3: flagStopAfterKill
// ItemFlags[2] = movement velocity.
// ItemFlags[3, 4, 5] = counters for dynamic lights and sparks.
// ItemFlags[6] = target heading angle.

// OCB:
// 0: Stop after killing player, otherwise don't stop.

namespace TEN::Entities::Traps
{
	constexpr auto ELECTRIC_CLEANER_VELOCITY  = BLOCK(1 / 16.0f);
	constexpr auto ELECTRIC_CLEANER_TURN_RATE = ANGLE(5.6f);

	const auto ElectricCleanerHarmJoints	= std::vector<unsigned int>{ 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 };
	const auto ElectricCleanerWireEndJoints = std::vector<unsigned int>{ 5, 9, 13 };

	enum ElectricCleanerState
	{
		CHOOSE_PATH = 0,
		ROTATE = 1,
		MOVE = 2
	};

	void InitializeElectricCleaner(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		// Align to sector center.
		item.Pose.Position.x = (item.Pose.Position.x & ~WALL_MASK) | (int)BLOCK(0.5f);
		item.Pose.Position.z = (item.Pose.Position.z & ~WALL_MASK) | (int)BLOCK(0.5f);

		item.Data = (int)ElectricCleanerState::CHOOSE_PATH;

		// Initialize flags.
		item.ItemFlags[0] = ELECTRIC_CLEANER_TURN_RATE;
		item.ItemFlags[1] = 0;
		item.ItemFlags[2] = ELECTRIC_CLEANER_VELOCITY;
		item.ItemFlags[6] = item.Pose.Orientation.y;
		item.Collidable = true;

		// Set flagStopAfterKill.
		if (item.TriggerFlags)
		{
			item.ItemFlags[1] &= ~(1 << 3);
		}
		else
		{
			item.ItemFlags[1] |= (1 << 3);
		}
	}

	static Vector3 GetElectricCleanerMovementDirection(const ItemInfo& item, const Vector3& dir0, const Vector3& dir1, const Vector3& dir2)
	{
		if (IsNextSectorValid(item, dir0, BLOCK(1), false))
			return dir0;

		if (IsNextSectorValid(item, dir1, BLOCK(1), false))
			return dir1;

		if (IsNextSectorValid(item, dir2, BLOCK(1), false))
			return dir2;

		return Vector3::Zero;
	}
			
	static void HandleElectricCleanerItemCollision(ItemInfo& item)
	{
		auto backupPos = item.Pose.Position;

		switch (item.Pose.Orientation.y)
		{
		case ANGLE(0.0f):
			item.Pose.Position.z += BLOCK(0.5f);
			break;

		case ANGLE(90.0f):
			item.Pose.Position.x += BLOCK(0.5f);
			break;

		case ANGLE(-180.0f):
			item.Pose.Position.z -= BLOCK(0.5f);
			break;

		case ANGLE(-90.0f):
			item.Pose.Position.x -= BLOCK(0.5f);
			break;
		}

		auto collObjects = GetCollidedObjects(item, true, true);
		if (!collObjects.IsEmpty())
		{
			for (auto* itemPtr : collObjects.Items)
			{
				const auto& object = Objects[itemPtr->ObjectNumber];

				if (object.intelligent)
				{
					itemPtr->HitPoints = 0;
					ItemElectricBurn(itemPtr, 120);
				}
			}
		}

		item.Pose.Position = backupPos;
	}

	static void SpawnElectricCleanerSparks(ItemInfo& item)
	{
		SoundEffect(SFX_TR3_CLEANER_LOOP, &item.Pose);

		auto vel = Vector3i(
			(Random::GenerateInt(0, 255) * 4) - 512,
			Random::GenerateInt(0, 7) - 4,
			(Random::GenerateInt(0, 255) * 4) - 512);

		for (int i = 0; i < 3; i++)
		{
			if ((!(GetRandomControl() & 7) && !item.ItemFlags[3 + i]) || item.ItemFlags[3 + i])
			{
				if (!item.ItemFlags[3 + i])
					item.ItemFlags[3 + i] = Random::GenerateInt(0, 12) + 8;
				else
					item.ItemFlags[3 + i]--;

				int joint = ElectricCleanerWireEndJoints[i];
				auto pos = GetJointPosition(&item, joint, Vector3i(-160, -8, 16));

				byte c = Random::GenerateInt(0, 64) + 128;
				SpawnDynamicLight(pos.x, pos.y, pos.z, 10, c >> 2, c >> 1, c);

				auto& spark = GetFreeSparkParticle();

				spark = {};
				spark.active = true;
				spark.age = 0;
				float color = (192.0F + Random::GenerateFloat(0, 63.0F)) / 255.0F;
				spark.sourceColor = Vector4(color / 4, color / 2, color, 1.0F);
				color = (192.0F + Random::GenerateFloat(0, 63.0F)) / 255.0F;
				spark.destinationColor = Vector4(color / 4, color / 2, color, 1.0F);
				spark.life = Random::GenerateFloat(20, 27);
				spark.friction = 1.2f;
				spark.gravity = 1.5f;
				spark.width = 8.0f;
				spark.height = 96.0f;
				auto v = vel.ToVector3();
				v.Normalize(v);
				spark.velocity = v;
				spark.pos = pos.ToVector3();
				spark.room = item.RoomNumber;
			}
		}
	}

	void ControlElectricCleaner(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		const auto& object = Objects[item.ObjectNumber];

		auto& moveVel = item.ItemFlags[2];

		if (!TriggerActive(&item))
		{
			if (moveVel > 0)
			{
				moveVel = 0;
				SoundEffect(SFX_TR3_CLEANER_FUSEBOX, &item.Pose);
			}

			return;
		}

		if (moveVel <= 0)
			return;

		// Get flags.
		bool flagTurnRight			   = ((item.ItemFlags[1] & (1 << 0)) != 0);
		bool flagPriorityForward	   = ((item.ItemFlags[1] & (1 << 1)) != 0);
		bool flagCounterClockwiseOrder = ((item.ItemFlags[1] & (1 << 2)) != 0);

		int& activeState = item.Data;
		short& targetHeadingAngle = item.ItemFlags[6];

		switch (activeState)
		{
		case ElectricCleanerState::ROTATE:
			{
				short& rotRate = item.ItemFlags[0];

				int headingAngleDelta = abs(targetHeadingAngle - item.Pose.Orientation.y);

				// Continue rotating.
				if (headingAngleDelta > rotRate)
				{
					item.Pose.Orientation.y += flagTurnRight ? -rotRate : rotRate;
				}
				// End rotation.
				else
				{
					item.Pose.Orientation.y = targetHeadingAngle;
					activeState = ElectricCleanerState::MOVE;
				}
			}

			break;

		case ElectricCleanerState::MOVE:
			{
				auto pointColl = GetPointCollision(item);
				item.Pose.Position.y = pointColl.GetFloorHeight();

				auto forwardDir = EulerAngles(0, item.Pose.Orientation.y, 0).ToDirection();

				// Move forward.
				item.Pose.Position = Geometry::TranslatePoint(item.Pose.Position, forwardDir, moveVel);

				if ((item.Pose.Position.x & WALL_MASK) == BLOCK(0.5f) &&
					(item.Pose.Position.z & WALL_MASK) == BLOCK(0.5f))
				{
					// Only turn on flat floor.
					if (pointColl.GetFloorNormal() == -Vector3::UnitY)
						activeState = ElectricCleanerState::CHOOSE_PATH;
				}
			}

			break;

		case ElectricCleanerState::CHOOSE_PATH:
			{
				// Do triggers.
				TestTriggers(&item, true);

				// Search for next direction.
				auto forwardDir = EulerAngles(0, item.Pose.Orientation.y, 0).ToDirection();
				auto rightDir = EulerAngles(0, item.Pose.Orientation.y + ANGLE(90.0f), 0).ToDirection();
				auto newDir = Vector3::Zero;
				
				if (flagPriorityForward)
				{
					// Forward, left, right.
					if (flagCounterClockwiseOrder)
					{
						newDir = GetElectricCleanerMovementDirection(item, forwardDir, -rightDir, rightDir);
					}
					// Forward, right, left.
					else
					{
						newDir = GetElectricCleanerMovementDirection(item, forwardDir, rightDir, -rightDir);
					}
				}
				else
				{
					// Right, forward, left.
					if (flagCounterClockwiseOrder)
					{
						newDir = GetElectricCleanerMovementDirection(item, rightDir, forwardDir, -rightDir);
					}
					// Left, forward, right.
					else
					{
						newDir = GetElectricCleanerMovementDirection(item, -rightDir, forwardDir, rightDir);
					}
				}

				// Turn back.
				if (newDir == Vector3::Zero)
					newDir = -forwardDir;

				// Store target heading angle.
				targetHeadingAngle = phd_atan(newDir.z, newDir.x);

				// Not rotating; keep moving forward.
				if (item.Pose.Orientation.y - targetHeadingAngle == 0)
				{
					item.Pose.Position = Geometry::TranslatePoint(item.Pose.Position, forwardDir, moveVel);
					activeState = ElectricCleanerState::MOVE;
				}
				else
				{
					// Set flagTurnRight.		
					auto cross = newDir.Cross(forwardDir);
					if (cross.y > 0)
					{
						// Activate turn right.
						item.ItemFlags[1] |= (1 << 0);
					}
					else if (cross.y < 0)
					{
						// Activate turn left.
						item.ItemFlags[1] &= ~(1 << 0);
					}

					activeState = ElectricCleanerState::ROTATE;
				}
			}

			break;

		default:
			TENLog("Error handling unregistered electric cleaner state " + std::to_string(activeState), LogLevel::Warning, LogConfig::All, false);
			break;
		}

		AnimateItem(item);

		int probedRoomNumber = GetPointCollision(item).GetRoomNumber();
		if (item.RoomNumber != probedRoomNumber)
			ItemNewRoom(itemNumber, probedRoomNumber);

		auto radius = Vector2(object.radius, object.radius);
		AlignEntityToSurface(&item, radius);

		SpawnElectricCleanerSparks(item);
		HandleElectricCleanerItemCollision(item);
	}

	void CollideElectricCleaner(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto& item = g_Level.Items[itemNumber];

		ObjectCollision(itemNumber, laraItem, coll);

		if (item.TouchBits.Test(ElectricCleanerHarmJoints) && item.ItemFlags[2])
		{
			ItemElectricBurn(laraItem, -1);
			laraItem->HitPoints = 0;

			bool flagStopAfterKill = ((item.ItemFlags[1] & (1 << 3)) != 0);
			if (flagStopAfterKill)
				item.ItemFlags[2] = 0;

			SoundEffect(SFX_TR3_CLEANER_FUSEBOX, &item.Pose);
		}
	}
}
