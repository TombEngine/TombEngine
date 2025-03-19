#include "framework.h"
#include "Objects/TR4/Trap/tr4_teethspike.h"

#include "Game/camera.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/effects/tomb4fx.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Collision::Point;

namespace TEN::Entities::Traps
{
	constexpr auto TEETH_SPIKE_HARM_DAMAGE_CONSTANT	   = 8;
	constexpr auto TEETH_SPIKE_HARM_DAMAGE_EMERGING	   = 30;
	constexpr auto TEETH_SPIKES_DEFAULT_INTERVAL	   = 64;
	constexpr auto TEETH_SPIKE_BOUNDS_TOLERANCE_RATIO = 0.95f;

	void InitializeTeethSpikes(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		// Set mutators to EulerAngles identity by default.
		for (auto& mutator : item.Model.Mutators)
			mutator.Scale.y = 0.0f;

		item.Status = ITEM_INVISIBLE;
		item.ItemFlags[0] = 1024;
		item.ItemFlags[2] = 0;
	}

	ContainmentType TestBoundsCollideTeethSpikes(ItemInfo* item, ItemInfo* collidingItem)
	{
		// Get both teeth spikes and colliding item bounds.
		auto spikeBox = GameBoundingBox(item).ToBoundingOrientedBox(item->Pose);
		auto itemBox = GameBoundingBox(collidingItem).ToBoundingOrientedBox(collidingItem->Pose);

		// Make intersection more forgiving by slightly reducing spike bounds.
		spikeBox.Extents = spikeBox.Extents * TEETH_SPIKE_BOUNDS_TOLERANCE_RATIO;
		return spikeBox.Contains(itemBox);
	}

	void DoTeethSpikeCollision(ItemInfo* trapItem, int radius)
	{
		CollisionInfo coll = {};
		coll.Setup.Radius = radius;
		coll.Setup.PrevPosition = trapItem->Pose.Position;
		coll.Setup.EnableObjectPush = false;

		DoObjectCollision(trapItem, &coll);
	}

	void ControlTeethSpikes(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (TriggerActive(&item) && item.ItemFlags[2] == 0)
		{
			// Get current item bounds and radius.
			const auto& bounds = GetClosestKeyframe(item).BoundingBox;
			int radius = std::max(abs(bounds.X2 - bounds.X1), abs(bounds.Z2 - bounds.Z1)) / 2;

			// Play sound only if spikes are just emerging.
			if (item.ItemFlags[0] == 1024 && item.TriggerFlags != 1)
				SoundEffect(SFX_TR4_TEETH_SPIKES, &item.Pose);

			// Immediately set spike state to fully protruded if flag is set.
			if (item.TriggerFlags == 1)
				item.ItemFlags[1] = 5120;

			// Kill enemies.
			item.Animation.Velocity.z = VEHICLE_COLLISION_TERMINAL_VELOCITY;
			DoTeethSpikeCollision(&item, radius * TEETH_SPIKE_BOUNDS_TOLERANCE_RATIO);

			item.Status = ITEM_ACTIVE;

			auto intersection = TestBoundsCollideTeethSpikes(&item, LaraItem);

			if (LaraItem->Animation.ActiveState != LS_DEATH && intersection != ContainmentType::DISJOINT)
			{
				// Calculate spike angle to horizon. If angle is upward, impale player.
				auto normal = Vector3::Transform(Vector3::UnitY, item.Pose.Orientation.ToRotationMatrix());
				float dot = Vector3::UnitX.Dot(normal);
				float angle = acos(dot / sqrt(normal.LengthSquared() * Vector3::UnitX.LengthSquared()));

				const auto& playerBounds = GetClosestKeyframe(*LaraItem).BoundingBox;

				int bloodCount = 0;

				// Spikes are upward and player is jumping, or spikes have just emerged - impale.
				if ((item.ItemFlags[0] >= 1024 || LaraItem->Animation.IsAirborne) &&
					(angle > PI * 0.25f && angle < PI * 0.75f))
				{
					if (LaraItem->Animation.Velocity.y > 6.0f || item.ItemFlags[0] > 1024)
					{
						LaraItem->HitPoints = -1;
						bloodCount = 20;
					}
				}
				// Spikes emerging or already fully protruded (in latter case, only damage player if running).
				else if ((item.TriggerFlags != 1) || LaraItem->Animation.Velocity.z >= 30.0f)
				{
					int damage = item.ItemFlags[0] == 1024 ? TEETH_SPIKE_HARM_DAMAGE_EMERGING : TEETH_SPIKE_HARM_DAMAGE_CONSTANT;
					DoDamage(LaraItem, damage);
					bloodCount = (GetRandomControl() & 3) + 2;
				}
				// Spikes retracting; do nothing.
				else
				{
					bloodCount = 0;
				}

				int y1, y2;
				int yTop = playerBounds.Y1 + LaraItem->Pose.Position.y;
				int yBottom = playerBounds.Y2 + LaraItem->Pose.Position.y;
				
				// Spikes pointing downward; move blood origin to top.
				if (angle < PI * 0.125f || angle > PI * 0.825f)
				{
					y1 = -bounds.Y2;
					y2 = -bounds.Y1;
				}
				// Spikes pointing upward; leave origin as is.
				else
				{
					y1 = bounds.Y1;
					y2 = bounds.Y2;
				}

				if (yTop < y1 + item.Pose.Position.y)
					yTop = y1 + item.Pose.Position.y;
				if (yBottom > y2 + item.Pose.Position.y)
					yBottom = y2 + item.Pose.Position.y;	

				int dy = (abs(yTop - yBottom)) + 1;

				// Increase blood if spikes are protruding from side.
				if ((angle > PI * 0.125f && angle < PI * 0.375f) ||
					(angle > PI * 0.625f && angle < PI * 0.750f))
				{
					bloodCount >>= 1;
				}

				for (int i = 0; i < bloodCount; i++)
				{
					int dx = LaraItem->Pose.Position.x + (GetRandomControl() & 127) - 64;
					int dz = LaraItem->Pose.Position.z + (GetRandomControl() & 127) - 64;
					TriggerBlood(dx, yBottom - (GetRandomControl() % dy), dz, GetRandomControl() << 1, 1);
				}

				if (LaraItem->HitPoints <= 0 && Lara.Context.Vehicle == NO_VALUE)
				{
					int heightFromFloor = GetPointCollision(*LaraItem).GetFloorHeight() - LaraItem->Pose.Position.y;

					if (item.Pose.Position.y >= LaraItem->Pose.Position.y && heightFromFloor < CLICK(1))
					{
						SetAnimation(*LaraItem, LA_SPIKE_DEATH);
						LaraItem->Animation.IsAirborne = false;

						Camera.flags = CF_FOLLOW_CENTER;
						Camera.targetAngle = ANGLE(-150.0f);
						Camera.targetElevation = ANGLE(-25.0f);
						Camera.targetDistance = BLOCK(2);
					}
				}
			}

			item.ItemFlags[0] += 128;
			item.ItemFlags[1] += item.ItemFlags[0];
			if (item.ItemFlags[1] >= 5120)
			{
				item.ItemFlags[1] = 5120;
				if (item.ItemFlags[0] <= 1024)
				{
					item.ItemFlags[0] = 0;
					if (item.TriggerFlags != 1 && LaraItem->HitPoints > 0)
					{
						int customInterval = item.TriggerFlags;
						item.ItemFlags[2] = customInterval ? customInterval : TEETH_SPIKES_DEFAULT_INTERVAL;
					}
				}
				else
				{
					item.ItemFlags[0] = -item.ItemFlags[0] >> 1;
				}
			}
		}
		else if (TriggerActive(&item))
		{
			item.ItemFlags[0] += (item.ItemFlags[0] >> 3) + 32;
			item.ItemFlags[1] -= item.ItemFlags[0];
			if (item.ItemFlags[1] <= 0)
			{
				item.ItemFlags[0] = 1024;
				item.ItemFlags[1] = 0;
				item.Status = ITEM_INVISIBLE;
			}

			if (item.TriggerFlags != 2)
			{
				if (item.ItemFlags[2])
					item.ItemFlags[2]--;
			}
			else
			{
				item.ItemFlags[2] = 1;
			}
		}
		else if (!item.Timer)
		{
			if (item.ItemFlags[1] > 0)
			{
				item.ItemFlags[0] += (item.ItemFlags[0] >> 3) + 32;
				item.ItemFlags[1] -= item.ItemFlags[0];
				if (item.ItemFlags[1] < 0)
					item.ItemFlags[1] = 0;
			}
		}

		// Update bone mutators.
		for (auto& mutator : item.Model.Mutators)
		{
			float scale = (float)item.ItemFlags[1] / 4096.0f;
			if (scale > 0.0f)
			{
				mutator.Scale = Vector3(1.0f, scale, 1.0f);
			}
			else
			{
				mutator.Scale = Vector3::Zero;
			}
		}
	}
}
