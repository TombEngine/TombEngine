#include "framework.h"
#include "Objects/TR5/Emitter/tr5_bats_emitter.h"

#include "Game/animation.h"
#include "Game/collision/collide_item.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Math;

constexpr auto BAT_LARA_DAMAGE = 2;

int NextBat;
BatData Bats[NUM_BATS];

void InitializeLittleBats(short itemNumber)
{
	auto* item = &g_Level.Items[itemNumber];

	if (item->Pose.Orientation.y == 0)
		item->Pose.Position.z += CLICK(2);
	else if (item->Pose.Orientation.y == -ANGLE(180.0f))
		item->Pose.Position.z -= CLICK(2);
	else if (item->Pose.Orientation.y == -ANGLE(90.0f))
		item->Pose.Position.x -= CLICK(2);
	else if (item->Pose.Orientation.y == ANGLE(90.0f))
		item->Pose.Position.x += CLICK(2);

	if (Objects[ID_BATS_EMITTER].loaded)
		ZeroMemory(Bats, NUM_BATS * sizeof(BatData));

	//LOWORD(item) = sub_402F27(ebx0, Bats, 0, 1920);
}

void LittleBatsControl(short itemNumber)
{
	auto* item = &g_Level.Items[itemNumber];

	if (TriggerActive(item))
	{
		if (item->TriggerFlags)
		{
			TriggerLittleBat(item);
			item->TriggerFlags--;
		}
		else
			KillItem(itemNumber);
	}
}

short GetNextBat()
{
	short batNumber = NextBat;
	auto* bat = &Bats[NextBat];

	int index = 0;
	while (bat->On)
	{
		if (batNumber == NUM_BATS - 1)
		{
			bat = (BatData*)Bats;
			batNumber = 0;
		}
		else
		{
			batNumber++;
			bat++;
		}

		index++;

		if (index >= NUM_BATS)
			return NO_VALUE;
	}

	NextBat = (batNumber + 1) & (NUM_BATS - 1);

	return batNumber;
}

void TriggerLittleBat(ItemInfo* item)
{
	short batNumber = GetNextBat();

	if (batNumber != NO_VALUE)
	{
		auto* bat = &Bats[batNumber];

		bat->RoomNumber = item->RoomNumber;
		bat->Pose.Position.x = item->Pose.Position.x;
		bat->Pose.Position.y = item->Pose.Position.y;
		bat->Pose.Position.z = item->Pose.Position.z;
		bat->Pose.Orientation.y = (GetRandomControl() & 0x7FF) + item->Pose.Orientation.y + -ANGLE(180.0f) - 1024;
		bat->On = true;
		bat->Flags = 0;
		bat->Pose.Orientation.x = (GetRandomControl() & 0x3FF) - 512;
		bat->Velocity = (GetRandomControl() & 0x1F) + 16;
		bat->LaraTarget = GetRandomControl() & 0x1FF;
		bat->Counter = 20 * ((GetRandomControl() & 7) + 15);
	}
}

void UpdateBats()
{
	if (!Objects[ID_BATS_EMITTER].loaded)
		return;

	int minDistance = INT_MAX;
	int minIndex = NO_VALUE;

	for (int i = 0; i < NUM_BATS; i++)
	{
		auto* bat = &Bats[i];

		if (!bat->On)
			continue;

		bat->StoreInterpolationData();

		if ((LaraItem->Effect.Type != EffectType::None || LaraItem->HitPoints <= 0) &&
			bat->Counter > 90 &&
			!(GetRandomControl() & 7))
		{
			bat->Counter = 90;
		}

		if (!(--bat->Counter))
		{
			bat->On = false;
			continue;
		}

		if (!(GetRandomControl() & 7))
		{
			bat->LaraTarget = GetRandomControl() % 640 + 128;
			bat->XTarget = (GetRandomControl() & 0x7F) - 64;
			bat->ZTarget = (GetRandomControl() & 0x7F) - 64;
		}

		auto angles = Geometry::GetOrientToPoint(
			bat->Pose.Position.ToVector3(),
			Vector3(
				LaraItem->Pose.Position.x + bat->XTarget * 8,
				LaraItem->Pose.Position.y - bat->LaraTarget,
				LaraItem->Pose.Position.z + bat->ZTarget * 8
			));

		int x = LaraItem->Pose.Position.x - bat->Pose.Position.x;
		int z = LaraItem->Pose.Position.z - bat->Pose.Position.z;
		int distance = pow(x, 2) + pow(z, 2);

		if (distance < minDistance)
		{
			minDistance = distance;
			minIndex = i;
		}

		distance = sqrt(distance) / 8;
		if (distance < 48)
			distance = 48;
		else if (distance > 128)
			distance = 128;

		if (bat->Velocity < distance)
			bat->Velocity++;
		else if (bat->Velocity > distance)
			bat->Velocity--;

		if (bat->Counter > 90)
		{
			short velocity = bat->Velocity * 128;

			short xAngle = abs(angles.x - bat->Pose.Orientation.x) / 8;
			short yAngle = abs(angles.y - bat->Pose.Orientation.y) / 8;

			if (xAngle < -velocity)
				xAngle = -velocity;
			else if (xAngle > velocity)
				xAngle = velocity;

			if (yAngle < -velocity)
				yAngle = -velocity;
			else if (yAngle > velocity)
				yAngle = velocity;

			bat->Pose.Orientation.x += xAngle;
			bat->Pose.Orientation.y += yAngle;
		}

		int sp = bat->Velocity * phd_cos(bat->Pose.Orientation.x);

		bat->Pose.Position.x += sp * phd_sin(bat->Pose.Orientation.y);
		bat->Pose.Position.y += bat->Velocity * phd_sin(-bat->Pose.Orientation.x);
		bat->Pose.Position.z += sp * phd_cos(bat->Pose.Orientation.y);

		if (!(i % 1) && ItemNearTarget(bat->Pose.Position, LaraItem, CLICK(1)))
		{
			TriggerBlood(bat->Pose.Position.x, bat->Pose.Position.y, bat->Pose.Position.z, 2 * GetRandomControl(), 2);
			DoDamage(LaraItem, BAT_LARA_DAMAGE);
		}

		Matrix translation = Matrix::CreateTranslation(bat->Pose.Position.x, bat->Pose.Position.y, bat->Pose.Position.z);
		Matrix rotation = bat->Pose.Orientation.ToRotationMatrix();
		bat->Transform = rotation * translation;
	}

	if (minIndex != NO_VALUE)
	{
		auto* bat = &Bats[minIndex];

		if (!(GetRandomControl() & 4))
			SoundEffect(SFX_TR4_BATS,&bat->Pose);
	}
}
