#include "framework.h"
#include "Objects/TR2/Trap/CircularSaw.h"

#include "Game/collision/collide_item.h"
#include "Game/collision/Sphere.h"
#include "Game/effects/effects.h"
#include "Game/effects/spark.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Specific/level.h"

using namespace TEN::Collision::Sphere;
using namespace TEN::Effects::Spark;
using namespace TEN::Math;
using namespace TEN::Math::Random;

namespace TEN::Entities::Traps
{
	constexpr auto CIRCULAR_SAW_HARM_DAMAGE = 5000;
	constexpr auto CIRCULAR_SAW_HARM_MESH = 4;
	constexpr auto CIRCULAR_SAW_OVERDRIVE_SOUND_TIMER = 12;

	enum CircularSawState
	{
		CIRCULAR_SAW_STATE_STOP,
		CIRCULAR_SAW_STATE_START		
	};

	enum CircularSawAnim
	{
		CIRCULAR_SAW_ANIM_DISABLED = 0,
		CIRCULAR_SAW_ANIM_ACTIVATING = 1,
		CIRCULAR_SAW_ANIM_ENABLED = 2,
		CIRCULAR_SAW_ANIM_DEACTIVATING = 3	
	};

	void InitializeCircularSaw(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		item.ItemFlags[3] = CIRCULAR_SAW_HARM_DAMAGE;
	}

	void ControlCircularSaw(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (TriggerActive(&item))
		{
			if (item.Animation.TargetState != CIRCULAR_SAW_STATE_START)
			{
				item.Animation.TargetState = CIRCULAR_SAW_STATE_START;
				item.ItemFlags[0] = CIRCULAR_SAW_HARM_MESH;
			}
		}
		else
		{
			if (item.Animation.TargetState != CIRCULAR_SAW_STATE_STOP)
			{
				item.Animation.TargetState = CIRCULAR_SAW_STATE_STOP;

				// Unset harm joints.
				item.ItemFlags[0] = 0;
			}
			else
			{
				if (TestLastFrame(item,  CIRCULAR_SAW_ANIM_DISABLED))
				{
					item.Flags &= 0xC1;
					RemoveActiveItem(itemNumber, false);
					item.Active = false;
					item.Status = ITEM_NOT_ACTIVE;
				}
			}
		}

		if (item.Animation.AnimNumber == CIRCULAR_SAW_ANIM_ENABLED)
		{
			if (item.ItemFlags[5])
			{
				StopSoundEffect(SFX_TR2_SAW_REVVING);
				SoundEffect(SFX_TR2_SAW_REVVING, &item.Pose, SoundEnvironment::Land, 2, 4);
				item.ItemFlags[5]--;
			}
			else if (item.TriggerFlags)
			{
				TriggerSawSparkles(&item);
				SoundEffect(SFX_TR2_SAW_REVVING, &item.Pose, SoundEnvironment::Land, 2, 3);
			}
			else
			{
				SoundEffect(SFX_TR2_SAW_REVVING, &item.Pose, SoundEnvironment::Land, 1, 2);
			}
		}

		AnimateItem(&item);
	}

	void TriggerSawSparkles(ItemInfo* item)
	{
		for (int i = 0; i < 2; i++)
		{
			auto sparkYAngle = item->Pose.Orientation.y + GenerateAngle(ANGLE(-30.0f), ANGLE(30.0f));
			auto pos = GetJointPosition(item, 2);
			pos = Geometry::TranslatePoint(pos, item->Pose.Orientation.y, Vector3i(0, 270, 150));

			TriggerSawSpark(pos, EulerAngles(Random::GenerateAngle(), sparkYAngle, 0), 3, Vector4(1.0f, 0.5f, 0.1f, 1.0f));

			if (!i)
				continue;
			
			float mult = Random::GenerateFloat(0.7f, 1.0f);
			auto r = (unsigned char)(mult * 190.0f);
			auto g = (unsigned char)(mult * 100.0f);
			SpawnDynamicLight(pos.x, pos.y, pos.z, 2, r, g, 0);
		}
	}

	void TriggerSawSpark(const GameVector& pos, const EulerAngles& angle, int count, const Vector4& colorStart)
	{
		for (int i = 0; i < count; i++)
		{
			auto& s = GetFreeSparkParticle();
			s = {};
			s.age = 0;
			s.life = GenerateFloat(10, 20);
			s.friction = 0.98f;
			s.gravity = 1.2f;
			s.width = 9.0f;
			s.height = 85.0f;
			s.room = pos.RoomNumber;
			s.pos = pos.ToVector3();
			float ang = TO_RAD(angle.y);
			float vAng = -TO_RAD(angle.x);
			Vector3 v = Vector3(sin(ang), vAng + GenerateFloat(-PI / 16, PI / 16), cos(ang));
			v.Normalize(v);
			s.velocity = v * GenerateFloat(32, 64);
			s.sourceColor = colorStart;
			s.destinationColor = Vector4::Zero;
			s.active = true;
		}
	}

	void CollideCircularSaw(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
	{
		auto& item = g_Level.Items[itemNumber];
		if (item.Status == ITEM_INVISIBLE)
			return;

		if (!TestBoundsCollide(&item, playerItem, coll->Setup.Radius))
			return;

		HandleItemSphereCollision(item, *playerItem);
		if (!item.TouchBits.TestAny())
			return;

		short prevYOrient = item.Pose.Orientation.y;
		item.Pose.Orientation.y = 0;
		auto spheres = item.GetSpheres();
		item.Pose.Orientation.y = prevYOrient;

		int harmBits = *(int*)&item.ItemFlags[0]; // NOTE: Value spread across ItemFlags[0] and ItemFlags[1].

		auto collidedBits = item.TouchBits;

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

				if (ItemPushItem(&item, playerItem, coll, harmBits & 1, 0) && (harmBits & 1) && (item.ItemFlags[3] > 0))
				{
					DoDamage(playerItem, item.ItemFlags[3]);

					if (TriggerActive(&item))
					{
						item.ItemFlags[5] = CIRCULAR_SAW_OVERDRIVE_SOUND_TIMER;
						TriggerLaraBlood();
						DoLotsOfBlood(playerItem->Pose.Position.x, playerItem->Pose.Position.y - CLICK(2), playerItem->Pose.Position.z, (short)(item.Animation.Velocity.z * 2), playerItem->Pose.Orientation.y, playerItem->RoomNumber, 2);
					}
				}
			}

			harmBits >>= 1;
		}
	}
}
