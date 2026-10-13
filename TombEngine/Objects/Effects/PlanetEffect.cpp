#include "framework.h"
#include "Objects/Effects/PlanetEffect.h"

#include "Game/Animation/Animation.h"
#include "Game/control/control.h"
#include "Game/control/trigger.h"
#include "Game/effects/Electricity.h"
#include "Game/items.h"
#include "Math/Random.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Effects::Electricity;
using namespace TEN::Math;

namespace TEN::Entities::Effects
{
    namespace
    {
        constexpr int PLANET_EFFECT_LINK_COUNT = 4;
        constexpr int PLANET_EFFECT_LINK_FLAG_OFFSET = 2;
        constexpr int PLANET_EFFECT_ELECTRICITY_FLAGS = (int)ElectricityFlags::Spline | (int)ElectricityFlags::ThinIn | (int)ElectricityFlags::ThinOut;

        void SpawnPlanetArc(const Vector3& origin, const Vector3& target, unsigned char green, unsigned char blue)
        {
            SpawnElectricity(origin, target, (GetRandomControl() & 0x1F) + 32, 0, green, blue, 32,
                PLANET_EFFECT_ELECTRICITY_FLAGS, 32, 3);
        }
    }

    void InitializePlanetEffect(short itemNumber)
    {
        auto& item = g_Level.Items[itemNumber];
        item.MeshBits.ClearAll();
        item.ItemFlags[0] = NO_VALUE;
        item.ItemFlags[1] = NO_VALUE;

        if (item.TriggerFlags == 1)
            for (int i = 0; i < PLANET_EFFECT_LINK_COUNT; i++)
                item.ItemFlags[PLANET_EFFECT_LINK_FLAG_OFFSET + i] = NO_VALUE;

        for (int i = 0; i < g_Level.NumItems; i++)
        {
            auto& candidate = g_Level.Items[i];
            if (item.ItemFlags[0] == NO_VALUE && candidate.TriggerFlags == item.TriggerFlags &&
                candidate.ObjectNumber >= ID_PUSHABLE_OBJECT1 && candidate.ObjectNumber <= ID_PUSHABLE_OBJECT5)
                item.ItemFlags[0] = (short)i;

            if (item.TriggerFlags == 1 && i != itemNumber && candidate.ObjectNumber == ID_PLANET_EFFECT &&
                candidate.TriggerFlags >= 2 && candidate.TriggerFlags <= 5 &&
                item.ItemFlags[candidate.TriggerFlags] == NO_VALUE)
            {
                item.ItemFlags[candidate.TriggerFlags] = (short)i;
            }
        }

        item.Status = ITEM_ACTIVE;
        AddActiveItem(itemNumber);
    }

    void ControlPlanetEffect(short itemNumber)
    {
        auto& item = g_Level.Items[itemNumber];
        if (!TriggerActive(&item))
        {
            item.ItemFlags[1] = NO_VALUE;
            return;
        }

        if (item.ItemFlags[0] >= 0 && item.ItemFlags[0] < g_Level.NumItems)
        {
            auto& pushable = g_Level.Items[item.ItemFlags[0]];
            pushable.TriggerFlags = -pushable.TriggerFlags;
            item.ItemFlags[0] = NO_VALUE;
        }

        item.MeshBits.SetAll();
        AnimateItem(item);

        if (item.TriggerFlags != 1)
            return;

        std::array<ItemInfo*, PLANET_EFFECT_LINK_COUNT> linkedEffects = {};
        for (int i = 0; i < PLANET_EFFECT_LINK_COUNT; i++)
        {
            const int linkedItemNumber = item.ItemFlags[PLANET_EFFECT_LINK_FLAG_OFFSET + i];
            if (linkedItemNumber < 0 || linkedItemNumber >= g_Level.NumItems)
            {
                item.ItemFlags[1] = NO_VALUE;
                return;
            }

            linkedEffects[i] = &g_Level.Items[linkedItemNumber];
            if (linkedEffects[i]->ObjectNumber != ID_PLANET_EFFECT ||
                (linkedEffects[i]->Flags & IFLAG_ACTIVATION_MASK) != IFLAG_ACTIVATION_MASK)
            {
                item.ItemFlags[1] = NO_VALUE;
                return;
            }
        }

        auto* animatingItem = FindItem(ID_ANIMATING4);
        if (animatingItem == nullptr)
        {
            item.ItemFlags[1] = NO_VALUE;
            return;
        }

        const auto origin = GetJointPosition(item, 0).ToVector3();
        const auto anchor = GetJointPosition(*animatingItem, 0).ToVector3();
        const auto blue = (unsigned char)((GetRandomControl() & 0x1F) + 224);
        const auto green = (unsigned char)(blue - (GetRandomControl() & 0x3F));

        SpawnElectricityGlow(origin, 28, 0, green, blue);
        SpawnElectricityGlow(anchor, 32, 0, green, blue);

        if (!(GlobalCounter & 3))
            SpawnPlanetArc(origin, anchor, green, blue);

        if (item.ItemFlags[1] < 0 || item.ItemFlags[1] > PLANET_EFFECT_LINK_COUNT + 1)
            item.ItemFlags[1] = (short)Random::GenerateInt(0, PLANET_EFFECT_LINK_COUNT + 1);

        ItemInfo* soundSource = item.ItemFlags[1] == 0 ? &item :
            item.ItemFlags[1] <= PLANET_EFFECT_LINK_COUNT ? linkedEffects[item.ItemFlags[1] - 1] : animatingItem;
        SoundEffect(SFX_TR4_ELECTRIC_ARCING_LOOP, &soundSource->Pose);

        auto previousPosition = origin;
        for (auto* linkedEffect : linkedEffects)
        {
            const auto position = GetJointPosition(*linkedEffect, 0).ToVector3();
            SpawnElectricityGlow(position, 28, 0, green, blue);

            if (!(GlobalCounter & 3))
                SpawnPlanetArc(position, previousPosition, green, blue);

            previousPosition = position;
        }
    }
}
