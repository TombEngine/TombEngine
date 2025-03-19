#include "framework.h"
#include "Objects/TR4/Trap/SquishyBlock.h"

#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/collision/Sphere.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;
using namespace TEN::Collision::Sphere;

// NOTES:
// item.ItemFlags[0]: use dynamic motion.
// item.ItemFlags[1]: ??
// item.ItemFlags[4]: heading angle.
// TODO: In future add support for 4 directions.

namespace TEN::Entities::Traps
{	
	constexpr auto SQUISHY_BLOCK_LETHAL_FRAME = 33;
	constexpr auto MAX_FALLING_VELOCITY = 60;
	constexpr auto MAX_VELOCITY = 300;
	constexpr auto FALLING_BLOCK_IMPACT_FRAME = 8;
	constexpr auto FALLING_BLOCK_NEXT_FRAME = 2;

	enum SquishyBlockState
	{
		SQUISHY_BLOCK_STATE_MOVE = 0,
		SQUISHY_BLOCK_STATE_COLLIDE_LEFT = 1,
		SQUISHY_BLOCK_STATE_COLLIDE_RIGHT = 2,
		SQUISHY_BLOCK_STATE_BAKED_MOTION = 3,
		SQUISHY_BLOCK_STATE_COLLIDE_FRONT = 4,
		SQUISHY_BLOCK_STATE_COLLIDE_BACK = 5
	};

	enum SquishyBlockAnim
	{
		SQUISHY_BLOCK_ANIM_MOVE = 0,
		SQUISHY_BLOCK_ANIM_IMPACT_BACK = 1,
		SQUISHY_BLOCK_ANIM_IMPACT_FRONT = 2,
		SQUISHY_BLOCK_ANIM_BAKED_MOTION = 3,
		SQUISHY_BLOCK_ANIM_COLLIDE_FRONT = 4,
		SQUISHY_BLOCK_ANIM_COLLIDE_BACK = 5
	};

	void InitializeSquishyBlock(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!item.TriggerFlags)
			SetAnimation(item, SQUISHY_BLOCK_ANIM_BAKED_MOTION);

		item.HitPoints = NOT_TARGETABLE;
		item.ItemFlags[0] = item.TriggerFlags;
		item.ItemFlags[1] = 0;
		item.ItemFlags[4] = 0;
	}

	void ControlSquishyBlock(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!TriggerActive(&item))
			return;

		const auto& object = Objects[item.ObjectNumber];

		short& velocity = item.ItemFlags[0];
		short& someAngle = item.ItemFlags[1];
		short& headingAngle = item.ItemFlags[4];

		if (!item.TriggerFlags)
		{
			if (item.Animation.ActiveState != SQUISHY_BLOCK_STATE_BAKED_MOTION)
				SetAnimation(item, SQUISHY_BLOCK_ANIM_BAKED_MOTION);
		}
		else
		{
			if (item.Animation.ActiveState == SQUISHY_BLOCK_ANIM_BAKED_MOTION)
				SetAnimation(item, SQUISHY_BLOCK_STATE_MOVE);

			velocity = item.TriggerFlags;

			if (velocity > MAX_VELOCITY)
				velocity = MAX_VELOCITY;

			if (item.Animation.ActiveState == SQUISHY_BLOCK_STATE_MOVE)
			{
				auto forwardDir = EulerAngles(0, item.Pose.Orientation.y + headingAngle, 0).ToDirection();

				auto pointColl = GetPointCollision(item.Pose.Position, item.RoomNumber, forwardDir, BLOCK(0.5f));

				if (pointColl.GetRoomNumber() != item.RoomNumber)
					ItemNewRoom(itemNumber, pointColl.GetRoomNumber());

				if (!IsNextSectorValid(item, forwardDir, BLOCK(0.5f), true))
				{
					switch (headingAngle)
					{
					default:
					case ANGLE(0.0f):
						SetAnimation(item, SQUISHY_BLOCK_ANIM_IMPACT_FRONT);
						break;

					case ANGLE(-180.0f):
						SetAnimation(item, SQUISHY_BLOCK_ANIM_IMPACT_BACK);
						break;
					}
				}
				else
				{
					float dist = Lerp(item.TriggerFlags / 4, item.TriggerFlags, velocity);
					item.Pose.Position = Geometry::TranslatePoint(item.Pose.Position, forwardDir, dist);
				}
			}
			else
			{
				if (item.Animation.FrameNumber == GetAnimData(item).EndFrameNumber)
				{
					if (item.HitPoints != NOT_TARGETABLE && item.HitPoints)
					{
						someAngle = item.HitPoints;
						item.HitPoints = NOT_TARGETABLE;
					}

					headingAngle += ANGLE(180.0f);
					item.Pose.Orientation.y += ANGLE(someAngle);
				}				
			}
		}

		AnimateItem(item);		
	}

	void ControlFallingSquishyBlock(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!TriggerActive(&item))
			return;

		short& velocity = item.ItemFlags[0];

		if (velocity < MAX_FALLING_VELOCITY)
		{
			SoundEffect(SFX_TR4_EARTHQUAKE_LOOP, &item.Pose);
			Camera.bounce = (velocity - 92) / 2;
			velocity++;
		}
		else
		{
			if (item.Animation.FrameNumber == FALLING_BLOCK_IMPACT_FRAME)
				Camera.bounce = -96;

			AnimateItem(item);
		}
	}

	void CollideSquishyBlock(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
	{
		if (playerItem->HitPoints <= 0)
			return;

		auto& item = g_Level.Items[itemNumber];

		if (!TestBoundsCollide(&item, playerItem, coll->Setup.Radius))
			return;
			
		if (!HandleItemSphereCollision(item, *playerItem))
			return;

		if (!ItemPushItem(&item, playerItem, coll, false, 1))
			return;

		if (item.Animation.ActiveState == SQUISHY_BLOCK_STATE_BAKED_MOTION)
		{
			int frameNumber = item.Animation.FrameNumber;
			if (frameNumber == 0 || frameNumber == SQUISHY_BLOCK_LETHAL_FRAME)
				DoDamage(playerItem, INT_MAX);
		}
		else if (item.Animation.ActiveState == SQUISHY_BLOCK_STATE_COLLIDE_RIGHT ||
				 item.Animation.ActiveState == SQUISHY_BLOCK_STATE_COLLIDE_LEFT)
		{
			DoDamage(playerItem, INT_MAX);
		}
	}

	void CollideFallingSquishyBlock(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!TestBoundsCollide(&item, playerItem, coll->Setup.Radius))
			return;
			
		if (!HandleItemSphereCollision(item, *playerItem))
			return;

		if (item.Animation.FrameNumber <= FALLING_BLOCK_IMPACT_FRAME)
		{
			item.Animation.FrameNumber += FALLING_BLOCK_NEXT_FRAME;

			DoDamage(playerItem, INT_MAX);
			SetAnimation(*playerItem, LA_BOULDER_DEATH);
			playerItem->Animation.Velocity.y = 0.0f;	
			playerItem->Animation.Velocity.z = 0.0f;	
		}
		else if (playerItem->HitPoints > 0)
		{
			ItemPushItem(&item, playerItem, coll, false, 1);
		}
	}
}
