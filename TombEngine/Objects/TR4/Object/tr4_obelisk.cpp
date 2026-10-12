#include "framework.h"
#include "Objects/TR4/Object/tr4_obelisk.h"

#include "Game/Animation/Animation.h"
#include "Game/control/control.h"
#include "Game/effects/debris.h"
#include "Game/effects/effects.h"
#include "Game/effects/Electricity.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Renderer/Renderer.h"
#include "Sound/sound.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Effects::Electricity;
using namespace TEN::Input;

namespace TEN::Entities::TR4
{
	void InitializeObelisk(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		item->Animation.AnimNumber = 3;
		item->Animation.FrameNumber = 0;

		AddActiveItem(itemNumber);
		item->Status = ITEM_ACTIVE;

		item->ItemFlags[2] = NO_VALUE;
		for (int i = 0; i < g_Level.NumItems; i++)
		{
			const auto* currentItem = &g_Level.Items[i];
			if (currentItem->ObjectNumber == ID_OBELISK_CAGE)
				item->ItemFlags[2] = i;
		}
	}

	void InitializeObeliskCage(short itemNumber)
	{
		g_Level.Items[itemNumber].ItemFlags[0] = 0;
	}

	bool AreObelisksAligned()
	{
		const ItemInfo* obelisks[3] = {};

		for (int i = 0; i < g_Level.NumItems; i++)
		{
			const auto& candidate = g_Level.Items[i];
			if (candidate.ObjectNumber != ID_OBELISK)
				continue;

			const auto ocb = candidate.TriggerFlags;
			if (ocb < 1 || ocb > 3 || obelisks[ocb - 1] != nullptr)
				return false;

			obelisks[ocb - 1] = &candidate;
		}

		return obelisks[0] != nullptr && obelisks[0]->Pose.Orientation.y == ANGLE(180.0f) &&
			obelisks[1] != nullptr && obelisks[1]->Pose.Orientation.y == 0 &&
			obelisks[2] != nullptr && obelisks[2]->Pose.Orientation.y == ANGLE(90.0f);
	}

	void ObeliskControl(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		auto pos = Vector3::Zero;
		auto pos2 = Vector3::Zero;

		if (TriggerActive(item))
		{
			if (item->ItemFlags[3] > 346)
				return;

			item->ItemFlags[3]++;

			unsigned char r = (GetRandomControl() & 0x1F) + 224;
			unsigned char g = r - (GetRandomControl() & 0x1F) - 32;
			unsigned char b = g - (GetRandomControl() & 0x1F) - 128;

			if (!(GlobalCounter & 1) && item->ItemFlags[2] != NO_VALUE)
			{
				pos.x = item->Pose.Position.x + 3456 * phd_sin(item->Pose.Orientation.y + ANGLE(90.0f));
				pos.y = item->Pose.Position.y - CLICK(1);
				pos.z = item->Pose.Position.z + 3456 * phd_cos(item->Pose.Orientation.y + ANGLE(90.0f));
				pos2 = g_Level.Items[item->ItemFlags[2]].GetObb().Center;

				if (!(GlobalCounter & 3))
					SpawnElectricity(pos, pos2, (GetRandomControl() & 0x1F) + 32, r, g, b, 24, 1, 32, 5);

				SpawnElectricityGlow(pos, 48, r, g, b);
			}

			if (item->ItemFlags[3] >= 256 && item->TriggerFlags == 2 && item->ItemFlags[2] != NO_VALUE)
			{
				auto* cage = &g_Level.Items[item->ItemFlags[2]];
				SoundEffect(SFX_TR4_ELECTRIC_ARCING_LOOP, &cage->Pose);

				if (item->ItemFlags[3] >= 346 && (GlobalCounter & 1) && !cage->ItemFlags[0])
				{
					cage->ItemFlags[0] = 1;
					cage->Status = ITEM_ACTIVE;
					AddActiveItem(cage->Index);
				}
			}
		}
		else
		{
			AnimateItem(item);
			const auto animationNumber = item->Animation.AnimNumber;
			const auto frameNumber = item->Animation.FrameNumber;

			auto* obj = &Objects[item->ObjectNumber];
			bool flag = false;

			if (item->Animation.AnimNumber == 2)
			{
				item->Pose.Orientation.y -= ANGLE(90.0f);
				item->DisableInterpolation = true;
				item->DisableAnimBlend();

				if (IsHeld(In::Action))
				{
					item->Animation.AnimNumber = 1;
					item->Animation.FrameNumber = 0;
				}
				else
					flag = true;
			}

			if (item->Animation.AnimNumber == 6)
			{
				item->Pose.Orientation.y += ANGLE(90.0f);
				item->DisableInterpolation = true;
				item->DisableAnimBlend();

				if (!IsHeld(In::Action))
				{
					item->Animation.AnimNumber = 3;
					item->Animation.FrameNumber = 0;
					flag = false;
				}
				else
				{
					item->Animation.AnimNumber = 5;
					item->Animation.FrameNumber = 0;
				}
			}

			if (flag)
			{
				item->Animation.AnimNumber = 3;
				item->Animation.FrameNumber = 0;
			}

           if (item->Animation.AnimNumber != animationNumber || item->Animation.FrameNumber != frameNumber)
				TEN::Renderer::g_Renderer.UpdateItemAnimations(item->Index, true);

			if (item->TriggerFlags == 2)
			{
				for (int i = 0; i < g_Level.NumItems; i++)
				{
					auto& currentItem = g_Level.Items[i];
					if (currentItem.ObjectNumber == ID_PULLEY)
					{
						currentItem.ItemFlags[1] = AreObelisksAligned() ? 0 : 1;
						break;
					}
				}
			}
		}
	}

	void ObeliskCageControl(short itemNumber)
	{
		auto* cage = &g_Level.Items[itemNumber];
		if (!cage->ItemFlags[0])
			return;

		const auto cagePosition = cage->Pose.Position;
		const auto cageCenter = cage->GetObb().Center;
		const auto cageRoom = cage->RoomNumber;
		ExplodeItemNode(cage, 0, 0, 128);
		SoundEffect(SFX_TR4_EXPLOSION1, &cage->Pose);
		SoundEffect(SFX_TR4_EXPLOSION2, &cage->Pose);
		KillItem(itemNumber);

		TriggerExplosionSparks((int)cageCenter.x, (int)cageCenter.y, (int)cageCenter.z, 3, -2, 0, cageRoom);
		TriggerExplosionSparks((int)cageCenter.x, (int)cageCenter.y, (int)cageCenter.z, 3, -1, 0, cageRoom);

		for (int i = 0; i < g_Level.NumItems; i++)
		{
			auto& item = g_Level.Items[i];
			if (item.ObjectNumber == ID_OBELISK)
				item.ItemFlags[2] = NO_VALUE;
		}

		for (int i = 0; i < g_Level.NumItems; i++)
		{
			auto& item = g_Level.Items[i];
			if (i == itemNumber || item.Status != ITEM_INVISIBLE)
				continue;

			if (item.Pose.Position.x / BLOCK(1) == cagePosition.x / BLOCK(1) &&
				item.Pose.Position.z / BLOCK(1) == cagePosition.z / BLOCK(1))
				item.Status = ITEM_NOT_ACTIVE;
		}
	}
}
