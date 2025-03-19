#include "framework.h"
#include "Game/missile.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
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
using namespace TEN::Collision::Point;;
using namespace TEN::Effects::Bubble;
using namespace TEN::Effects::Explosion;
using namespace TEN::Effects::Light;
using namespace TEN::Math;

constexpr auto MUTANT_SHARD_DAMAGE	= 30;
constexpr auto MUTANT_BOMB_DAMAGE	= 100;
constexpr auto DIVER_HARPOON_DAMAGE = 50;
constexpr auto KNIFE_DAMAGE			= 50;

void ShootAtLara(FX_INFO& fx)
{
	auto target = Vector3(
		LaraItem->Pose.Position.x,
		LaraItem->Pose.Position.y - (GameBoundingBox(LaraItem).GetHeight() * 0.75f),
		LaraItem->Pose.Position.z);
	fx.pos.Orientation = Geometry::GetOrientToPoint(fx.pos.Position.ToVector3(), target);

	// Apply slight random scatter.
	fx.pos.Orientation += EulerAngles(
		Random::GenerateAngle(ANGLE(-1.4f), ANGLE(1.4f)),
		Random::GenerateAngle(ANGLE(-1.4f), ANGLE(1.4f)),
		0);
}

// TODO: Make ControlMissile() not use LaraItem global. -- TokyoSU 5/8/2022
void ControlMissile(short fxNumber)
{
	static const int hitRadius = 200;

	// TODO: Add fx.target (ItemInfo* target) to get the actual target (if it was really it).
	auto& fx = EffectList[fxNumber];

	auto isUnderwater = TestEnvironment(ENV_FLAG_WATER, fx.roomNumber);
	auto soundFXType = isUnderwater ? SoundEnvironment::Underwater : SoundEnvironment::Land;

	if (fx.objectNumber == ID_SCUBA_HARPOON && isUnderwater &&
		fx.pos.Orientation.x > ANGLE(-67.5f))
	{
		fx.pos.Orientation.x -= ANGLE(1.0f);
	}

	fx.pos.Translate(fx.pos.Orientation, fx.speed);

	auto pointColl = GetPointCollision(fx.pos.Position, fx.roomNumber);
	auto hasHitPlayer = ItemNearLara(fx.pos.Position, hitRadius);

	// Check whether something was hit.
	if (fx.pos.Position.y >= pointColl.GetFloorHeight() ||
		fx.pos.Position.y <= pointColl.GetCeilingHeight() ||
		pointColl.IsWall() ||
		hasHitPlayer)
	{
		if (fx.objectNumber == ID_KNIFETHROWER_KNIFE ||
			fx.objectNumber == ID_SCUBA_HARPOON ||
			fx.objectNumber == ID_PROJ_SHARD)
		{
			SoundEffect((fx.objectNumber == ID_SCUBA_HARPOON) ? SFX_TR4_WEAPON_RICOCHET : SFX_TR2_CIRCLE_BLADE_HIT, &fx.pos, soundFXType);
		}
		else if (fx.objectNumber == ID_PROJ_BOMB)
		{
			SoundEffect(SFX_TR1_ATLANTEAN_EXPLODE, &fx.pos, soundFXType);
			TriggerExplosionSparks(fx.pos.Position.x, fx.pos.Position.y, fx.pos.Position.z, 3, -2, 0, fx.roomNumber);
			TriggerExplosionSparks(fx.pos.Position.x, fx.pos.Position.y, fx.pos.Position.z, 3, -1, 0, fx.roomNumber);
			TriggerShockwave(&fx.pos, 48, 304, (GetRandomControl() & 0x1F) + 112, 128, 32, 32, 32, EulerAngles(2048, 0.0f, 0.0f), 0, true, false, false, (int)ShockwaveStyle::Normal);
		}

		if (hasHitPlayer)
		{
			if (fx.objectNumber == ID_KNIFETHROWER_KNIFE)
			{
				DoDamage(LaraItem, KNIFE_DAMAGE);
			}
			else if (fx.objectNumber == ID_SCUBA_HARPOON)
			{
				DoDamage(LaraItem, DIVER_HARPOON_DAMAGE);
			}
			else if (fx.objectNumber == ID_PROJ_BOMB)
			{
				DoDamage(LaraItem, MUTANT_BOMB_DAMAGE);
			}
			else if (fx.objectNumber == ID_PROJ_SHARD)
			{
				TriggerBlood(fx.pos.Position.x, fx.pos.Position.y, fx.pos.Position.z, 0, 10);
				SoundEffect(SFX_TR4_BLOOD_LOOP, &fx.pos, soundFXType);
				DoDamage(LaraItem, MUTANT_SHARD_DAMAGE);
			}

			LaraItem->HitStatus = true;
			fx.pos.Orientation.y = LaraItem->Pose.Orientation.y;
			fx.speed = LaraItem->Animation.Velocity.z;
			fx.frameNumber = 0;
			fx.counter = 0;
		}

		KillEffect(fxNumber);
		return;
	}

	if (pointColl.GetRoomNumber() != fx.roomNumber)
		EffectNewRoom(fxNumber, pointColl.GetRoomNumber());

	if (fx.objectNumber == ID_KNIFETHROWER_KNIFE)
		fx.pos.Orientation.z += ANGLE(30.0f); // Update knife rotation over time.

	switch (fx.objectNumber)
	{
	case ID_SCUBA_HARPOON:
		if (TestEnvironment(RoomEnvFlags::ENV_FLAG_WATER, fx.roomNumber))
			SpawnBubble(fx.pos.Position.ToVector3(), fx.roomNumber);

		break;

	case ID_PROJ_BOMB:
		SpawnDynamicLight(fx.pos.Position.x, fx.pos.Position.y, fx.pos.Position.z, 14, 180, 100, 0);
		break;
	}
}

void ControlNatlaGun(short fxNumber)
{
	auto& fx = EffectList[fxNumber];

	fx.frameNumber--;
	if (fx.frameNumber <= Objects[fx.objectNumber].nmeshes)
		KillEffect(fxNumber);

	// If first frame, start another explosion at next position.
	if (fx.frameNumber == -1)
	{
		auto pointColl = GetPointCollision(fx.pos.Position, fx.roomNumber, fx.pos.Orientation.y, fx.speed);

		// Don't create one if hit a wall.
		if (pointColl.GetPosition().y >= pointColl.GetFloorHeight() ||
			pointColl.GetPosition().y <= pointColl.GetCeilingHeight())
		{
			return;
		}

		fxNumber = CreateNewEffect(pointColl.GetRoomNumber());
		if (fxNumber != NO_VALUE)
		{
			auto& fxNew = EffectList[fxNumber];

			fxNew.pos.Position = pointColl.GetPosition();
			fxNew.pos.Orientation.y = fx.pos.Orientation.y;
			fxNew.roomNumber = pointColl.GetRoomNumber();
			fxNew.speed = fx.speed;
			fxNew.frameNumber = 0;
			fxNew.objectNumber = ID_PROJ_BOMB;
		}
	}
}

short ShardGun(int x, int y, int z, short velocity, short yRot, short roomNumber)
{
	int fxNumber = CreateNewEffect(roomNumber);
	if (fxNumber != NO_VALUE)
	{
		auto& fx = EffectList[fxNumber];

		fx.pos.Position = Vector3i(x, y, z);
		fx.pos.Orientation = EulerAngles(0, yRot, 0);
		fx.roomNumber = roomNumber;
		fx.speed = velocity;
		fx.frameNumber = 0;
		fx.objectNumber = ID_PROJ_SHARD;
		fx.color = Vector4::One;
		ShootAtLara(fx);
	}

	return fxNumber;
}

short BombGun(int x, int y, int z, short velocity, short yRot, short roomNumber)
{
	int fxNumber = CreateNewEffect(roomNumber);
	if (fxNumber != NO_VALUE)
	{
		auto& fx = EffectList[fxNumber];

		fx.pos.Position = Vector3i(x, y, z);
		fx.pos.Orientation = EulerAngles(0, yRot, 0);
		fx.roomNumber = roomNumber;
		fx.speed = velocity;
		fx.frameNumber = 0;
		fx.objectNumber = ID_PROJ_BOMB;
		fx.color = Vector4::One;
		ShootAtLara(fx);
	}

	return fxNumber;
}
