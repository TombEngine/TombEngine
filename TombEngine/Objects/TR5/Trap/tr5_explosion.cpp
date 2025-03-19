#include "framework.h"
#include "Objects/TR5/Trap/tr5_explosion.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/effects/debris.h"
#include "Game/effects/effects.h"
#include "Game/effects/item_fx.h"
#include "Game/effects/tomb4fx.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_one_gun.h"
#include "Game/Lara/lara_fire.h"
#include "Game/Setup.h"
#include "Objects/Generic/Switches/generic_switch.h"
#include "Objects/TR5/Shatter/tr5_smashobject.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Effects::Items;
using namespace TEN::Entities::Switches;

namespace TEN::Entities::Traps
{
	void InitializeExplosion(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (item.TriggerFlags >= 30000)
		{
			item.ItemFlags[1] = 3;
			item.TriggerFlags -= 30000;
		}
		else if (item.TriggerFlags >= 20000)
		{
			item.ItemFlags[1] = 2;
			item.TriggerFlags -= 20000;
		}
		else if (item.TriggerFlags >= 10000)
		{
			item.ItemFlags[1] = 1;
			item.TriggerFlags -= 10000;
		}

		if (item.TriggerFlags >= 1000)
		{
			item.ItemFlags[3] = 1;
			item.TriggerFlags -= 1000;
		}

		item.ItemFlags[2] = item.TriggerFlags / 100;
		item.TriggerFlags = 7 * (item.TriggerFlags % 100);
	}

	void ControlExplosion(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (TriggerActive(&item))
		{
			item.Flags |= IFLAG_INVISIBLE;

			if (item.ItemFlags[0] < item.TriggerFlags)
			{
				++item.ItemFlags[0];
			}
			else if (item.ItemFlags[0] == item.TriggerFlags)
			{
				int flag;
				++item.ItemFlags[0];

				if (TestEnvironment(ENV_FLAG_WATER, item.RoomNumber) ||
					TestEnvironment(ENV_FLAG_SWAMP, item.RoomNumber))
				{
					flag = 1;
				}
				else
				{
					flag = item.ItemFlags[1] == 1 ? 2 : 0;
				}

				SoundEffect(SFX_TR4_EXPLOSION1, &item.Pose, SoundEnvironment::Land, 1.5f);
				SoundEffect(SFX_TR4_EXPLOSION2, &item.Pose);
				TriggerExplosionSparks(item.Pose.Position.x, item.Pose.Position.y, item.Pose.Position.z, 3, -2, flag, item.RoomNumber);

				for (int i = 0; i < item.ItemFlags[2]; ++i)
				{
					TriggerExplosionSparks(
						item.Pose.Position.x + (GetRandomControl() % 128 - 64) * item.ItemFlags[2],
						item.Pose.Position.y + (GetRandomControl() % 128 - 64) * item.ItemFlags[2],
						item.Pose.Position.z + (GetRandomControl() % 128 - 64) * item.ItemFlags[2],
						2, 0, flag, item.RoomNumber);
				}

				auto pose = Pose::Zero;
				pose.Position.x = item.Pose.Position.x;
				pose.Position.y = item.Pose.Position.y - 128;
				pose.Position.z = item.Pose.Position.z;

				if (item.ItemFlags[3])
				{
					if (flag == 2)
					{
						TriggerShockwave(&pose, 48, 32 * item.ItemFlags[2] + 304, 4 * item.ItemFlags[2] + 96, 0, 96, 128, 24, EulerAngles(2048, 0.0f, 0.0f), 0, true, false, false, (int)ShockwaveStyle::Normal);
					}
					else
					{
						TriggerShockwave(&pose, 48, 32 * item.ItemFlags[2] + 304, 4 * item.ItemFlags[2] + 96, 128, 96, 0, 24, EulerAngles(2048, 0.0f, 0.0f), 0, true, false, false, (int)ShockwaveStyle::Normal);
					}
				}

				if (flag != 2)
				{
					auto vec = GetJointPosition(LaraItem, LM_HIPS);

					int dx = vec.x - item.Pose.Position.x;
					int dy = vec.y - item.Pose.Position.y;
					int dz = vec.z - item.Pose.Position.z;

					if (abs(dx) < BLOCK(1) &&
						abs(dy) < BLOCK(1) &&
						abs(dz) < BLOCK(1))
					{
						int distance = sqrt(pow(dx, 2) + pow(dy, 2) + pow(dz, 2));
						if (distance < BLOCK(2))
						{
							DoDamage(LaraItem, distance / 16);

							if (distance < CLICK(3))
								ItemBurn(LaraItem);
						}
					}
				}

				auto collObjects = GetCollidedObjects(item, true, true, BLOCK(2), ObjectCollectionMode::All);
				if (!collObjects.IsEmpty())
				{
					for (auto* itemPtr : collObjects.Items)
					{
						if (itemPtr->ObjectNumber >= ID_SMASH_OBJECT1 && itemPtr->ObjectNumber <= ID_SMASH_OBJECT16)
						{
							TriggerExplosionSparks(itemPtr->Pose.Position.x, itemPtr->Pose.Position.y, itemPtr->Pose.Position.z, 3, -2, 0, itemPtr->RoomNumber);
							itemPtr->Pose.Position.y -= 128;
							TriggerShockwave(&itemPtr->Pose, 48, 304, 96, 128, 96, 0, 24, EulerAngles::Identity, 0, true, false, false, (int)ShockwaveStyle::Normal);
							itemPtr->Pose.Position.y += 128;
							ExplodeItemNode(itemPtr, 0, 0, 80);
							SmashObject(itemPtr->Index);
							KillItem(itemPtr->Index);
						}
						else if (itemPtr->ObjectNumber != ID_SWITCH_TYPE7 && itemPtr->ObjectNumber != ID_SWITCH_TYPE8)
						{
							if (Objects[itemPtr->ObjectNumber].intelligent)
								DoExplosiveDamage(*LaraItem, *itemPtr, item, Weapons[(int)LaraWeaponType::GrenadeLauncher].Damage);
						}
						else
						{
							// @FIXME: This calls CrossbowHitSwitchType78()
						}
					}

					for (auto* staticPtr : collObjects.Statics)
					{
						if (Statics[staticPtr->staticNumber].shatterType != ShatterType::None)
						{
							TriggerExplosionSparks(staticPtr->pos.Position.x, staticPtr->pos.Position.y, staticPtr->pos.Position.z, 3, -2, 0, item.RoomNumber);
							staticPtr->pos.Position.y -= 128;
							TriggerShockwave(&staticPtr->pos, 40, 176, 64, 128, 96, 0, 16, EulerAngles::Identity, 0, true, false, false, (int)ShockwaveStyle::Normal);
							staticPtr->pos.Position.y += 128;
							SoundEffect(GetShatterSound(staticPtr->staticNumber), &staticPtr->pos);
							ShatterObject(nullptr, staticPtr, -128, item.RoomNumber, 0);
						}
					}

					AlertNearbyGuards(&item);
				}

				if (item.ItemFlags[1] >= 2)
				{
					if (item.ItemFlags[1] == 3)
					{
						short triggerItems[8];
						for (int i = GetSwitchTrigger(&item, triggerItems, 1); i > 0; --i)
							g_Level.Items[triggerItems[i - 1]].ItemFlags[0] = 0;

						item.ItemFlags[0] = 0;
					}
				}
				else
				{
					KillItem(itemNumber);
				}
			}
		}
	}
}
