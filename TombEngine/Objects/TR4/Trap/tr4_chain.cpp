#include "framework.h"
#include "Objects/TR4/Trap/tr4_chain.h"

#include "Game/collision/Sphere.h"
#include "Game/Lara/lara.h"
#include "Game/effects/effects.h"
#include "Game/effects/item_fx.h"
#include "Game/collision/collide_item.h"
#include "Specific/level.h"
#include "Game/effects/spark.h"

using namespace TEN::Effects::Spark;

namespace TEN::Entities::Traps
{
	constexpr auto PENDULUM_FIRE_FOG_DENSITY = 0.08f;
	constexpr auto PENDULUM_FIRE_FOG_RADIUS = 4;
	constexpr auto PENDULUM_FLAME_SPARK_LENGHT = 190;

	void TriggerPendulumFlame(int itemNumber, Vector3i pos)
	{
		auto& item = g_Level.Items[itemNumber];

		auto* spark = GetFreeParticle();
		spark->on = 1;
		spark->sR = (GetRandomControl() & 0x1F) + 48;
		spark->sG = spark->sR >> 1;
		spark->sB = 0;
		spark->dR = (GetRandomControl() & 0x3F) + 192;
		spark->dG = (GetRandomControl() & 0x3F) + 128;
		spark->dB = 32;
		spark->colFadeSpeed = (GetRandomControl() & 3) + 12;
		spark->fadeToBlack = 8;

		spark->extras = 0;
		spark->life = Random::GenerateInt(1,15);
		spark->sLife = spark->life;
		spark->dynamic = 1;

		spark->xVel = (GetRandomControl() & 0x3F) - 32;
		spark->yVel = -16 - (GetRandomControl() & 0xF);
		spark->zVel = (GetRandomControl() & 0x3F) - 32;
		spark->friction = 4;
		spark->flags = SP_SCALE | SP_DEF | SP_ROTATE | SP_EXPDEF | SP_ITEM ;

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
		spark->dSize = static_cast<int>(spark->size) >> 2;

		spark->SpriteSeqID = ID_DEFAULT_SPRITES;
		spark->SpriteID = 0;
	}

	void TriggerPendulumSpark(const GameVector& pos, const EulerAngles& angle, float length, int count)
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
			s.velocity =  v* Random::GenerateFloat(32, 64);
			s.sourceColor = Vector4(1, 0.7f, 0.4f, 1);
			s.destinationColor = Vector4(0.4f, 0.1f, 0, 0.5f);
			s.active = true;
		}
	}    

	void ControlChain(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!TriggerActive(&item))
			return;

		if (item.TriggerFlags < 0)
		{
			item.ItemFlags[2] = 1;
			item.ItemFlags[3] = 75;
			item.ItemFlags[5] = 1; // Set the item on fire when collide.

			if (TriggerActive(&item))
			{
				*(int*)&item.ItemFlags[0] = 0x787E;
				AnimateItem(&item);

				auto pos = GetJointPosition(item, 4, Vector3i(0, 260, 0));
				auto angle = GetBoneOrientation(item, 5);
				unsigned char r = 51 - ((GetRandomControl() / 16) & 6);
				unsigned char g = 44 - ((GetRandomControl() / 64) & 6);
				unsigned char b = GetRandomControl() & 10;
				SpawnDynamicLight(pos.x, pos.y, pos.z, 12, r, g, b);

				r += 125 - ((GetRandomControl() / 16) & 4);
				g += 98 - ((GetRandomControl() / 16) & 8);

				auto color = Color(r / (float)CHAR_MAX, g / (float)CHAR_MAX, b / (float)CHAR_MAX);

				SpawnDynamicFogBulb(pos.ToVector3(), PENDULUM_FIRE_FOG_RADIUS, PENDULUM_FIRE_FOG_DENSITY, color);
				TriggerPendulumFlame(itemNumber, pos);
				TriggerPendulumSpark(pos, angle, PENDULUM_FLAME_SPARK_LENGHT, 1);

				return;
			}
		}
		else if (item.TriggerFlags > 0)
		{
			item.ItemFlags[2] = 1;
			item.ItemFlags[3] = 75;
			item.ItemFlags[5] = 0;

			if (TriggerActive(&item))
			{
				*(int*)&item.ItemFlags[0] = 0x787E;
				AnimateItem(&item);
				return;
			}
		} 
		else
		{
			item.ItemFlags[3] = 25;

			if (TriggerActive(&item))
			{
				*(int*)&item.ItemFlags[0] = 0x780;
				AnimateItem(&item);
				return;
			}
		}

		*(int*)&item.ItemFlags[0] = 0;
	}

	void CollideChain(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
	{
		auto& item = g_Level.Items[itemNumber];
		if (item.Status == ITEM_INVISIBLE)
			return;

		if (!TestBoundsCollide(&item, playerItem, coll->Setup.Radius))
			return;

		TEN::Collision::Sphere::HandleItemSphereCollision(item, *playerItem);
		if (!item.TouchBits.TestAny())
			return;

		short prevYOrient = item.Pose.Orientation.y;
		item.Pose.Orientation.y = 0;
		auto spheres = item.GetSpheres();
		item.Pose.Orientation.y = prevYOrient;

		int harmBits = *(int*)&item.ItemFlags[0]; // NOTE: Value spread across ItemFlags[0] and ItemFlags[1].

		auto collidedBits = item.TouchBits;
		if (item.ItemFlags[2] != 0)
			collidedBits.Clear(0);

		coll->Setup.EnableObjectPush = (item.ItemFlags[4] == 0);

		// Handle push and damage.
		for (int i = 0; i < spheres.size(); i++)
		{
			if (collidedBits.Test(i))
			{
				const auto& sphere = spheres[i];

				GlobalCollisionBounds.X1 = sphere.Center.x - sphere.Radius - item.Pose.Position.x;
				GlobalCollisionBounds.X2 = sphere.Center.x + sphere.Radius - item.Pose.Position.x;
				GlobalCollisionBounds.Y1 = sphere.Center.y - sphere.Radius - item.Pose.Position.y;
				GlobalCollisionBounds.Y2 = sphere.Center.y + sphere.Radius - item.Pose.Position.y;
				GlobalCollisionBounds.Z1 = sphere.Center.z - sphere.Radius - item.Pose.Position.z;
				GlobalCollisionBounds.Z2 = sphere.Center.z + sphere.Radius - item.Pose.Position.z;

				auto pos = playerItem->Pose.Position;
				if (ItemPushItem(&item, playerItem, coll, harmBits & 1, 3) && (harmBits & 1))
				{
					DoDamage(playerItem, item.ItemFlags[3]);

					if (item.ItemFlags[5])
						TEN::Effects::Items::ItemBurn(LaraItem);

					auto deltaPos = pos - playerItem->Pose.Position;
					if (deltaPos != Vector3i::Zero)
					{
						if (TriggerActive(&item))
							TriggerLaraBlood();
					}

					if (!coll->Setup.EnableObjectPush)
						playerItem->Pose.Position += deltaPos;
				}
			}

			harmBits >>= 1;
		}
	}
}