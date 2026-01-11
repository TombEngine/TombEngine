#include "framework.h"
#include "Objects/TR5/Entity/AutoGun.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/Sphere.h"
#include "Game/control/los.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Sphere;
using namespace TEN::Math;

// NOTES:
// ItemFlags[0]: X axis orientation.
// ItemFlags[1]: Y axis orientation.
// ItemFlags[2]: barrel turn rate.

namespace TEN::Entities::Creatures::TR5
{
	constexpr auto AUTO_GUN_SHOT_DAMAGE			= 20;
	constexpr auto AUTO_GUN_BLOOD_EFFECT_CHANCE = 3 / 4.0f;

	constexpr auto AUTO_GUN_ORIENT_LERP_ALPHA	  = 0.1f;
	constexpr auto AUTO_GUN_BARREL_TURN_RATE	  = ANGLE(0.35f);
	constexpr auto AUTO_GUN_FIRE_CONSTRAINT_ANGLE = ANGLE(5.6f);

	constexpr auto AUTO_GUN_BARREL_JOINT_INDEX = 9;

	const auto AutoGunChassisJoints		= std::vector<unsigned int>{ 0, 1, 2, 3, 4, 5, 6, 11, 12};
	const auto AutoGunBodyJoints		= std::vector<unsigned int>{ 7, 9 };
	const auto AutoGunClosedHatchJoints = std::vector<unsigned int>{ 10 };
	const auto AutoGunFlashJoints		= std::vector<unsigned int>{ 8 };

	void InitializeAutoGun(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		item.Data = std::array<short, 4>();

		item.Status = ITEM_NOT_ACTIVE;
		item.MeshBits.Clear(AutoGunFlashJoints);
		item.MeshBits.Set(AutoGunBodyJoints);
		item.MeshBits.Set(AutoGunChassisJoints);
	}

	static void SpawnAutoGunSmoke(const Vector3& pos, char shade)
	{
		auto& smoke = SmokeSparks[GetFreeSmokeSpark()];

		auto sphere = BoundingSphere(pos, BLOCK(1 / 64.0f));
		auto smokePos = Random::GeneratePointInSphere(sphere);

		smoke.on = true;
		smoke.sShade = 0;
		smoke.dShade = shade;
		smoke.colFadeSpeed = 4;
		smoke.fadeToBlack = 32;
		smoke.blendMode = BlendMode::Additive;
		smoke.life = smoke.sLife = Random::GenerateInt(40, 44);
		smoke.position.x = smokePos.x;
		smoke.position.y = smokePos.y;
		smoke.position.z = smokePos.z;
		smoke.velocity = Vector3i::Zero;
		smoke.friction = 4;
		smoke.flags = SP_ROTATE;
		smoke.rotAng = Random::GenerateInt(0, 4096);
		smoke.rotAdd = Random::GenerateInt(-32, 32);
		smoke.maxYvel = 0;
		smoke.gravity = Random::GenerateInt(-4, -8);
		smoke.dSize = Random::GenerateInt(24, 40);
		smoke.sSize = smoke.dSize / 4;
		smoke.size = smoke.dSize / 4;
	}

	void ControlAutoGun(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		auto& playerItem = *LaraItem;
		
		if (!TriggerActive(&item))
			return;

		if (TestLastFrame(*&item))
		{
			auto& autoGun = *GetCreatureInfo(&item);

			// Set visible meshes.
			item.MeshBits.Set(AutoGunClosedHatchJoints);
			item.MeshBits.Clear(AutoGunChassisJoints);

			// Assess line of sight.
			auto origin = GameVector(item.Pose.Position, item.RoomNumber);
			auto target = GameVector(GetJointPosition(&playerItem, LM_TORSO), playerItem.RoomNumber);
			bool los = LOS(&origin, &target);

			// Interpolate orientation.
			auto orient = EulerAngles(item.ItemFlags[0], item.ItemFlags[1], 0);
			auto orientTo = los ? Geometry::GetOrientToPoint(origin.ToVector3(), target.ToVector3()) : item.Pose.Orientation;
			orient.Lerp(orientTo, AUTO_GUN_ORIENT_LERP_ALPHA);

			item.ItemFlags[0] = orient.x;
			item.ItemFlags[1] = orient.y;

			auto deltaAngle = orient - orientTo;

			// Handle joint rotation.
			autoGun.JointRotation[0] = orient.y;
			autoGun.JointRotation[1] = orient.x;
			autoGun.JointRotation[2] += item.ItemFlags[2];

			// Fire gunshot.
			if (los &&
				abs(deltaAngle.x) <= AUTO_GUN_FIRE_CONSTRAINT_ANGLE &&
				abs(deltaAngle.y) <= AUTO_GUN_FIRE_CONSTRAINT_ANGLE)
			{
				SoundEffect(SFX_TR4_HK_FIRE, &item.Pose, SoundEnvironment::Land, 0.8f);

				if (GlobalCounter & 1)
				{
					item.MeshBits.Set(AutoGunFlashJoints);

					auto lightColor = Vector3(Random::GenerateFloat(0.75f, 0.85f), Random::GenerateFloat(0.5f, 0.6f), 0.0f) * 255;
					SpawnDynamicLight(origin.x, origin.y, origin.z, 10, lightColor.x, lightColor.y, lightColor.z);

					// Spawn blood.
					if (Random::TestProbability(AUTO_GUN_BLOOD_EFFECT_CHANCE))
					{
						DoDamage(&playerItem, AUTO_GUN_SHOT_DAMAGE);

						auto bloodPos = GetJointPosition(&playerItem, Random::GenerateInt(0, NUM_LARA_MESHES - 1));
						float bloodVel = Random::GenerateFloat(4.0f, 8.0f);
						DoBloodSplat(bloodPos.x, bloodPos.y, bloodPos.z, bloodVel, Random::GenerateAngle(), playerItem.RoomNumber);
					}
					// Spawn ricochet. TODO: Simplify.
					else
					{
						auto pos = target;

						int dx = target.x - origin.x;
						int dy = target.y - origin.y;
						int dz = target.z - origin.z;

						while (true)
						{
							if (abs(dx) >= BLOCK(12))
								break;

							if (abs(dy) >= BLOCK(12))
								break;

							if (abs(dz) >= BLOCK(12))
								break;

							dx *= 2;
							dy *= 2;
							dz *= 2;
						}

						pos.x += dx + GetRandomControl() - 128;
						pos.y += dy + GetRandomControl() - 128;
						pos.z += dz + GetRandomControl() - 128;

						if (!LOS(&origin, &pos))
							TriggerRicochetSpark(pos, Random::GenerateAngle());
					}
				}
				else
				{
					item.MeshBits.Clear(AutoGunFlashJoints);
				}

				if (item.ItemFlags[2] < AUTO_GUN_FIRE_CONSTRAINT_ANGLE)
					item.ItemFlags[2] += AUTO_GUN_BARREL_TURN_RATE;
			}
			// Reset barrel.
			else
			{
				if (item.ItemFlags[2] != 0)
					item.ItemFlags[2] -= AUTO_GUN_BARREL_TURN_RATE;

				item.MeshBits.Clear(AutoGunFlashJoints);
			}

			if (item.ItemFlags[2] != 0)
				SpawnAutoGunSmoke(GetJointPosition(&item, AUTO_GUN_BARREL_JOINT_INDEX).ToVector3(), item.ItemFlags[2] / 16);
		}
		else
		{
			item.MeshBits.Set(AutoGunChassisJoints);
			item.MeshBits.Clear(AutoGunClosedHatchJoints);
			item.MeshBits.Clear(AutoGunFlashJoints);

			AnimateItem(item);
		}		
	}
}
