#include "framework.h"
#include "Objects/TR5/Light/tr5_light.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_room.h"
#include "Game/control/los.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Math/Math.h"
#include "Objects/TR5/Light/tr5_light_info.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Math;

static ElectricalLightInfo& GetElectricalLightInfo(ItemInfo& item)
{
	return *(ElectricalLightInfo*)item.Data;
}

void PulseLightControl(short itemNumber)
{
	auto* item = &g_Level.Items[itemNumber];

	if (TriggerActive(item))
	{
		item->ItemFlags[0] -= 1024;

		long pulse = 256 * phd_sin(item->ItemFlags[0] + ((item->Pose.Position.y & 0x3FFF) * 4));
		pulse = abs(pulse);
		if (pulse > 255)
			pulse = 255;

		SpawnDynamicLight(
			item->Pose.Position.x,
			item->Pose.Position.y,
			item->Pose.Position.z,
			24,
			(pulse * item->Model.Color.x * SCHAR_MAX) / 512,
			(pulse * item->Model.Color.y * SCHAR_MAX) / 512,
			(pulse * item->Model.Color.z * SCHAR_MAX) / 512);
	}
}

void TriggerAlertLight(int x, int y, int z, int r, int g, int b, short angle, short roomNumber, int falloff)
{
	GetFloor(x, y, z, &roomNumber);

	auto origin = GameVector(x, y, z, roomNumber);
	auto target =  GameVector(Geometry::TranslatePoint(origin.ToVector3(), angle * 16, BLOCK(16)));

	if (!LOS(&origin, &target))
		SpawnDynamicLight(target.x, target.y, target.z, falloff, r, g, b);
}

void StrobeLightControl(short itemNumber)
{
	auto* item = &g_Level.Items[itemNumber];

	if (TriggerActive(item))
	{
		item->Pose.Orientation.y += ANGLE(16.0f);

		byte r = item->Model.Color.x * SCHAR_MAX;
		byte g = item->Model.Color.y * SCHAR_MAX;
		byte b = item->Model.Color.z * SCHAR_MAX;

		TriggerAlertLight(
			item->Pose.Position.x,
			item->Pose.Position.y - CLICK(2),
			item->Pose.Position.z,
			r, g, b,
			((item->Pose.Orientation.y + 22528) / 16) & 0xFFF,
			item->RoomNumber,
			12);

		SpawnDynamicLight(
			item->Pose.Position.x + 256 * phd_sin(item->Pose.Orientation.y + 22528),
			item->Pose.Position.y - 768,
			item->Pose.Position.z + 256 * phd_cos(item->Pose.Orientation.y + 22528),
			8,
			r, g, b);
	}
}

void ColorLightControl(short itemNumber)
{
	auto* item = &g_Level.Items[itemNumber];

	if (TriggerActive(item))
	{
		SpawnDynamicLight(
			item->Pose.Position.x,
			item->Pose.Position.y,
			item->Pose.Position.z,
			24,
			item->Model.Color.x * SCHAR_MAX,
			item->Model.Color.y * SCHAR_MAX,
			item->Model.Color.z * SCHAR_MAX);
	}
}

void InitializeElectricalLight(short itemNumber)
{
	auto& item = g_Level.Items[itemNumber];
	item.Data = ElectricalLightInfo();
	auto& lightPtr = GetElectricalLightInfo(item);

	lightPtr.Color = item.Model.Color;
	item.MeshBits.ClearAll();
}

void ElectricalLightControl(short itemNumber)
{
	auto* item = &g_Level.Items[itemNumber];
	auto* lightPtr = &GetElectricalLightInfo(*item);

	if (!TriggerActive(item))
	{
		item->ItemFlags[0] = 0;
		item->MeshBits.Clear(abs(item->TriggerFlags));
		return;
	}

	int intensity = 0;
	
	// NOTE: Positive TriggerFlags allows light to behave like a neon light. Negative OCB makes the light flicker.
	if (item->TriggerFlags > 0)
	{
		item->MeshBits.Set(item->TriggerFlags);

		if (item->ItemFlags[0] < 16)
		{
			intensity = (GetRandomControl() & 0x3F) * 4;
			item->ItemFlags[0]++;
		}
		else if (item->ItemFlags[0] >= 96)
		{
			if (item->ItemFlags[0] >= 160)
			{
				intensity = 255 - (GetRandomControl() & 0x1F);				
			}
			else
			{
				intensity = 96 - (GetRandomControl() & 0x1F);
				if (!(GetRandomControl() & 0x1F) && item->ItemFlags[0] > 128)
					item->ItemFlags[0] = 160;
				else
					item->ItemFlags[0]++;
			}
		}
		else
		{
			if (Wibble & 0x3F && GetRandomControl() & 7)
			{
				intensity = GetRandomControl() & 0x3F;
				item->ItemFlags[0]++;
			}
			else
			{
				intensity = 192 - (GetRandomControl() & 0x3F);
				item->ItemFlags[0]++;
			}
		}
	}
	else
	{
		item->MeshBits.Set(abs(item->TriggerFlags));

		if (item->ItemFlags[0] <= 0)
		{
			item->ItemFlags[0] = Random::GenerateInt(4, 8);
			item->ItemFlags[1] = Random::GenerateInt(128, 256);
			item->ItemFlags[2] = Random::GenerateInt(0, 1);
		}

		item->ItemFlags[0]--;

		if (!item->ItemFlags[2])
		{
			item->ItemFlags[0]--;

			intensity = item->ItemFlags[1] - Random::GenerateInt(0, 128);
			if (intensity > 64)
				SoundEffect(SFX_TR5_ELECTRIC_LIGHT_CRACKLES, &item->Pose, SoundEnvironment::Land, 1.0f, intensity / 192.0f);
		}
		else
		{
			intensity = 0;
			item->Model.Color = Vector4::Zero;
			return;
		}
	}

	SpawnDynamicLight(
		item->Pose.Position.x,
		item->Pose.Position.y,
		item->Pose.Position.z,
		24,
		(intensity * (lightPtr->Color.x / 2)),
		(intensity * (lightPtr->Color.y / 2)) ,
		(intensity * (lightPtr->Color.z / 2)));

	// Set light mesh color. Model.Color max value is 2.0f.
	item->Model.Color = Vector4(
		((intensity / 2) * lightPtr->Color.x) / 96,
		((intensity / 2) * lightPtr->Color.y) / 96,
		((intensity / 2) * lightPtr->Color.z) / 96,
		1.0f);
}

void BlinkingLightControl(short itemNumber)
{
	auto* item = &g_Level.Items[itemNumber];

	if (TriggerActive(item))
	{
		item->ItemFlags[0]--;

		if (item->ItemFlags[0] >= 3)
		{
			item->MeshBits = 1;
		}
		else
		{
			auto pos = GetJointPosition(item, 0);

			SpawnDynamicLight(
				pos.x, pos.y, pos.z,
				16,
				item->Model.Color.x * SCHAR_MAX,
				item->Model.Color.y * SCHAR_MAX,
				item->Model.Color.z * SCHAR_MAX);

			item->MeshBits = 2;

			if (item->ItemFlags[0] < 0)
				item->ItemFlags[0] = 30;
		}
	}
}
