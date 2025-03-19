#include "framework.h"
#include "Objects/TR4/Entity/tr4_sentry_gun.h"

#include "Game/Animation/Animation.h"
#include "Game/control/box.h"
#include "Game/control/lot.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/Gui.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Math/Math.h"
#include "Objects/objectslist.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Gui;

// NOTES:
// item.ItemFlags[0] = Gun flash duration in frame time.
// item.ItemFlags[1] = Radar angle.
// item.ItemFlags[2] = Barrel angle.
// item.ItemFlags[3] = Object ID of inventory item which deactivates sentry gun if present.

namespace TEN::Entities::TR4
{
	constexpr auto SENTRYGUN_SHOT_DAMAGE = 5;

	const auto SentryGunRadarJoints	  = std::vector<unsigned int>{ 3 };
	const auto SentryGunBarrelJoints  = std::vector<unsigned int>{ 4 };
	const auto SentryGunFuelCanJoints = std::vector<unsigned int>{ 6 };
	const auto SentryGunFlashJoints	  = std::vector<unsigned int>{ 8 };
	const auto SentryGunBodyJoints	  = std::vector<unsigned int>{ 7, 9 };
	
	const auto SentryGunFlameOffset = Vector3i(-140, 0, 0);
	const auto SentryGunBite = CreatureBiteInfo(Vector3::Zero, 8);

	void InitializeSentryGun(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);

		auto& flashDuration = item.ItemFlags[0];
		auto& radarAngle = item.ItemFlags[1];
		auto& barrelAngle = item.ItemFlags[2];
		auto& deactivatorItemObjctID = item.ItemFlags[3];

		flashDuration = 0;
		radarAngle = 768;
		barrelAngle = 0;

		if (deactivatorItemObjctID == 0)
			deactivatorItemObjctID = ID_PUZZLE_ITEM5;
	}

	void ControlSentryGun(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto& item = g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(&item);

		auto& flashDuration = item.ItemFlags[0];
		auto& radarAngle = item.ItemFlags[1];
		auto& barrelAngle = item.ItemFlags[2];
		auto& deactivatorItemObjectID = item.ItemFlags[3];

		int headingAngle = 0;

		// TODO: Why this check?
		if (creature == nullptr)
			return;

		// Was fuel can exploded?
		if (item.MeshBits.Test(SentryGunFuelCanJoints))
		{
			if (flashDuration)
			{
				auto pos = GetJointPosition(item, SentryGunBite);
				SpawnDynamicLight(pos.x, pos.y, pos.z, 4 * flashDuration + 12, 24, 16, 4);
				flashDuration--;
			}

			if (flashDuration & 1)
			{
				item.MeshBits.Set(SentryGunFlashJoints);
			}
			else
			{
				item.MeshBits.Clear(SentryGunFlashJoints);
			}

			if (item.TriggerFlags == 0)
			{
				item.Pose.Position.y -= CLICK(2);

				AI_INFO ai;
				CreatureAIInfo(&item, &ai);

				item.Pose.Position.y += CLICK(2);

				int deltaAngle = ai.angle - creature->JointRotation[0];

				ai.ahead = true;
				if (deltaAngle <= ANGLE(-90.0f) || deltaAngle >= ANGLE(90.0f))
					ai.ahead = false;

				if (Targetable(&item, &ai))
				{
					if (ai.distance < SQUARE(BLOCK(9)))
					{
						if (!g_Gui.IsObjectInInventory(deactivatorItemObjectID) && !flashDuration)
						{
							if (ai.distance <= SQUARE(BLOCK(2)))
							{
								// Throw fire.
								ThrowFire(itemNumber, 7, SentryGunFlameOffset, SentryGunFlameOffset);
								headingAngle = phd_sin((GlobalCounter & 0x1F) * 2048) * 4096; // TODO: Demagic.
							}
							else
							{
								// Shoot player.
								headingAngle = 0;
								flashDuration = 2;

								ShotLara(&item, &ai, SentryGunBite, creature->JointRotation[0], SENTRYGUN_SHOT_DAMAGE);
								SoundEffect(SFX_TR4_AUTOGUNS, &item.Pose);

								barrelAngle += ANGLE(1.5f);
								if (barrelAngle > ANGLE(45.0f))
									barrelAngle = ANGLE(45.0f);
							}
						}

						deltaAngle = (headingAngle + ai.angle) - creature->JointRotation[0];
						if (deltaAngle <= ANGLE(10.0f))
						{
							if (deltaAngle < ANGLE(-10.0f))
								deltaAngle = ANGLE(-10.0f);
						}
						else
						{
							deltaAngle = ANGLE(10.0f);
						}

						creature->JointRotation[0] += deltaAngle;

						CreatureJoint(&item, 1, -ai.xAngle);
					}
				}

				// Rotate barrel.
				barrelAngle -= 32;
				if (barrelAngle < 0)
					barrelAngle = 0;

				creature->JointRotation[3] += barrelAngle;

				// Rotate radar.
				creature->JointRotation[2] += radarAngle;
				if (creature->JointRotation[2] > ANGLE(90.0f) || creature->JointRotation[2] < ANGLE(-90.0f))
					radarAngle = -radarAngle;
			}
			else
			{
				// Stuck sentry gun.
				CreatureJoint(&item, 0, (GetRandomControl() & 0x7FF) - 1024);
				CreatureJoint(&item, 1, ANGLE(45.0f));
				CreatureJoint(&item, 2, (GetRandomControl() & 0x3FFF) - ANGLE(45.0f));
			}
		}
		else
		{
			ExplodingDeath(itemNumber, BODY_DO_EXPLOSION | BODY_NO_BOUNCE);
			DisableEntityAI(itemNumber);
			KillItem(itemNumber);

			item.Flags |= 1;
			item.Status = ITEM_DEACTIVATED;

			RemoveAllItemsInRoom(item.RoomNumber, ID_SMOKE_EMITTER_BLACK);

			TriggerExplosionSparks(item.Pose.Position.x, item.Pose.Position.y - CLICK(3), item.Pose.Position.z, 3, -2, 0, item.RoomNumber);
			for (int i = 0; i < 2; i++)
				TriggerExplosionSparks(item.Pose.Position.x, item.Pose.Position.y - CLICK(3), item.Pose.Position.z, 3, -1, 0, item.RoomNumber);

			SoundEffect(SFX_TR4_EXPLOSION1, &item.Pose, SoundEnvironment::Land, 1.5f);
			SoundEffect(SFX_TR4_EXPLOSION2, &item.Pose);
		}
	}
}
