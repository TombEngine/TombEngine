#include "framework.h"
#include "Objects/TR3/Emitter/tr3_bats_emitter.h"

#include "Game/collision/collide_room.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Setup.h"
#include "Sound/sound.h"
#include "Specific/level.h"

namespace TEN::Entities::TR3
{
	constexpr auto TR3_BAT_LIFE_MIN	  = 144;
	constexpr auto TR3_BAT_SPEED_MIN	  = 64;
	constexpr auto TR3_BAT_SPEED_MAX	  = 300;
	constexpr auto TR3_BAT_SPEED_ACCEL = 12;
	constexpr auto TR3_BAT_SPEED_DIVISOR = 6;

	Tr3BatData Tr3Bats[NUM_TR3_BATS];

	static Tr3BatData* GetFreeTr3Bat()
	{
		for (auto& bat : Tr3Bats)
		{
			if (!bat.On)
				return &bat;
		}

		return nullptr;
	}

	void ClearTr3Bats()
	{
		for (auto& bat : Tr3Bats)
		{
			bat = {};
			bat.RoomNumber = NO_VALUE;
		}
	}

	void InitializeTr3BatsEmitter(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		item.TriggerFlags = 0;

		if (Objects[ID_BATS_EMITTER_TR3].loaded)
			ClearTr3Bats();
	}

	void TriggerTr3Bats(ItemInfo* item)
	{
		short angle = (item->Pose.Orientation.y >> 4) & 0xFFF;

		for (int i = 0; i < TR3_BATS_PER_EMITTER; i++)
		{
			auto* bat = GetFreeTr3Bat();

			if (!bat)
				return;

			bat->RoomNumber = item->RoomNumber;
			bat->Pose.Position.x = (GetRandomControl() & 0x1FF) + item->Pose.Position.x - 256;
			bat->Pose.Position.y = item->Pose.Position.y - (GetRandomControl() & 0xFF) + 256;
			bat->Pose.Position.z = (GetRandomControl() & 0x1FF) + item->Pose.Position.z - 256;
			bat->Pose.Orientation.x = 0;
			bat->Pose.Orientation.y = (((GetRandomControl() & 0x7F) + angle - 64) & 0xFFF) << 4;
			bat->Pose.Orientation.z = 0;
			bat->Velocity = (GetRandomControl() & 0x1F) + TR3_BAT_SPEED_MIN;
			bat->Counter = (GetRandomControl() & 7) + TR3_BAT_LIFE_MIN;
			bat->FrameOffset = (int)(bat - Tr3Bats) * 5;
			bat->On = true;

			auto translation = Matrix::CreateTranslation(bat->Pose.Position.ToVector3());
			bat->Transform = bat->Pose.Orientation.ToRotationMatrix() * translation;
			bat->PrevTransform = bat->Transform;
		}
	}

	void Tr3BatsEmitterControl(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!TriggerActive(&item))
			return;

		TriggerTr3Bats(&item);
		KillItem(itemNumber);
	}

	void UpdateTr3Bats()
	{
		if (!Objects[ID_BATS_EMITTER_TR3].loaded)
			return;

		for (int i = 0; i < NUM_TR3_BATS; i++)
		{
			auto& bat = Tr3Bats[i];

			if (!bat.On)
				continue;

			bat.StoreInterpolationData();

			if (!(i & 3) && !(GetRandomControl() & 7))
				SoundEffect(SFX_TR4_BATS, &bat.Pose);

			int velocity = bat.Velocity / TR3_BAT_SPEED_DIVISOR;

			bat.Pose.Position.x += velocity * phd_sin(bat.Pose.Orientation.y);
			bat.Pose.Position.y -= GetRandomControl() & 3;
			bat.Pose.Position.z += velocity * phd_cos(bat.Pose.Orientation.y);

			if (bat.Counter < 128)
			{
				bat.Pose.Position.y += -4 - (i >> 1);

				if (!(GetRandomControl() & 3))
				{
					bat.Pose.Orientation.y += ((GetRandomControl() & 0xFF) - 128) << 4;
					bat.Velocity += GetRandomControl() & 3;
				}
			}

			bat.Velocity += TR3_BAT_SPEED_ACCEL;

			if (bat.Velocity > TR3_BAT_SPEED_MAX)
				bat.Velocity = TR3_BAT_SPEED_MAX;

			if (bat.Counter && (Wibble & 4))
			{
				bat.Counter--;

				if (!bat.Counter)
					bat.On = false;
			}

			GetFloor(bat.Pose.Position.x, bat.Pose.Position.y, bat.Pose.Position.z, &bat.RoomNumber);

			auto translation = Matrix::CreateTranslation(bat.Pose.Position.ToVector3());
			bat.Transform = bat.Pose.Orientation.ToRotationMatrix() * translation;
		}
	}
}
