#include "framework.h"
#include "Objects/TR5/Emitter/tr5_bats_emitter.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Math;

constexpr auto TR5_BAT_LARA_DAMAGE    = 2;
constexpr auto TR5_BAT_FLYOFF_TIMEOUT = 90;
constexpr auto TR3_BAT_COUNT          = 32;
constexpr auto TR3_BAT_LIFE_MIN       = 144;
constexpr auto TR3_BAT_SPEED_MIN      = 64;
constexpr auto TR3_BAT_SPEED_MAX      = 300;
constexpr auto TR3_BAT_SPEED_ACCEL    = 12;
constexpr auto TR3_BAT_SPEED_DIVISOR  = 6;
constexpr auto TR3_BAT_SCALE          = 4.0f / 3.0f;

enum BatsEmitterMode
{
	BATS_EMITTER_MODE_TR3_FLYAWAY = 0,
	BATS_EMITTER_MODE_TR5_ATTACK
};

int NextBat;
int BatsAnimFrameOffset = 0;
BatData Bats[NUM_BATS];
BatData Tr3Bats[NUM_BATS];

void UpdateBatTransform(BatData* bat)
{
	Matrix translation = Matrix::CreateTranslation(bat->Pose.Position.x, bat->Pose.Position.y, bat->Pose.Position.z);
	Matrix rotation = bat->Pose.Orientation.ToRotationMatrix();
	bat->Transform = rotation * translation;
}

void ResetBatInterpolationData(BatData* bat)
{
	UpdateBatTransform(bat);
	bat->PrevTransform = bat->Transform;
}

void ClearTr3Bats()
{
	ZeroMemory(Tr3Bats, NUM_BATS * sizeof(BatData));

	for (int i = 0; i < NUM_BATS; i++)
		ResetBatInterpolationData(&Tr3Bats[i]);
}

void InitializeTr3BatsEmitter(ItemInfo* item)
{
	item->TriggerFlags = 0;
}

void TriggerTr3Bats(ItemInfo* item)
{
	short angle = ((item->Pose.Orientation.y >> 4) - 1024) & 0xFFF;

	for (int i = 0; i < TR3_BAT_COUNT; i++)
	{
		auto* bat = &Tr3Bats[i];

		bat->RoomNumber = item->RoomNumber;
		bat->Pose.Position.x = (GetRandomControl() & 0x1FF) + item->Pose.Position.x - 256;
		bat->Pose.Position.y = item->Pose.Position.y - (GetRandomControl() & 0xFF) + 256;
		bat->Pose.Position.z = (GetRandomControl() & 0x1FF) + item->Pose.Position.z - 256;
		bat->Pose.Orientation.x = 0;
		bat->Pose.Orientation.y = (((GetRandomControl() & 0x7F) + angle - 64) & 0xFFF) << 4;
		bat->Pose.Orientation.z = 0;
		bat->Velocity = (GetRandomControl() & 0x1F) + TR3_BAT_SPEED_MIN;
		bat->Counter = (GetRandomControl() & 7) + TR3_BAT_LIFE_MIN;
		bat->LaraTarget = 0;
		bat->XTarget = 0;
		bat->ZTarget = 0;
		bat->On = true;
		bat->Flags = 0;
		ResetBatInterpolationData(bat);
	}
}

void ControlTr3BatsEmitter(short itemNumber, ItemInfo* item)
{
	TriggerTr3Bats(item);
	KillItem(itemNumber);
}

void UpdateTr3Bat(BatData* bat, int index)
{
	static const Matrix tr3BatScaleMatrix = Matrix::CreateScale(TR3_BAT_SCALE);

	if (!(index & 3) && !(GetRandomControl() & 7))
		SoundEffect(SFX_TR4_BATS, &bat->Pose);

	int velocity = bat->Velocity / TR3_BAT_SPEED_DIVISOR;

	bat->Pose.Position.x -= velocity * phd_cos(bat->Pose.Orientation.y);
	bat->Pose.Position.y -= GetRandomControl() & 3;
	bat->Pose.Position.z += velocity * phd_sin(bat->Pose.Orientation.y);

	if (bat->Counter < 128)
	{
		bat->Pose.Position.y += -4 - (index >> 1);

		if (!(GetRandomControl() & 3))
		{
			bat->Pose.Orientation.y += ((GetRandomControl() & 0xFF) - 128) << 4;
			bat->Velocity += GetRandomControl() & 3;
		}
	}

	bat->Velocity += TR3_BAT_SPEED_ACCEL;

	if (bat->Velocity > TR3_BAT_SPEED_MAX)
		bat->Velocity = TR3_BAT_SPEED_MAX;

	if (bat->Counter && (Wibble & 4))
	{
		bat->Counter--;

		if (!bat->Counter)
			bat->On = false;
	}

	GetFloor(bat->Pose.Position.x, bat->Pose.Position.y, bat->Pose.Position.z, &bat->RoomNumber);

	EulerAngles orient = bat->Pose.Orientation;
	orient.y -= ANGLE(90.0f);

	Matrix translation = Matrix::CreateTranslation(bat->Pose.Position.x, bat->Pose.Position.y, bat->Pose.Position.z);
	Matrix rotation = orient.ToRotationMatrix();
	bat->Transform = tr3BatScaleMatrix * rotation * translation;
}

void UpdateTr3Bats()
{
	for (int i = 0; i < TR3_BAT_COUNT; i++)
	{
		auto* bat = &Tr3Bats[i];

		if (!bat->On)
			continue;

		bat->StoreInterpolationData();
		UpdateTr3Bat(bat, i);
	}
}

void ClearTr5Bats()
{
	ZeroMemory(Bats, NUM_BATS * sizeof(BatData));
	NextBat = 0;

	for (int i = 0; i < NUM_BATS; i++)
		ResetBatInterpolationData(&Bats[i]);
}

void ClearBats()
{
	ClearTr3Bats();
	ClearTr5Bats();
}

void InitializeTr5BatsEmitter(ItemInfo* item)
{
	if (item->Pose.Orientation.y == 0)
		item->Pose.Position.z += CLICK(2);
	else if (item->Pose.Orientation.y == -ANGLE(180.0f))
		item->Pose.Position.z -= CLICK(2);
	else if (item->Pose.Orientation.y == -ANGLE(90.0f))
		item->Pose.Position.x -= CLICK(2);
	else if (item->Pose.Orientation.y == ANGLE(90.0f))
		item->Pose.Position.x += CLICK(2);
}

void InitializeLittleBats(short itemNumber)
{
	auto* item = &g_Level.Items[itemNumber];

	item->ItemFlags[0] = item->TriggerFlags > 0 ? BATS_EMITTER_MODE_TR5_ATTACK : BATS_EMITTER_MODE_TR3_FLYAWAY;
	item->ItemFlags[1] = 0;

	if (Objects[ID_BATS_EMITTER].loaded)
		ClearBats();

	switch (item->ItemFlags[0])
	{
	case BATS_EMITTER_MODE_TR3_FLYAWAY:
		InitializeTr3BatsEmitter(item);
		break;

	default:
		InitializeTr5BatsEmitter(item);
		break;
	}
}

void ControlTr5BatsEmitter(short itemNumber, ItemInfo* item)
{
	if (item->TriggerFlags)
	{
		TriggerLittleBat(item);
		item->TriggerFlags--;
		return;
	}

	KillItem(itemNumber);
}

void LittleBatsControl(short itemNumber)
{
	auto* item = &g_Level.Items[itemNumber];

	if (!TriggerActive(item))
		return;

	if (!item->ItemFlags[1])
	{
		BatsAnimFrameOffset = GetRandomControl() & 3;
		item->ItemFlags[1] = 1;
	}

	switch (item->ItemFlags[0])
	{
	case BATS_EMITTER_MODE_TR3_FLYAWAY:
		ControlTr3BatsEmitter(itemNumber, item);
		break;

	default:
		ControlTr5BatsEmitter(itemNumber, item);
		break;
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
			bat = &Bats[0];
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
		ResetBatInterpolationData(bat);
	}
}

void UpdateTr5Bat(BatData* bat, int index, const Vector3i* laraPos, EffectType laraEffectType, long long* minDistance, int* minIndex)
{
	if ((laraEffectType != EffectType::None || LaraItem->HitPoints <= 0) &&
		bat->Counter > TR5_BAT_FLYOFF_TIMEOUT &&
		!(GetRandomControl() & 7))
	{
		bat->Counter = TR5_BAT_FLYOFF_TIMEOUT;
	}

	if (!(--bat->Counter))
	{
		bat->On = false;
		return;
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
			laraPos->x + bat->XTarget * 8,
			laraPos->y - bat->LaraTarget,
			laraPos->z + bat->ZTarget * 8
		));

	int x = laraPos->x - bat->Pose.Position.x;
	int z = laraPos->z - bat->Pose.Position.z;
	long long distanceSq = (long long)x * x + (long long)z * z;

	if (distanceSq < *minDistance)
	{
		*minDistance = distanceSq;
		*minIndex = index;
	}

	int distance = (int)(sqrt((double)distanceSq) / 8.0);
	if (distance < 48)
		distance = 48;
	else if (distance > 128)
		distance = 128;

	if (bat->Velocity < distance)
		bat->Velocity++;
	else if (bat->Velocity > distance)
		bat->Velocity--;

	if (bat->Counter > TR5_BAT_FLYOFF_TIMEOUT)
	{
		short velocity = bat->Velocity * 128;

		short xAngle = Geometry::GetShortestAngle(bat->Pose.Orientation.x, angles.x) / 8;
		short yAngle = Geometry::GetShortestAngle(bat->Pose.Orientation.y, angles.y) / 8;

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

	if (ItemNearTarget(bat->Pose.Position, LaraItem, CLICK(1)))
	{
		TriggerBlood(bat->Pose.Position.x, bat->Pose.Position.y, bat->Pose.Position.z, 2 * GetRandomControl(), 2);

		if (LaraItem->HitPoints > 0)
			DoDamage(LaraItem, TR5_BAT_LARA_DAMAGE);
	}

	GetFloor(bat->Pose.Position.x, bat->Pose.Position.y, bat->Pose.Position.z, &bat->RoomNumber);
	UpdateBatTransform(bat);
}

void UpdateBats()
{
	if (!Objects[ID_BATS_EMITTER].loaded)
		return;

	UpdateTr3Bats();

	if (!LaraItem)
		return;

	const Vector3i laraPos = LaraItem->Pose.Position;
	EffectType laraEffectType = LaraItem->Effect.Type;

	long long minDistance = INT64_MAX;
	int minIndex = NO_VALUE;

	for (int i = 0; i < NUM_BATS; i++)
	{
		auto* bat = &Bats[i];

		if (!bat->On)
			continue;

		bat->StoreInterpolationData();
		UpdateTr5Bat(bat, i, &laraPos, laraEffectType, &minDistance, &minIndex);
	}

	if (minIndex != NO_VALUE)
	{
		auto* bat = &Bats[minIndex];

		if (!(GetRandomControl() & 4))
			SoundEffect(SFX_TR4_BATS, &bat->Pose);
	}
}
