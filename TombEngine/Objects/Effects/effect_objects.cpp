#include "framework.h"
#include "Objects/Effects/effect_objects.h"

#include "Game/Animation/Animation.h"
#include "Game/control/control.h"
#include "Game/control/trigger.h"
#include "Game/effects/Electricity.h"
#include "Game/items.h"
#include "Game/Setup.h"
#include "Objects/Effects/enemy_missile.h"
#include "Objects/Effects/flame_emitters.h"
#include "Objects/Effects/LensFlare.h"
#include "Objects/Utils/object_helper.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Effects::Electricity;
using namespace TEN::Math;
using namespace TEN::Entities::Effects;

namespace
{
	constexpr int PLANET_EFFECT_LINK_COUNT = 4;
	constexpr int PLANET_EFFECT_LINK_FLAG_OFFSET = 2;
	constexpr int PLANET_EFFECT_ELECTRICITY_FLAGS =	(int)ElectricityFlags::Spline | (int)ElectricityFlags::ThinIn | (int)ElectricityFlags::ThinOut;

	void InitializePlanetEffect(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		item.MeshBits.ClearAll();
		item.ItemFlags[0] = NO_VALUE;

		for (int i = 0; i < g_Level.NumItems; i++)
		{
			auto& candidate = g_Level.Items[i];
			if (candidate.ObjectNumber >= ID_PUSHABLE_OBJECT1 && candidate.ObjectNumber <= ID_PUSHABLE_OBJECT5 &&
				candidate.TriggerFlags == item.TriggerFlags)
			{
				item.ItemFlags[0] = (short)i;
				break;
			}
		}

		if (item.TriggerFlags == 1)
		{
			for (int i = 0; i < PLANET_EFFECT_LINK_COUNT; i++)
			{
				item.ItemFlags[PLANET_EFFECT_LINK_FLAG_OFFSET + i] = NO_VALUE;
				const auto linkOcb = i + 2;

				for (int j = 0; j < g_Level.NumItems; j++)
				{
					auto& candidate = g_Level.Items[j];
					if (candidate.ObjectNumber == ID_PLANET_EFFECT && &candidate != &item && candidate.TriggerFlags == linkOcb)
					{
						item.ItemFlags[PLANET_EFFECT_LINK_FLAG_OFFSET + i] = (short)j;
						break;
					}
				}
			}
		}

		item.Status = ITEM_ACTIVE;
		AddActiveItem(itemNumber);
	}

	void ControlPlanetEffect(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		if (!TriggerActive(&item))
			return;

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
				return;

			linkedEffects[i] = &g_Level.Items[linkedItemNumber];
			if (linkedEffects[i]->ObjectNumber != ID_PLANET_EFFECT ||
				(linkedEffects[i]->Flags & IFLAG_ACTIVATION_MASK) != IFLAG_ACTIVATION_MASK)
				return;
		}

		auto* animatingItem = FindItem(ID_ANIMATING4);
		if (animatingItem == nullptr)
			return;

		const auto origin = GetJointPosition(item, 0).ToVector3();
		const auto anchor = GetJointPosition(*animatingItem, 0).ToVector3();
		const auto blue = (unsigned char)((GetRandomControl() & 0x1F) + 224);
		const auto green = (unsigned char)(blue - (GetRandomControl() & 0x3F));

		SpawnElectricityGlow(origin, 28, 0, green, blue);
		SpawnElectricityGlow(anchor, 32, 0, green, blue);

		if (!(GlobalCounter & 3))
		{
			SpawnElectricity(origin, anchor, (GetRandomControl() & 0x1F) + 32, 0, green, blue, 32,
				PLANET_EFFECT_ELECTRICITY_FLAGS, 32, 3);
		}
		SoundEffect(SFX_TR4_ELECTRIC_ARCING_LOOP, &item.Pose);

		auto previousPosition = origin;
		for (auto* linkedEffect : linkedEffects)
		{
			const auto position = GetJointPosition(*linkedEffect, 0).ToVector3();
			SpawnElectricityGlow(position, 28, 0, green, blue);

			if (!(GlobalCounter & 3))
				SpawnElectricity(position, previousPosition, (GetRandomControl() & 0x1F) + 32, 0, green, blue, 32,
					PLANET_EFFECT_ELECTRICITY_FLAGS, 32, 3);

			previousPosition = position;
		}
	}
}

void InitializeEffectsObjects()
{
	ObjectInfo* obj;

	// Flame is always loaded
	obj = &Objects[ID_FLAME];
	{
		obj->control = nullptr;
		obj->Hidden = true;
	}

	obj = &Objects[ID_FLAME_EMITTER];
	if (obj->loaded)
	{
		obj->Initialize = InitializeFlameEmitter;
		obj->collision = FlameEmitterCollision;
		obj->control = FlameEmitterControl;
		obj->Hidden = true;
	}

	obj = &Objects[ID_FLAME_EMITTER2];
	if (obj->loaded)
	{
		obj->Initialize = InitializeFlameEmitter2;
		obj->collision = FlameEmitterCollision;
		obj->control = FlameEmitter2Control;
		obj->Hidden = true;
	}

	obj = &Objects[ID_FLAME_EMITTER3];
	if (obj->loaded)
	{
		obj->Initialize = InitializeFlameEmitter3;
		obj->control = FlameEmitter3Control;
		obj->Hidden = true;
	}

	InitProjectile(obj, ControlEnemyMissile, ID_ENERGY_BUBBLES, true);

	obj = &Objects[ID_LENS_FLARE];
	if (obj->loaded)
	{
		obj->Hidden = true;
		obj->control = ControlLensFlare;
		obj->AlwaysActive = true;
	}

	obj = &Objects[ID_PLANET_EFFECT];
	if (obj->loaded)
	{
		obj->Initialize = InitializePlanetEffect;
		obj->control = ControlPlanetEffect;
		obj->AlwaysActive = true;
	}
}
