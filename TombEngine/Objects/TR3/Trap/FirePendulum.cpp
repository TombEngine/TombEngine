#include "framework.h"
#include "Objects/TR3/Trap/FirePendulum.h"

#include "Game/collision/sphere.h"
#include "Game/Lara/lara.h"
#include "Game/effects/effects.h"
#include "Game/effects/item_fx.h"
#include "Game/collision/collide_item.h"
#include "Scripting/Internal/TEN/Properties/PropertyHandler.h"
#include "Scripting/Internal/TEN/Properties/PropertyNames.h"
#include "Specific/level.h"
#include "Game/effects/spark.h"

using namespace TEN::Collision::Sphere;
using namespace TEN::Effects::Spark;
using namespace TEN::Effects::Items;
using namespace TEN::Scripting::Properties;

namespace TEN::Entities::Traps
{
	constexpr auto PENDULUM_FIRE_FOG_DENSITY   = 15;
	constexpr auto PENDULUM_FIRE_FOG_RADIUS    = 4; // Clicks; SpawnDynamicFogBulb converts to world units (x UCHAR_MAX).
	constexpr auto PENDULUM_FLAME_SPARK_LENGTH = 80;
	constexpr auto PENDULUM_FLAME_SPARK_COUNT  = 3;
	constexpr auto PENDULUM_FLAME_SPARK_SPREAD = 48;
	constexpr auto PENDULUM_DAMAGE_VALUE       = 75;

	const std::vector<unsigned int> FirePendulumHarmJoints = { 4, 5 };

	enum PendulumFlags
	{
		FireColorRed,
		FireColorGreen,
		FireColorBlue,
		FlameMesh
	};

	void TriggerPendulumFlame(int itemNumber, Vector3i pos, Color color)
	{
		auto& item = g_Level.Items[itemNumber];

		auto* spark = GetFreeParticle();
		spark->on = 1;
		spark->sR = (GetRandomControl() & 0x1F) + 48;
		spark->sG = spark->sR >> 1;
		spark->sB = 0;
		spark->dR = color.x;
		spark->dG = color.y;
		spark->dB = color.z;
		spark->colFadeSpeed = (GetRandomControl() & 3) + 12;
		spark->fadeToBlack = 8;

		spark->extras = 0;
		spark->life = Random::GenerateInt(1, 15);
		spark->sLife = spark->life;

		spark->xVel = (GetRandomControl() & 0x3F) - 32;
		spark->yVel = -16 - (GetRandomControl() & 0xF);
		spark->zVel = (GetRandomControl() & 0x3F) - 32;
		spark->friction = 4;
		spark->flags = SP_SCALE | SP_DEF | SP_ROTATE;

		if (GetRandomControl() & 1)
		{
			spark->flags |= SP_ROTATE;
			spark->rotAng = GetRandomControl() & 0xFFF;
			spark->rotAdd = (GetRandomControl() & 0x1F) - 16;
		}

		spark->x = pos.x + (GetRandomControl() & 0x1F) - 16;
		spark->y = pos.y;
		spark->z = pos.z + (GetRandomControl() & 0x1F) - 16;

		spark->gravity = -16 - (GetRandomControl() & 0x1F);
		spark->maxYvel = -16 - (GetRandomControl() & 7);
		spark->scalar = spark->life < 32 ? 4 : 3;
		spark->size = (GetRandomControl() & 7) + 20;
		spark->sSize = spark->size;
		spark->dSize = spark->size / 8.0f;

		spark->SpriteSeqID = ID_DEFAULT_SPRITES;
		spark->SpriteID = 0;
	}

	void TriggerPendulumSpark(const GameVector& pos, const EulerAngles& angle, float length, int count, Color color)
	{
		for (int i = 0; i < count; i++)
		{
			auto& s = GetFreeSparkParticle();
			s = {};
			s.age = 1;
			s.life = Random::GenerateFloat(5, 8);
			s.friction = 0.5f;
			s.gravity = 0;
			s.height = length;
			s.width = Random::GenerateFloat(16.0f, 22.0f);
			s.room = pos.RoomNumber;
			s.pos = Vector3(pos.x + Random::GenerateFloat(-PENDULUM_FLAME_SPARK_SPREAD, PENDULUM_FLAME_SPARK_SPREAD),
							pos.y + Random::GenerateFloat(-PENDULUM_FLAME_SPARK_SPREAD, PENDULUM_FLAME_SPARK_SPREAD),
							pos.z + Random::GenerateFloat(-PENDULUM_FLAME_SPARK_SPREAD, PENDULUM_FLAME_SPARK_SPREAD));
			float ang = TO_RAD(angle.y);
			float vAng = TO_RAD(angle.x);
			auto v = Vector3(sin(ang), vAng + Random::GenerateFloat(-PI / 16, PI / 16), cos(ang));
			v.Normalize(v);
			s.velocity = v * Random::GenerateFloat(64, 128);

			auto sourceColorR = std::clamp(color.x - 0.2f, 0.0f, 1.0f);
			auto sourceColorG = std::clamp(color.y - 0.2f, 0.0f, 1.0f);
			auto sourceColorB = std::clamp(color.z - 0.2f, 0.0f, 1.0f);

			s.sourceColor = Vector4(sourceColorR, sourceColorG, sourceColorB, 1);
			s.destinationColor = Vector4(color.x, color.y, color.z, 0.5f);
			s.active = true;
		}
	}

	void ControlFirePendulum(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!TriggerActive(&item))
			return;

		AnimateItem(item);

		auto flameMesh = PropertyHandler::Get(item, PropName_EffectMeshID, (int)item.ItemFlags[PendulumFlags::FlameMesh], true);

		auto pos = GetJointPosition(item, flameMesh, Vector3i(0, 260, 0));
		auto angle = GetBoneOrientation(item, 5);

		auto defaultScriptColor = ScriptColor(
			item.ItemFlags[PendulumFlags::FireColorRed],
			item.ItemFlags[PendulumFlags::FireColorGreen],
			item.ItemFlags[PendulumFlags::FireColorBlue]);

		auto flameColor = (Vector3)PropertyHandler::Get(item, PropName_EffectColor, defaultScriptColor, true);

		unsigned char r, g, b;
		bool hasCustomColor = (flameColor != Vector3::Zero);

		if (hasCustomColor)
		{
			r = (unsigned char)(flameColor.x * (float)UCHAR_MAX);
			g = (unsigned char)(flameColor.y * (float)UCHAR_MAX);
			b = (unsigned char)(flameColor.z * (float)UCHAR_MAX);
		}
		else
		{
			r = 51 - ((GetRandomControl() / 16) & 6);
			g = 44 - ((GetRandomControl() / 64) & 6);
			b = GetRandomControl() & 10;
		}

		auto flameColor1 = Vector3::Zero;
		auto flameColor2 = Vector3::Zero;

		if (hasCustomColor)
		{
			auto sourceColorR = std::clamp((float)r / (float)UCHAR_MAX + 0.2f, 0.0f, 1.0f);
			auto sourceColorG = std::clamp((float)g / (float)UCHAR_MAX + 0.2f, 0.0f, 1.0f);
			auto sourceColorB = std::clamp((float)b / (float)UCHAR_MAX + 0.2f, 0.0f, 1.0f);

			flameColor1 = Vector3(sourceColorR, sourceColorG, sourceColorB);
			flameColor2 = Vector3((float)r / (float)UCHAR_MAX, (float)g / (float)UCHAR_MAX, (float)b / (float)UCHAR_MAX);
		}

		SpawnDynamicLight(pos.x, pos.y, pos.z, 12, r, g, b);

		r += 125 - ((GetRandomControl() / 16) & 4);
		g += 98 - ((GetRandomControl() / 16) & 8);

		auto color = Color(r / (float)UCHAR_MAX, g / (float)UCHAR_MAX, b / (float)UCHAR_MAX);

		if (PropertyHandler::Get(item, PropName_FogEffect, false))
			SpawnDynamicFogBulb(pos.ToVector3(), PENDULUM_FIRE_FOG_RADIUS, PENDULUM_FIRE_FOG_DENSITY, color);

		TriggerPendulumFlame(itemNumber, pos, color);
		TriggerPendulumSpark(pos, angle, PENDULUM_FLAME_SPARK_LENGTH, PENDULUM_FLAME_SPARK_COUNT, color);
		TriggerFireFlame(pos.x, pos.y, pos.z, FlameType::Trail, flameColor1, flameColor2);
	}

	void CollideFirePendulum(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
	{
		auto* item = &g_Level.Items[itemNumber];

		if (!TriggerActive(item))
			return;

		if (item->Status == ITEM_INVISIBLE)
			return;

		if (!TestBoundsCollide(item, playerItem, coll->Setup.Radius))
			return;

		if (!HandleItemSphereCollision(*item, *playerItem))
			return;

		auto defaultScriptColor = ScriptColor(
			item->ItemFlags[PendulumFlags::FireColorRed],
			item->ItemFlags[PendulumFlags::FireColorGreen],
			item->ItemFlags[PendulumFlags::FireColorBlue]);

		auto flameColor = (Vector3)PropertyHandler::Get(item, PropName_EffectColor, defaultScriptColor, true);
		bool hasCustomColor = (flameColor != Vector3::Zero);

		for (int i = 0; i < FirePendulumHarmJoints.size(); i++)
		{
			if (item->TouchBits.Test(FirePendulumHarmJoints[i]))
			{
				DoDamage(playerItem, PENDULUM_DAMAGE_VALUE);

				TriggerLaraBlood();

				if (playerItem->HitPoints > 0)
				{
					ItemPushItem(item, playerItem, coll, false, 1);
				}

				if (!hasCustomColor)
				{
					TEN::Effects::Items::ItemBurn(playerItem);
				}
				else
				{
					unsigned char r = (unsigned char)(flameColor.x * (float)UCHAR_MAX);
					unsigned char g = (unsigned char)(flameColor.y * (float)UCHAR_MAX);
					unsigned char b = (unsigned char)(flameColor.z * (float)UCHAR_MAX);

					auto sourceColorR = std::clamp((float)r / (float)UCHAR_MAX + 0.2f, 0.0f, 1.0f);
					auto sourceColorG = std::clamp((float)g / (float)UCHAR_MAX + 0.2f, 0.0f, 1.0f);
					auto sourceColorB = std::clamp((float)b / (float)UCHAR_MAX + 0.2f, 0.0f, 1.0f);

					ItemCustomBurn(playerItem, Vector3(sourceColorR, sourceColorG, sourceColorB), Vector3((float)r / (float)UCHAR_MAX, (float)g / (float)UCHAR_MAX, (float)b / (float)UCHAR_MAX));
				}
			}
		}
	}
}