#include "framework.h"
#include "Objects/Effects/Boss.h"

#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/effects/effects.h"
#include "Game/effects/Light.h"
#include "Game/effects/spark.h"
#include "Game/effects/tomb4fx.h"
#include "Game/items.h"
#include "Game/misc.h"
#include "Game/pickup/pickup.h"
#include "Game/Setup.h"
#include "Objects/TR3/Entity/PunaBoss.h"

using namespace TEN::Collision::Point;
using namespace TEN::Effects::Spark;
using namespace TEN::Entities::Creatures::TR3;

namespace TEN::Effects::Boss
{
	constexpr auto EXPLOSION_RING_SPEED  = 256;
	constexpr auto EXPLOSION_LIGHT_RANGE = BLOCK(8);

	std::array<BossExplosionRing, MAX_BOSS_EXPLOSION_RINGS> BossExplosionRings = {};

	void SpawnShield(const ItemInfo& item, const Vector4& color)
	{
		if (!item.TestFlags((int)BossItemFlags::Object, (short)BossFlagValue::Shield))
			return;

		int itemNumber = CreateItem();
		if (itemNumber == NO_VALUE)
			return;

		auto& shieldItem = g_Level.Items[itemNumber];

		shieldItem.ObjectNumber = ID_BOSS_SHIELD;
		shieldItem.RoomNumber = item.RoomNumber;
		shieldItem.Pose.Position = item.Pose.Position;
		shieldItem.Pose.Position.y -= CLICK(3);
		shieldItem.Pose.Orientation = item.Pose.Orientation;
		shieldItem.Flags |= IFLAG_ACTIVATION_MASK;

		InitializeItem(itemNumber);

		shieldItem.Model.Color = color;
		shieldItem.Collidable = true;
		shieldItem.ItemFlags[0] = 2;
		shieldItem.Model.Mutators[0].Scale = Vector3(2.0f); // Scale model by factor of 2.
		shieldItem.Status = ITEM_ACTIVE;

		AddActiveItem(itemNumber);
	}

	void SpawnShockwaveExplosion(const ItemInfo& item, const Vector4& color)
	{
		if (!item.TestFlags((int)BossItemFlags::Object, (short)BossFlagValue::ShockwaveExplosion))
			return;

		int itemNumber = CreateItem();
		if (itemNumber == NO_VALUE)
			return;

		auto& shockwaveItem = g_Level.Items[itemNumber];

		shockwaveItem.Pose.Position = item.Pose.Position;
		shockwaveItem.Pose.Position.y -= CLICK(2);

		shockwaveItem.Pose.Orientation = EulerAngles(
			Random::GenerateAngle(ANGLE(-20.0f), ANGLE(20.0f)),
			Random::GenerateAngle(ANGLE(-20.0f), ANGLE(20.0f)),
			Random::GenerateAngle(ANGLE(-20.0f), ANGLE(20.0f)));

		shockwaveItem.ObjectNumber = ID_BOSS_EXPLOSION_SHOCKWAVE;
		shockwaveItem.RoomNumber = item.RoomNumber;
		shockwaveItem.Flags |= IFLAG_ACTIVATION_MASK;

		InitializeItem(itemNumber);

		auto result = color;
		result.w = 1.0f;
		shockwaveItem.Model.Color = result;
		shockwaveItem.Collidable = false;					   // No collision for this entity.
		shockwaveItem.ItemFlags[0] = 64;					   // Timer before clearing; will fade out, then get destroyed.
		shockwaveItem.Model.Mutators[0].Scale = Vector3::Zero; // Start without scale.
		shockwaveItem.Status = ITEM_ACTIVE;

		AddActiveItem(itemNumber);
		SoundEffect(SFX_TR3_BLAST_CIRCLE, &shockwaveItem.Pose);
	}

	void TriggerBossExplosionRing(const Vector3& pos, int speed, BossExplosionRingColor colorType)
	{
		for (auto& ring : BossExplosionRings)
		{
			if (ring.IsActive)
				continue;

			   ring.IsActive = true;
			   ring.ColorType = colorType;
			   ring.Life = BOSS_EXPLOSION_RING_LIFE_MAX;
			   ring.Speed = speed;
			   ring.Radius = 0;
			   ring.XRot = (short)Random::GenerateInt(0, USHRT_MAX);
			   ring.ZRot = (short)Random::GenerateInt(0, USHRT_MAX);
			   ring.Position = pos;
			   return;
		}
	}

	void UpdateBossExplosionRings()
	{
		for (auto& ring : BossExplosionRings)
		{
			if (!ring.IsActive)
				continue;

			ring.StoreInterpolationData();

			ring.Life--;
			if (ring.Life <= 0)
			{
				ring.IsActive = false;
				continue;
			}

			ring.Radius += ring.Speed;
		}
	}

	void ShieldControl(int itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		// Entity life.
		if (item.ItemFlags[0] > 0)
			item.ItemFlags[0]--;

		if (item.ItemFlags[0] <= 0)
		{
			RemoveActiveItem(itemNumber);
			KillItem(itemNumber);
		}

		auto colorEnd = Vector4(item.Model.Color.x - 0.1f, item.Model.Color.y - 0.1f, item.Model.Color.z - 0.1f, item.Model.Color.w);
		item.Model.Color = Vector4::Lerp(item.Model.Color, colorEnd, 0.35f);
		item.Pose.Orientation.y += Random::GenerateAngle();
		UpdateItemRoom(itemNumber);
	}

	// NOTE: Ring and explosion wave rotate and scale by default.
	void ShockwaveRingControl(int itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		// Entity life.
		if (item.ItemFlags[0] > 0)
			item.ItemFlags[0]--;

		if (item.ItemFlags[0] <= 0)
		{
			auto colorEnd = Vector4(item.Model.Color.x - 0.1f, item.Model.Color.y - 0.1f, item.Model.Color.z - 0.1f, item.Model.Color.w);
			item.Model.Color = Vector4::Lerp(item.Model.Color, colorEnd, 0.35f);

			if (item.Model.Color.x <= 0.0f &&
				item.Model.Color.y <= 0.0f &&
				item.Model.Color.z <= 0.0f)
			{
				RemoveActiveItem(itemNumber);
				KillItem(itemNumber);
			}
		}

		item.Model.Mutators[0].Scale += Vector3(0.25f);
		UpdateItemRoom(itemNumber);
	}

	void ShockwaveExplosionControl(int itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		// Entity life.
		if (item.ItemFlags[0] > 0)
			item.ItemFlags[0]--;

		if (item.ItemFlags[0] <= 0)
		{
			auto colorEnd = Vector4(item.Model.Color.x - 0.1f, item.Model.Color.y - 0.1f, item.Model.Color.z - 0.1f, item.Model.Color.w);
			item.Model.Color = Vector4::Lerp(item.Model.Color, colorEnd, 0.35f);

			if (item.Model.Color.x <= 0.0f &&
				item.Model.Color.y <= 0.0f &&
				item.Model.Color.z <= 0.0f)
			{
				RemoveActiveItem(itemNumber);
				KillItem(itemNumber);
			}
		}

		item.Pose.Orientation.y += ANGLE(5.0f);
		item.Model.Mutators[0].Scale += Vector3(0.15f);
		UpdateItemRoom(itemNumber);
	}

	void SpawnExplosionSmoke(const Vector3& pos, const Vector4& color)
	{
		auto& smoke = *GetFreeParticle();

		auto sphere = BoundingSphere(pos, BLOCK(1 / 64.0f));
		auto effectPos = Random::GeneratePointInSphere(sphere);

		smoke.on = true;
		smoke.blendMode = BlendMode::Additive;

		smoke.x = effectPos.x;
		smoke.y = effectPos.y;
		smoke.z = effectPos.z;
		smoke.xVel = Random::GenerateFloat(BLOCK(0.5f), BLOCK(0.5f));
		smoke.yVel = GetRandomControl() - 128;
		smoke.zVel = Random::GenerateFloat(BLOCK(0.5f), BLOCK(0.5f));
		smoke.sR = color.x * 200;
		smoke.sG = color.y * 200;
		smoke.sB = color.z * 200;
		smoke.dR = color.x * 100;
		smoke.dG = color.y * 100;
		smoke.dB = color.z * 100;
		smoke.colFadeSpeed = 8;
		smoke.fadeToBlack = 64;
		smoke.life =
		smoke.sLife = Random::GenerateInt(96, 128);
		smoke.friction = 6;
		smoke.rotAng = Random::GenerateAngle();
		smoke.rotAdd = Random::GenerateAngle(ANGLE(-0.2f), ANGLE(0.2f));

		smoke.scalar = 3;
		smoke.gravity = Random::GenerateInt(-8, -4);
		smoke.maxYvel = Random::GenerateInt(-12, -8);
		smoke.dSize = Random::GenerateInt(256, 288);
		smoke.sSize =
		smoke.size = smoke.dSize / 2;
		smoke.flags = SP_SCALE | SP_DEF | SP_ROTATE | SP_EXPDEF;
	}

	// NOTE: Actual death occurs when countUntilDeath >= 60.
	void ExplodeBoss(ItemInfo& item, int countUntilDeath, const Vector4& color, const Vector4& explosionColor1, const Vector4& explosionColor2, bool allowExplosion)
	{
		// Disable shield.
		item.SetFlagField((int)BossItemFlags::ShieldIsEnabled, 0);
		item.HitPoints = NOT_TARGETABLE;

		// Determine ring color type from boss object ID.
		auto ringColor = BossExplosionRingColor::Willard;
		switch (item.ObjectNumber)
		{
		case ID_TONY_BOSS:
			ringColor = BossExplosionRingColor::Tony;
			break;
		case ID_SOPHIA_LEIGH_BOSS:
			ringColor = BossExplosionRingColor::Sophia;
			break;
		case ID_PUNA_BOSS:
			ringColor = BossExplosionRingColor::Puna;
			break;
		}

		// Start explosion (entity will keep duration count).
		int counter = item.ItemFlags[(int)BossItemFlags::ExplodeCount];
		if (counter == 1)
		{
			SoundEffect(SFX_TR4_EXPLOSION2, &item.Pose);
			SpawnShockwaveExplosion(item, color);
		}

		// Spawn staggered explosion rings, smoke and sparks at classic TR3 intervals.
		if (counter == 1 || counter == 15 || counter == 25 || counter == 35 || counter == 45 || counter == 55)
		{
			auto ringPos = item.Pose.Position.ToVector3() + Vector3(0.0f, -CLICK(2), 0.0f);
			TriggerBossExplosionRing(ringPos, EXPLOSION_RING_SPEED, ringColor);

			auto lightPos = item.Pose.Position.ToVector3();
			auto lightColor = Color(explosionColor1.x, explosionColor1.y, explosionColor1.z);
			SpawnDynamicPointLight(lightPos, lightColor, EXPLOSION_LIGHT_RANGE);

			auto sphere = BoundingSphere(item.Pose.Position.ToVector3() + Vector3(0.0f, -CLICK(3), 0.0f), BLOCK(0.5f));
			for (int i = 0; i < 3; i++)
			{
				auto pos = Random::GeneratePointInSphere(sphere);
				SpawnExplosionSmoke(pos, color);

				TriggerExplosionSparks(
					item.Pose.Position.x + (Random::GenerateInt(0, 127) - 64 * 2),
					(item.Pose.Position.y - CLICK(2)) + (Random::GenerateInt(0, 127) - 64 * 2),
					item.Pose.Position.z + (Random::GenerateInt(0, 127) - 64 * 2),
					2, -3, 0, item.RoomNumber, Vector3(explosionColor1.x, explosionColor1.y, explosionColor1.z), 
					Vector3(explosionColor2.x, explosionColor2.y, explosionColor2.z));
			}

			SoundEffect(SFX_TR3_BLAST_CIRCLE, &item.Pose);
		}

		if (counter > 0 && !(counter % 10))
		{
			auto sphere = BoundingSphere(item.Pose.Position.ToVector3() + Vector3(0.0f, -CLICK(3), 0.0f), BLOCK(0.5f));
			for (int i = 0; i < 3; i++)
			{
				auto pos = Random::GeneratePointInSphere(sphere);
				SpawnExplosionSmoke(pos, color);

				TriggerExplosionSparks(
					item.Pose.Position.x + (Random::GenerateInt(0, 127) - 64 * 2),
					(item.Pose.Position.y - CLICK(2)) + (Random::GenerateInt(0, 127) - 64 * 2),
					item.Pose.Position.z + (Random::GenerateInt(0, 127) - 64 * 2),
					2, -3, 0, item.RoomNumber, Vector3(explosionColor1.x, explosionColor1.y, explosionColor1.z),
					Vector3(explosionColor2.x, explosionColor2.y, explosionColor2.z));
			}

			auto lightPos = item.Pose.Position.ToVector3();
			auto lightColor = Color(explosionColor1.x, explosionColor1.y, explosionColor1.z);
			SpawnDynamicPointLight(lightPos, lightColor, EXPLOSION_LIGHT_RANGE);
		}

		SpawnDynamicLight(
			item.Pose.Position.x,
			item.Pose.Position.y - CLICK(2),
			item.Pose.Position.z,
			counter / 2,
			color.x * UCHAR_MAX, color.y * UCHAR_MAX, color.z * UCHAR_MAX);

		if (counter >= countUntilDeath)
		{
			DropPickups(&item, true);

			if (allowExplosion)
				CreatureDie(item.Index, allowExplosion, true);
			else
				KillItem(item.Index);
		}
	}

	void CheckForRequiredObjects(ItemInfo& item)
	{
		short flags = 0;

		if (item.ObjectNumber == ID_PUNA_BOSS && Objects[ID_LIZARD].loaded)		
			flags |= (short)BossFlagValue::Lizard;
		
		// The following are only for aesthetics.

		if (Objects[ID_BOSS_EXPLOSION_SHOCKWAVE].loaded)
			flags |= (short)BossFlagValue::ShockwaveExplosion;

		if (Objects[ID_BOSS_SHIELD].loaded)
			flags |= (short)BossFlagValue::Shield;

		item.SetFlagField((int)BossItemFlags::Object, flags);
	}

	// NOTE: Used by player's FireWeapon() and only if ID_BOSS_SHIELD is loaded.
	void SpawnShieldAndRichochetSparks(const ItemInfo& item, const Vector3& pos, const Vector4& color)
	{
		SpawnShield(item, color);

		auto target = GameVector(pos, item.RoomNumber);
		auto yOrient = Geometry::GetOrientToPoint(item.Pose.Position.ToVector3(), target.ToVector3()).y;
		auto sparkColor = color;
		sparkColor.w = 1.0f;
		TriggerRicochetSpark(target, yOrient, 13, sparkColor);
	}
}
