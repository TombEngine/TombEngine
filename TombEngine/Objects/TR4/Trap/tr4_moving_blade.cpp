#include "framework.h"
#include "Objects/TR4/Trap/tr4_moving_blade.h"\

#include "Game/animation.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Sound/sound.h"
#include "Specific/level.h"

namespace TEN::Entities::Traps
{
    void ControlMovingBlade(short itemNumber)
    {
        auto& item = g_Level.Items[itemNumber];

        if (TriggerActive(&item))
        {
            item.ItemFlags[3] = 50;
            AnimateItem(&item);
        }
        else
        {
            item.Animation.FrameNumber = GetAnimData(item).frameBase;
        }
    }
}
