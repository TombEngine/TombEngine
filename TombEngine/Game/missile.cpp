#include "framework.h"
#include "Game/missile.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/control/box.h"
#include "Game/effects/Decal.h"
#include "Game/effects/tomb4fx.h"
#include "Game/effects/effects.h"
#include "Game/effects/explosion.h"
#include "Game/effects/Light.h"
#include "Game/effects/Bubble.h"
#include "Game/Lara/lara.h"
#include "Game/items.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;
using namespace TEN::Effects::Bubble;
using namespace TEN::Effects::Decal;
using namespace TEN::Effects::Explosion;
using namespace TEN::Effects::Light;
using namespace TEN::Math;

constexpr auto MUTANT_SHARD_DAMAGE	= 30;
constexpr auto MUTANT_BOMB_DAMAGE	= 100;
constexpr auto DIVER_HARPOON_DAMAGE = 50;
constexpr auto KNIFE_DAMAGE			= 50;

void ShootAtEnemy(Vector3i target, ItemInfo* item, int index)
{
	constexpr auto RANDOM_ORIENT = 1.4f;

	if (item == nullptr || index <= NO_VALUE)
		return;

	// If target deviated too much from the item position, prefer item's own position.
	if (Vector3i::Distance(item->Pose.Position, target) > TARGET_DEVIATION_THRESHOLD)
		target = item->Pose.Position;

	auto targetWithOffset = Vector3(
		target.x,
		target.y - (GameBoundingBox(item).GetHeight() * 0.75f),
		target.z);

	// Apply slight random scatter.
	auto randomOrient = EulerAngles(
		Random::GenerateAngle(ANGLE(-RANDOM_ORIENT), ANGLE(RANDOM_ORIENT)),
		Random::GenerateAngle(ANGLE(-RANDOM_ORIENT), ANGLE(RANDOM_ORIENT)),
		0);

	auto& fx = g_Level.Items[index];
	fx.Pose.Orientation = Geometry::GetOrientToPoint(fx.Pose.Position.ToVector3(), targetWithOffset);
	fx.Pose.Orientation += randomOrient;
}

// TODO: Make ControlMissile() not use LaraItem global. -- TokyoSU 5/8/2022
void ControlMissile(short fxNumber)
{
	static const int hitRadius = 200;

	// TODO: Add fx.target (ItemInfo* target) to get the actual target (if it was really it).
	auto& fx = g_Level.Items[fxNumber];
	auto& fxInfo = GetFXInfo(fx);

	auto isUnderwater = TestEnvironment(ENV_FLAG_WATER, fx.RoomNumber);
	auto soundFXType = isUnderwater ? SoundEnvironment::Underwater : SoundEnvironment::Land;

	// Make harpoon arch downwards if not underwater.
	if (fx.ObjectNumber == ID_SCUBA_HARPOON && !isUnderwater && fx.Pose.Orientation.x > ANGLE(-67.5f))
		fx.Pose.Orientation.x -= ANGLE(1.0f);

	fx.Pose.Translate(fx.Pose.Orientation, fx.Animation.Velocity.z);

	auto pointColl = GetPointCollision(fx.Pose.Position, fx.RoomNumber);
	auto hasHitPlayer = ItemNearLara(fx.Pose.Position, hitRadius);

	// Check whether something was hit.
	if (fx.Pose.Position.y >= pointColl.GetFloorHeight() ||
		fx.Pose.Position.y <= pointColl.GetCeilingHeight() ||
		pointColl.IsWall() ||
		hasHitPlayer)
	{
		if (fx.ObjectNumber == ID_KNIFETHROWER_KNIFE ||
			fx.ObjectNumber == ID_SCUBA_HARPOON ||
			fx.ObjectNumber == ID_PROJ_SHARD)
		{
			SoundEffect((fx.ObjectNumber == ID_SCUBA_HARPOON) ? SFX_TR4_WEAPON_RICOCHET : SFX_TR2_CIRCLE_BLADE_HIT, &fx.Pose, soundFXType);
		}
		else if (fx.ObjectNumber == ID_PROJ_BOMB)
		{
			SpawnDecal(fx.Pose.Position.ToVector3(), fx.RoomNumber, DecalType::Explosion);
			SoundEffect(SFX_TR1_ATLANTEAN_EXPLODE, &fx.Pose, soundFXType);
			TriggerExplosionSparks(fx.Pose.Position.x, fx.Pose.Position.y, fx.Pose.Position.z, 3, -2, 0, fx.RoomNumber);
			TriggerExplosionSparks(fx.Pose.Position.x, fx.Pose.Position.y, fx.Pose.Position.z, 3, -1, 0, fx.RoomNumber);
			TriggerShockwave(&fx.Pose, 48, 304, (GetRandomControl() & 0x1F) + 112, 128, 32, 32, 32, EulerAngles(2048, 0.0f, 0.0f), 0, true, false, false, (int)ShockwaveStyle::Normal);
		}

		if (hasHitPlayer)
		{
			if (fx.ObjectNumber == ID_KNIFETHROWER_KNIFE)
			{
				DoDamage(LaraItem, KNIFE_DAMAGE);
			}
			else if (fx.ObjectNumber == ID_SCUBA_HARPOON)
			{
				DoDamage(LaraItem, DIVER_HARPOON_DAMAGE);
			}
			else if (fx.ObjectNumber == ID_PROJ_BOMB)
			{
				DoDamage(LaraItem, MUTANT_BOMB_DAMAGE);
			}
			else if (fx.ObjectNumber == ID_PROJ_SHARD)
			{
				TriggerBlood(fx.Pose.Position.x, fx.Pose.Position.y, fx.Pose.Position.z, 0, 10);
				SoundEffect(SFX_TR4_BLOOD_LOOP, &fx.Pose, soundFXType);
				DoDamage(LaraItem, MUTANT_SHARD_DAMAGE);
			}

			LaraItem->HitStatus = true;
			fx.Pose.Orientation.y = LaraItem->Pose.Orientation.y;
			fx.Animation.Velocity.z = LaraItem->Animation.Velocity.z;
			fx.Animation.FrameNumber = 0;
			fxInfo.Counter = 0;
		}

		KillItem(fxNumber);
		return;
	}

	if (pointColl.GetRoomNumber() != fx.RoomNumber)
		ItemNewRoom(fxNumber, pointColl.GetRoomNumber());

	if (fx.ObjectNumber == ID_KNIFETHROWER_KNIFE)
		fx.Pose.Orientation.z += ANGLE(30.0f); // Update knife rotation over time.

	switch (fx.ObjectNumber)
	{
	case ID_SCUBA_HARPOON:
		if (TestEnvironment(RoomEnvFlags::ENV_FLAG_WATER, fx.RoomNumber))
			SpawnBubble(fx.Pose.Position.ToVector3(), fx.RoomNumber);

		break;

	case ID_PROJ_BOMB:
		SpawnDynamicLight(fx.Pose.Position.x, fx.Pose.Position.y, fx.Pose.Position.z, 14, 180, 100, 0);
		break;
	}
}

void ControlNatlaGun(short fxNumber)
{
	auto& fx = g_Level.Items[fxNumber];
	auto& fxInfo = GetFXInfo(fx);

	fx.Animation.FrameNumber--;
	if (fx.Animation.FrameNumber <= Objects[fx.ObjectNumber].nmeshes)
		KillItem(fxNumber);

	// If first frame, start another explosion at next position.
	if (fx.Animation.FrameNumber == -1)
	{
		auto pointColl = GetPointCollision(fx.Pose.Position, fx.RoomNumber, fx.Pose.Orientation.y, fx.Animation.Velocity.y);

		// Don't create one if hit a wall.
		if (pointColl.GetPosition().y >= pointColl.GetFloorHeight() ||
			pointColl.GetPosition().y <= pointColl.GetCeilingHeight())
		{
			return;
		}

		fxNumber = CreateNewEffect(pointColl.GetRoomNumber(), ID_PROJ_BOMB, fx.Pose.Position);
		if (fxNumber != NO_VALUE)
		{
			auto& fxNew = g_Level.Items[fxNumber];

			fxNew.Pose.Position = pointColl.GetPosition();
			fxNew.Pose.Orientation.y = fx.Pose.Orientation.y;
			fxNew.RoomNumber = pointColl.GetRoomNumber();
			fxNew.Animation.Velocity.z = fx.Animation.Velocity.z;
			fxNew.Animation.FrameNumber = 0;
			fxNew.ObjectNumber = ID_PROJ_BOMB;
		}
	}
}

short ShardGun(int x, int y, int z, short velocity, short yRot, short roomNumber)
{
	int fxNumber = CreateNewEffect(roomNumber, ID_PROJ_SHARD, Pose(Vector3i(x, y, z), EulerAngles(0, yRot, 0)));
	if (fxNumber != NO_VALUE)
	{
		auto& fx = g_Level.Items[fxNumber];

		fx.RoomNumber = roomNumber;
		fx.Animation.Velocity.z = velocity;
		fx.Animation.FrameNumber = 0;
		fx.Model.Color = Vector4::One;
	}

	return fxNumber;
}

short BombGun(int x, int y, int z, short velocity, short yRot, short roomNumber)
{
	int fxNumber = CreateNewEffect(roomNumber, ID_PROJ_BOMB, Pose(Vector3i(x, y, z), EulerAngles(0, yRot, 0)));
	if (fxNumber != NO_VALUE)
	{
		auto& fx = g_Level.Items[fxNumber];

		fx.RoomNumber = roomNumber;
		fx.Animation.Velocity.z = velocity;
		fx.Animation.FrameNumber = 0;
		fx.Model.Color = Vector4::One;
	}

	return fxNumber;
}

short HarpoonGun(int x, int y, int z, short velocity, short yRot, short roomNumber)
{
	int fxNumber = CreateNewEffect(roomNumber, ID_SCUBA_HARPOON, Pose(Vector3i(x, y, z), EulerAngles(0, yRot, 0)));
	if (fxNumber != NO_VALUE)
	{
		auto& fx = g_Level.Items[fxNumber];

		fx.RoomNumber = roomNumber;
		fx.Animation.Velocity.z = velocity;
		fx.Animation.FrameNumber = 0;
		fx.Model.Color = Vector4::One;
	}

	return fxNumber;
}