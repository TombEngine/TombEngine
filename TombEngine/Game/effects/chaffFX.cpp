#include "framework.h"
#include "Game/effects/chaffFX.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_room.h"
#include "Game/control/control.h"
#include "Game/effects/Bubble.h"
#include "Game/effects/smoke.h"
#include "Game/effects/spark.h"
#include "Game/effects/tomb4fx.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Specific/level.h"
#include "Renderer/RendererEnums.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Sound/sound.h"

using namespace TEN::Animation;
using namespace TEN::Effects::Bubble;
using namespace TEN::Math;

constexpr auto MAX_TRIGGER_RANGE = BLOCK(16);

void TriggerChaffEffects(int flareLife)
{
	auto offset = g_GameFlow->GetSettings()->Flare.Offset.ToVector3i() + Vector3i(11, 32, -4);
	auto pos = GetJointPosition(LaraItem, LM_LHAND,  offset);
	auto vec = GetJointPosition(LaraItem, LM_LHAND, Vector3i(11, 32, BLOCK(1) + Random::GenerateInt(0, 256)));
	auto vel = vec - pos;
	TriggerChaffEffects(*LaraItem, pos, vel, LaraItem->Animation.Velocity.z, TestEnvironment(ENV_FLAG_WATER, LaraItem->RoomNumber), flareLife);
}

void TriggerChaffEffects(ItemInfo& item, int age)
{
	auto offset = g_GameFlow->GetSettings()->Flare.Offset.ToVector3() + Vector3(0, 0, -4);
	auto world = Matrix::CreateTranslation(offset) * item.Pose.Orientation.ToRotationMatrix();
	auto pos = item.Pose.Position + Vector3i(world.Translation());

	world = Matrix::CreateTranslation(offset) *
		Matrix::CreateTranslation((GetRandomDraw() & 127) - 64, (GetRandomDraw() & 127) - 64, (GetRandomDraw() & 511) + 512) *
		item.Pose.Orientation.ToRotationMatrix();

	auto vel = Vector3i(world.Translation());
	TriggerChaffEffects(item, pos, vel, item.Animation.Velocity.z, TestEnvironment(ENV_FLAG_WATER, &item), age);
}

void TriggerChaffEffects(ItemInfo& item, const Vector3i& pos, const Vector3i& vel, int speed, bool isUnderwater, int age)
{
	auto pose = item.Pose;
	if (item.IsLara())
	{
		auto handPos = GetJointPosition(&item, LM_RHAND);
		pose.Position = handPos;
		pose.Position.y -= 64;
	}

	auto cond = TestEnvironment(RoomEnvFlags::ENV_FLAG_WATER, pose.Position, item.RoomNumber);
	SoundEffect(cond ? SFX_TR4_FLARE_BURN_UNDERWATER : SFX_TR4_FLARE_BURN_DRY, &pose, SoundEnvironment::Always, 1.0f, 0.5f);

	if (!age)
		return;

	int numSparks = Random::GenerateInt(1, 3);

	for (int i = 0; i < numSparks; i++)
	{
		long dx, dz;

		dx = item.Pose.Position.x - pos.x;
		dz = item.Pose.Position.z - pos.z;

		if (dx < -MAX_TRIGGER_RANGE || dx > MAX_TRIGGER_RANGE || dz < -MAX_TRIGGER_RANGE || dz > MAX_TRIGGER_RANGE)
			return;

		const auto& settings = g_GameFlow->GetSettings()->Flare;

		if (settings.Sparks)
			TriggerChaffSparkles(pos, vel, settings.Color, age, item);

		if (!settings.Smoke)
			continue;

		if (isUnderwater)
		{
			if (Random::TestProbability(1 / 4.0f))
				SpawnChaffBubble(pos.ToVector3(), item.RoomNumber);
		}
		else
		{
			auto direction = vel.ToVector3();
			direction.Normalize();
			TEN::Effects::Smoke::TriggerFlareSmoke(pos.ToVector3() + direction * 20, direction, age, item.RoomNumber);
		}
	}
}

void TriggerChaffSparkles(const Vector3i& pos, const Vector3i& vel, const Color& color, int age, const ItemInfo& item)
{
	TEN::Effects::Spark::TriggerFlareSparkParticles(pos, vel, color, item.RoomNumber);
}

void TriggerChaffSmoke(const Vector3i& pos, const Vector3i& vel, int speed, bool isMoving, bool wind)
{
	SMOKE_SPARKS* smoke;

	int rnd = 0;
	BYTE trans, size;
	
	smoke = &SmokeSparks[GetFreeSmokeSpark()];

	smoke->on = true;

	smoke->sShade = 0;
	if (isMoving)
	{
		trans = (speed << 7) >> 5;
		smoke->dShade = trans;
	}
	else
		smoke->dShade = 64 + (GetRandomDraw() & 7);

	smoke->colFadeSpeed = 4 + (GetRandomDraw() & 3);
	smoke->fadeToBlack = 4;

	rnd = (GetRandomControl() & 3) - (speed >> 12) + 20;
	if (rnd < 9)
	{
		smoke->life = 9;
		smoke->sLife = 9;
	}
	else
	{
		smoke->life = rnd;
		smoke->sLife = rnd;
	}

	smoke->blendMode = BlendMode::Additive;
	
	smoke->position.x = pos.x + (GetRandomControl() & 7) - 3;
	smoke->position.y = pos.y + (GetRandomControl() & 7) - 3;
	smoke->position.z = pos.z + (GetRandomControl() & 7) - 3;
	smoke->velocity.x = vel.x + ((GetRandomDraw() & 63) - 32);
	smoke->velocity.y = vel.y;
	smoke->velocity.z = vel.z + ((GetRandomDraw() & 63) - 32);
	smoke->friction = 4;

	if (GetRandomControl() & 1)
	{
		smoke->flags = SP_EXPDEF | SP_ROTATE | SP_DEF | SP_SCALE;
		smoke->rotAng = (GetRandomControl() & 0xFFF);
		if (GetRandomControl() & 1)
			smoke->rotAdd = (GetRandomControl() & 7) - 24;
		else
			smoke->rotAdd = (GetRandomControl() & 7) + 24;
	}
	else
		smoke->flags = SP_EXPDEF | SP_DEF | SP_SCALE;

	if (wind)
		smoke->flags |= SP_WIND;

	smoke->scalar = 1;
	smoke->gravity = (GetRandomControl() & 3) - 4;
	smoke->maxYvel = 0;
	size = (GetRandomControl() & 7) + (speed >> 7) + 32;
	smoke->sSize = size >> 2;
	smoke->size = smoke->dSize = size;
}
