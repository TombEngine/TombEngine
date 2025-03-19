#include "framework.h"
#include "Objects/TR4/Trap/tr4_moving_blade.h"\

#include "Game/Animation/Animation.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;

namespace TEN::Entities::Traps
{
    void ControlMovingBlade(short itemNumber)
    {
        auto& item = g_Level.Items[itemNumber];

        if (TriggerActive(&item))
        {
            item.ItemFlags[3] = 50;
            AnimateItem(item);
        }
        else
        {
            item.Animation.FrameNumber = 0;
        }
    }
}
