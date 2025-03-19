#include "framework.h"
#include "Objects/TR4/Trap/tr4_hammer.h"

#include "Game/Animation/Animation.h"
#include "Game/control/control.h"
#include "Game/effects/debris.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Objects/Generic/Switches/switch.h"
#include "Sound/sound.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Animation;

// NOTES:
// ItemFlags[0] | ItemFlags[1] = Harm joints.
// ItemFlags[2] = Timer in frame time before next activation.
// ItemFlags[3] = Damage.

namespace TEN::Entities::Traps
{
	constexpr auto HAMMER_HIT_DAMAGE	= 150;
	constexpr auto HAMMER_OCB4_INTERVAL = 60;
	constexpr auto HAMMER_HIT_FRAME		= 8;  // Frame pushables can be destroyed.
	constexpr auto HAMMER_CLOSED_FRAME	= 52; // Frame on whcih hammer is fully closed.

	constexpr auto RIGHT_HAMMER_BITS = (1 << 5) | (1 << 6) | (1 << 7);
	constexpr auto LEFT_HAMMER_BITS	 = (1 << 8) | (1 << 9) | (1 << 10);

	enum HammerState
	{
		HAMMER_STATE_NONE = 0,
		HAMMER_STATE_IDLE = 1,
		HAMMER_STATE_ACTIVE = 2,
	};

	enum HammerAnim
	{
		HAMMER_ANIM_INACTIVE = 0,
		HAMMER_ANIM_ACTIVE = 1
	};

	void ControlHammer(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		int frameNumber = item.Animation.FrameNumber;
		item.ItemFlags[3] = HAMMER_HIT_DAMAGE;

		if (!TriggerActive(&item))
		{
			*(long*)&item.ItemFlags[0] = 0;
			return;
		}

		bool isHammerTouched = false;

		if (!item.TriggerFlags)
		{
			if (frameNumber < HAMMER_CLOSED_FRAME)
			{
				*(long*)&item.ItemFlags[0] = RIGHT_HAMMER_BITS;
			}
			else
			{
				*(long*)&item.ItemFlags[0] = 0;
			}
		}
		else if (item.Animation.ActiveState == HAMMER_STATE_IDLE && item.Animation.TargetState == HAMMER_STATE_IDLE)
		{
			if (item.ItemFlags[2])
			{
				if (item.TriggerFlags == 3)
				{
					item.Flags &= ~CODE_BITS;
					item.ItemFlags[2] = 0;
				}
				else if (item.TriggerFlags == 4)
				{
					item.ItemFlags[2]--;
				}
				else
				{
					item.ItemFlags[2] = 0;
				}
			}
			else
			{
				item.Animation.AnimNumber = HAMMER_ANIM_ACTIVE;
				item.Animation.FrameNumber = 0;
				item.Animation.ActiveState = HAMMER_STATE_ACTIVE;
				item.Animation.TargetState = HAMMER_STATE_ACTIVE;
				item.ItemFlags[2] = HAMMER_OCB4_INTERVAL;
			}
		}
		else
		{
			item.Animation.TargetState = HAMMER_STATE_IDLE;

			if (frameNumber < HAMMER_CLOSED_FRAME)
			{
				*(long*)&item.ItemFlags[0] = RIGHT_HAMMER_BITS | LEFT_HAMMER_BITS;
			}
			else
			{
				*(long*)&item.ItemFlags[0] = 0;
			}

			if (frameNumber == HAMMER_HIT_FRAME)
			{
				if (item.TriggerFlags == 2)
				{
					short targetItem = g_Level.Rooms[item.RoomNumber].itemNumber;

					if (targetItem != NO_VALUE)
					{
						auto* target = &g_Level.Items[targetItem];
						for (; targetItem != NO_VALUE; targetItem = target->NextItem)
						{
							target = &g_Level.Items[targetItem];

							if (target->ObjectNumber == ID_OBELISK && target->Pose.Orientation.y == -ANGLE(270) &&
								g_Level.Items[target->ItemFlags[0]].Pose.Orientation.y == ANGLE(90) &&
								g_Level.Items[target->ItemFlags[1]].Pose.Orientation.y == 0)
							{
								target->Flags |= CODE_BITS;
								g_Level.Items[target->ItemFlags[0]].Flags |= CODE_BITS;
								g_Level.Items[target->ItemFlags[1]].Flags |= CODE_BITS;
								break;
							}
						}
					}

					SoundEffect(SFX_TR4_GENERIC_HEAVY_THUD, &item.Pose);
					SoundEffect(SFX_TR4_EXPLOSION2, &item.Pose);
				}
				else
				{
					int targetItemNumber = g_Level.Rooms[item.RoomNumber].itemNumber;
					if (targetItemNumber != NO_VALUE)
					{
						// TODO: What is this syntax?
						auto* targetItem = &g_Level.Items[targetItemNumber];
						for (; targetItemNumber != NO_VALUE; targetItemNumber = targetItem->NextItem)
						{
							targetItem = &g_Level.Items[targetItemNumber];

							if ((targetItem->ObjectNumber >= ID_PUSHABLE_OBJECT1 && targetItem->ObjectNumber <= ID_PUSHABLE_OBJECT10) ||
								(targetItem->ObjectNumber >= ID_PUSHABLE_OBJECT_CLIMBABLE1 && targetItem->ObjectNumber <= ID_PUSHABLE_OBJECT_CLIMBABLE10))
							{
								if (item.Pose.Position.x == targetItem->Pose.Position.x &&
									item.Pose.Position.z == targetItem->Pose.Position.z)
								{
									ExplodeItemNode(targetItem, 0, 0, 128);
									KillItem(targetItemNumber);
									isHammerTouched = true;
								}
							}
						}
					}

					if (isHammerTouched)
					{
						targetItemNumber = g_Level.Rooms[item.RoomNumber].itemNumber;

						if (targetItemNumber != NO_VALUE)
						{
							auto* target = &g_Level.Items[targetItemNumber];
							for (; targetItemNumber != NO_VALUE; targetItemNumber = target->NextItem)
							{
								target = &g_Level.Items[targetItemNumber];

								// Take all puzzle items, keys, and their combos.
								if ((target->ObjectNumber >= ID_PUZZLE_ITEM1 && target->ObjectNumber <= ID_PUZZLE_ITEM16) ||
									(target->ObjectNumber >= ID_PUZZLE_ITEM1_COMBO1 && target->ObjectNumber <= ID_PUZZLE_ITEM16_COMBO2) ||
									(target->ObjectNumber >= ID_KEY_ITEM1 && target->ObjectNumber <= ID_KEY_ITEM16) ||
									(target->ObjectNumber >= ID_KEY_ITEM1_COMBO1 && target->ObjectNumber <= ID_KEY_ITEM16_COMBO2))
								{
									if (item.Pose.Position.x == target->Pose.Position.x &&
										item.Pose.Position.z == target->Pose.Position.z)
									{
										target->Status = ITEM_NOT_ACTIVE;
									}
								}
							}
						}
					}
				}
			}
			else if (frameNumber > HAMMER_CLOSED_FRAME && item.TriggerFlags == 2)
				item.Flags &= ~CODE_BITS;
		}

		AnimateItem(item);
	}
}
