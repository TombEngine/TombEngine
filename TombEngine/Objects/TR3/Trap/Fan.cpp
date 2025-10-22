#include "framework.h"
#include "Objects/TR3/Trap/Fan.h"

#include "Game/animation.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Sphere.h"
#include "Game/control/control.h"
#include "Game/collision/Point.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Collision::Sphere;
using namespace TEN::Collision::Point;

namespace TEN::Entities::Traps
{
	constexpr auto TURNING_BLADE_HARM_DAMAGE = 100;

	enum FanState
	{
		FAN_STATE_ROTATING = 0,
		FAN_STATE_IDLE = 1,

	};

	enum FanAnim
	{
		FAN_ANIM_ROTATING = 0,
		FAN_ANIM_STOPPING = 1,
		FAN_ANIM_IDLE = 2,
		FAN_ANIM_STARTING = 3,
	};

	const std::vector<unsigned int> TurningBladeHarmJoints = { 2, 3, 4, 5, 6, 7, 8, 9, 10 };


	void ControlFan(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!TriggerActive(&item))
		{
			if (item.Animation.ActiveState != FAN_STATE_IDLE)
				{
					item.Animation.TargetState = FAN_STATE_IDLE;
				}
			return;
		}

		AnimateItem(&item);


		if (!TriggerActive(item) || item->flags & IFL_INVISIBLE)
		{
			if (item->goal_anim_state != 1)
			{
				if (item->object_number == FAN)
					SoundEffect(SFX_UNDERWATER_FAN_STOP, &item->pos, SFX_WATER);

				item->goal_anim_state = 1;
			}
		}
		else
		{
			item->goal_anim_state = 0;

			if (item->touch_bits & 6)
			{
				if (CurrentLevel == LV_ROOFTOPS)
				{
					lara_item->hit_points = -1;
					DoLotsOfBlood(lara_item->pos.x_pos, lara_item->pos.y_pos - 512, lara_item->pos.z_pos,
						short(GetRandomControl() >> 10), item->pos.y_rot + 0x4000, lara_item->room_number, 5);
				}
				else
					lara_item->hit_points -= 200;

				lara_item->hit_status = 1;
				DoLotsOfBlood(lara_item->pos.x_pos, lara_item->pos.y_pos - 512, lara_item->pos.z_pos,
					short(GetRandomControl() >> 10), item->pos.y_rot + 0x4000, lara_item->room_number, 3);

				if (item->object_number == SAW)
					SoundEffect(SFX_VERY_SMALL_WINCH, &item->pos, 0);
			}
			else if (item->object_number == SAW)
				SoundEffect(SFX_DRILL_BIT_1, &item->pos, SFX_DEFAULT);
			else if (item->object_number == FAN)
				SoundEffect(SFX_UNDERWATER_FAN_ON, &item->pos, SFX_WATER);
			else
				SoundEffect(SFX_SMALL_FAN_ON, &item->pos, SFX_DEFAULT);
		}

		AnimateItem(item);

		if (item->status == ITEM_DEACTIVATED)
		{
			RemoveActiveItem(item_number);

			if (item->object_number != SAW)
				item->collidable = 0;
		}

	}

	void CollideFan(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* item = &g_Level.Items[itemNumber];

		if (!TriggerActive(item))
			return;

		if (item->Status == ITEM_INVISIBLE)
			return;

		if (!TestBoundsCollide(item, laraItem, coll->Setup.Radius))
			return;

		if (!HandleItemSphereCollision(*item, *laraItem))
			return;

		// Blades deal damage cumulatively.
		auto spheres = item->GetSpheres();
		for (int i = 0; i < TurningBladeHarmJoints.size(); i++)
		{
			if (item->TouchBits.Test(TurningBladeHarmJoints[i]))
			{
				DoDamage(laraItem, TURNING_BLADE_HARM_DAMAGE);
				DoBloodSplat(
					(GetRandomControl() & 0x3F) + laraItem->Pose.Position.x - 32,
					(GetRandomControl() & 0x1F) + spheres[i].Center.y - 16,
					(GetRandomControl() & 0x3F) + laraItem->Pose.Position.z - 32,
					(GetRandomControl() & 3) + 2,
					GetRandomControl() * 2,
					laraItem->RoomNumber);

				TriggerLaraBlood();

				if (laraItem->HitPoints > 0)
				{
					ItemPushItem(item, laraItem, coll, false, 1);
				}
			}
		}
	}
}
