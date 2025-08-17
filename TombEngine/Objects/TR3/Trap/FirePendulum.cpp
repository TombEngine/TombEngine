#include "framework.h"
#include "Objects/TR3/Trap/FirePendulum.h"

#include "Game/collision/Sphere.h"
#include "Game/Lara/lara.h"
#include "Game/effects/effects.h"
#include "Game/effects/item_fx.h"
#include "Game/collision/collide_item.h"
#include "Specific/level.h"
#include "Game/effects/spark.h"

using namespace TEN::Collision::Sphere;
using namespace TEN::Effects::Spark;
using namespace TEN::Effects::Items;

namespace TEN::Entities::Traps
{
	constexpr auto PENDULUM_FIRE_FOG_DENSITY = 15;
	constexpr auto PENDULUM_FIRE_FOG_RADIUS = 4;
	constexpr auto PENDULUM_FLAME_SPARK_LENGHT = 190;

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
		spark->life = Random::GenerateInt(1,15);
		spark->sLife = spark->life;

		spark->xVel = (GetRandomControl() & 0x3F) - 32;
		spark->yVel = -16 - (GetRandomControl() & 0xF);
		spark->zVel = (GetRandomControl() & 0x3F) - 32;
		spark->friction = 4;
		spark->flags = SP_SCALE | SP_DEF | SP_ROTATE | SP_EXPDEF | SP_ITEM;

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
		spark->dSize = static_cast<int>(spark->size) >> 3;

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
			s.friction = 0.05f;
			s.gravity = 0.1f;
			s.height = length + Random::GenerateFloat(16.0f, 22.0f);
			s.width = length;
			s.room = pos.RoomNumber;
			s.pos = Vector3(pos.x + Random::GenerateFloat(-16, 16), pos.y + Random::GenerateFloat(6, 60), pos.z + Random::GenerateFloat(-16, 16));
			float ang = TO_RAD(angle.y);
			float vAng = -TO_RAD(angle.x);
			Vector3 v = Vector3(sin(ang), vAng + Random::GenerateFloat(-PI / 16, PI / 16), cos(ang));
			v.Normalize(v);
			s.velocity = v * Random::GenerateFloat(32, 64);

			auto sourceColorR = std::clamp(color.x - 0.2f, 0.0f, 1.0f);
			auto sourceColorG = std::clamp(color.y - 0.2f, 0.0f, 1.0f);
			auto sourceColorB = std::clamp(color.z - 0.2f, 0.0f, 1.0f);

			s.sourceColor = Vector4(sourceColorR, sourceColorG, sourceColorB, 1);
			s.destinationColor = Vector4(color.x, color.y, color.z, 0.5f);
			s.active = true;
		}
	}    

	void InitializeFirePendulum(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		item.ItemFlags[PendulumFlags::FlameMesh] = 4;

		item.ItemFlags[PendulumFlags::FireColorRed] = 0;
		item.ItemFlags[PendulumFlags::FireColorGreen] = 0;
		item.ItemFlags[PendulumFlags::FireColorBlue] = 0;
	}

	void ControlFirePendulum(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!TriggerActive(&item))
			return;

		AnimateItem(&item);

		auto flameMesh = item.ItemFlags[PendulumFlags::FlameMesh];

		auto pos = GetJointPosition(item, flameMesh, Vector3i(0, 260, 0));
		auto angle = GetBoneOrientation(item, 5);

		unsigned char r = item.ItemFlags[PendulumFlags::FireColorRed];
		unsigned char g = item.ItemFlags[PendulumFlags::FireColorGreen];
		unsigned char b = item.ItemFlags[PendulumFlags::FireColorBlue];

		Vector3 flameColor1 = Vector3::Zero;
		Vector3 flameColor2 = Vector3::Zero;

		if (item.ItemFlags[PendulumFlags::FireColorRed] == 0 &&
			item.ItemFlags[PendulumFlags::FireColorGreen] == 0 &&
			item.ItemFlags[PendulumFlags::FireColorBlue] == 0)
		{
				r = 51 - ((GetRandomControl() / 16) & 6);
				g = 44 - ((GetRandomControl() / 64) & 6);
				b = GetRandomControl() & 10;	
		}
		else
		{
			auto sourceColorR = std::clamp(r + 0.2f, 0.0f, 1.0f);
			auto sourceColorG = std::clamp(g + 0.2f, 0.0f, 1.0f);
			auto sourceColorB = std::clamp(b + 0.2f, 0.0f, 1.0f);

			flameColor1 = Vector3(sourceColorR, sourceColorG, sourceColorB);
			flameColor2 = Vector3(r, g, b);
		}

		SpawnDynamicLight(pos.x, pos.y, pos.z, 12, r, g, b);

		r += 125 - ((GetRandomControl() / 16) & 4);
		g += 98 - ((GetRandomControl() / 16) & 8);

		auto color = Color(r / (float)CHAR_MAX, g / (float)CHAR_MAX, b / (float)CHAR_MAX);

		SpawnDynamicFogBulb(pos.ToVector3(), PENDULUM_FIRE_FOG_RADIUS, PENDULUM_FIRE_FOG_DENSITY, color);
		TriggerPendulumFlame(itemNumber, pos, color);
		TriggerPendulumSpark(pos, angle, PENDULUM_FLAME_SPARK_LENGHT, 1, color);
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

		for (int i = 0; i < FirePendulumHarmJoints.size(); i++)
		{
			if (item->TouchBits.Test(FirePendulumHarmJoints[i]))
			{
				DoDamage(playerItem, abs(item->TriggerFlags));

				TriggerLaraBlood();

				if (playerItem->HitPoints > 0)
				{
					ItemPushItem(item, playerItem, coll, false, 1);
				}

				if (item->ItemFlags[PendulumFlags::FireColorRed] == 0 &&
					item->ItemFlags[PendulumFlags::FireColorGreen] == 0 &&
					item->ItemFlags[PendulumFlags::FireColorBlue] == 0)
				{
					TEN::Effects::Items::ItemBurn(playerItem);
				}
				else
				{
					unsigned char r = item->ItemFlags[PendulumFlags::FireColorRed];
					unsigned char g = item->ItemFlags[PendulumFlags::FireColorGreen];
					unsigned char b = item->ItemFlags[PendulumFlags::FireColorBlue];

					auto sourceColorR = std::clamp(r + 0.2f, 0.0f, 1.0f);
					auto sourceColorG = std::clamp(g + 0.2f, 0.0f, 1.0f);
					auto sourceColorB = std::clamp(b + 0.2f, 0.0f, 1.0f);

					ItemCustomBurn(playerItem, Vector3(sourceColorR, sourceColorG, sourceColorB), Vector3(r, g, b));
				}
			}
		}
	}
}