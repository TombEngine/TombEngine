#include "framework.h"
#include "Objects/TR2/Entity/tr2_skidman.h"

#include "Game/Animation/Animation.h"
#include "Game/items.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/Sphere.h"
#include "Game/control/box.h"
#include "Game/control/lot.h"
#include "Game/effects/smoke.h"
#include "Game/itemdata/creature_info.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Objects/TR2/Vehicles/skidoo.h"
#include "Objects/TR2/Vehicles/skidoo_info.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Sphere;
using namespace TEN::Effects::Smoke;

namespace TEN::Entities::Creatures::TR2
{
	constexpr auto SMAN_WAIT_RANGE = SQUARE(BLOCK(4));

	const auto SkidooBiteLeft  = CreatureBiteInfo(Vector3(240, -80, 540), 0);
	const auto SkidooBiteRight = CreatureBiteInfo(Vector3(-240, -80, 540), 0);
	const auto SkidooBiteSmokeLeft	= CreatureBiteInfo(Vector3(240, -80, 450), 0);
	const auto SkidooBiteSmokeRight = CreatureBiteInfo(Vector3(-240, -80, 450), 0);

	constexpr auto SKIDOO_MAN_TURN_RATE_MIN = ANGLE(2.0f);
	constexpr auto SKIDOO_MAN_TARGET_ANGLE	= ANGLE(30.0f);

	enum SnowmobileManState
	{
		// No state 0.
		SMAN_STATE_WAIT = 1,
		SMAN_STATE_MOVING = 2,
		SMAN_STATE_START_LEFT = 3,
		SMAN_STATE_START_RIGHT = 4,
		SMAN_STATE_LEFT = 5,
		SMAN_STATE_RIGHT = 6,
		SMAN_STATE_DEATH = 7
	};

	// TODO
	enum SkidooManAnim
	{
		SMAN_ANIM_DEATH = 10
	};

	static void CreateSkidooGun(ItemInfo& riderItem)
	{
		int skidooItemNumber = CreateItem();
		if (skidooItemNumber == NO_VALUE)
		{
			TENLog("Failed to create ID_SNOWMOBILE_GUN from ID_SNOWMOBILE_DRIVER.", LogLevel::Warning);
			return;
		}

		auto& skidooGunItem = g_Level.Items[skidooItemNumber];

		skidooGunItem.Pose.Position = riderItem.Pose.Position;
		skidooGunItem.Pose.Orientation.y = riderItem.Pose.Orientation.y;
		skidooGunItem.RoomNumber = riderItem.RoomNumber;
		skidooGunItem.ObjectNumber = ID_SNOWMOBILE_GUN;
		skidooGunItem.Model.Color = riderItem.Model.Color;
		skidooGunItem.Flags = IFLAG_ACTIVATION_MASK;

		InitializeItem(skidooItemNumber); g_Level.NumItems++;

		// Register snowmobile gun for driver to control.
		riderItem.ItemFlags[0] = skidooItemNumber;
	}

	void InitializeSkidooMan(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (item.Flags & IFLAG_REVERSE)
		{
			item.Status = ITEM_NOT_ACTIVE;
		}
		else
		{
			item.Status = ITEM_INVISIBLE;
		}
	}

	void SkidooManCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!TestBoundsCollide(&item, laraItem, coll->Setup.Radius))
			return;

		if (!HandleItemSphereCollision(item, *laraItem))
			return;

		if (coll->Setup.EnableObjectPush)
		{
			if (item.Animation.Velocity.z > 0.0f)
			{
				ItemPushItem(&item, laraItem, coll, coll->Setup.EnableSpasm, 0);
			}
			else
			{
				ItemPushItem(&item, laraItem, coll, false, 0);
			}
		}

		if (Lara.Context.Vehicle == NO_VALUE && item.Animation.Velocity.z > 0.0f)
			DoDamage(laraItem, 100);
	}

	void SkidooManControl(short riderItemNumber)
	{
		auto& riderItem = g_Level.Items[riderItemNumber];
		if (!riderItem.ItemFlags[0])
		{
			// Create snowmobile.
			CreateSkidooGun(riderItem);
			if (!riderItem.ItemFlags[0])
				TENLog("Skidoo rider data does not contain skidoo item ID.", LogLevel::Error);

			return;
		}

		int skidooItemNumber = (short)riderItem.ItemFlags[0];
		auto* skidooItem = &g_Level.Items[skidooItemNumber];

		if (!CreatureActive(skidooItemNumber))
			return;

		if (!skidooItem->Data)
		{
			EnableEntityAI(skidooItemNumber, true);
			skidooItem->Status = ITEM_ACTIVE;
		}

		auto* creature = GetCreatureInfo(skidooItem);

		short headingAngle = 0;

		if (creature->MuzzleFlash[0].Delay != 0)
			creature->MuzzleFlash[0].Delay--;

		if (creature->MuzzleFlash[1].Delay != 0)
			creature->MuzzleFlash[1].Delay--;

		AI_INFO ai;
		if (skidooItem->HitPoints <= 0)
		{
			if (riderItem.Animation.ActiveState != SMAN_STATE_DEATH)
			{
				SetAnimation(riderItem, SMAN_ANIM_DEATH);
				riderItem.Pose.Position = skidooItem->Pose.Position;
				riderItem.Pose.Orientation.y = skidooItem->Pose.Orientation.y;
				riderItem.RoomNumber = skidooItem->RoomNumber;

				if (Lara.TargetEntity == skidooItem)
					Lara.TargetEntity = nullptr;
			}
			else
			{
				AnimateItem(riderItem);
			}

			if (skidooItem->Animation.ActiveState == SMAN_STATE_MOVING || skidooItem->Animation.ActiveState == SMAN_STATE_WAIT)
			{
				skidooItem->Animation.TargetState = SMAN_STATE_WAIT;
			}
			else
			{
				skidooItem->Animation.TargetState = SMAN_STATE_MOVING;
			}
		}
		else
		{
			CreatureAIInfo(skidooItem, &ai);

			GetCreatureMood(skidooItem, &ai, true);
			CreatureMood(skidooItem, &ai, true);

			headingAngle = CreatureTurn(skidooItem, ANGLE(3.0f));

			switch (skidooItem->Animation.ActiveState)
			{
			case SMAN_STATE_WAIT:
				if (creature->Mood == MoodType::Bored)
				{
					break;
				}
				else if (abs(ai.angle) < SKIDOO_MAN_TARGET_ANGLE && ai.distance < SMAN_WAIT_RANGE)
				{
					break;
				}

				skidooItem->Animation.TargetState = SMAN_STATE_MOVING;
				break;

			case SMAN_STATE_MOVING:
				if (creature->Mood == MoodType::Bored)
				{
					skidooItem->Animation.TargetState = SMAN_STATE_WAIT;
				}
				else if (abs(ai.angle) < SKIDOO_MAN_TARGET_ANGLE && ai.distance < SMAN_WAIT_RANGE)
				{
					skidooItem->Animation.TargetState = SMAN_STATE_WAIT;
				}
				else if (headingAngle < -SKIDOO_MAN_TURN_RATE_MIN)
				{
					skidooItem->Animation.TargetState = SMAN_STATE_START_LEFT;
				}
				else if (headingAngle > SKIDOO_MAN_TURN_RATE_MIN)
				{
					skidooItem->Animation.TargetState = SMAN_STATE_START_RIGHT;
				}

				break;

			case SMAN_STATE_START_LEFT:
			case SMAN_STATE_LEFT:
				if (headingAngle < -SKIDOO_MAN_TURN_RATE_MIN)
				{
					skidooItem->Animation.TargetState = SMAN_STATE_LEFT;
				}
				else
				{
					skidooItem->Animation.TargetState = SMAN_STATE_MOVING;
				}

				break;

			case SMAN_STATE_START_RIGHT:
			case SMAN_STATE_RIGHT:
				if (headingAngle < -SKIDOO_MAN_TURN_RATE_MIN)
				{
					skidooItem->Animation.TargetState = SMAN_STATE_LEFT;
				}
				else
				{
					skidooItem->Animation.TargetState = SMAN_STATE_MOVING;
				}

				break;
			}
		}

		if (riderItem.Animation.ActiveState != SMAN_STATE_DEATH)
		{
			if (creature->Flags == 0 && abs(ai.angle) < SKIDOO_MAN_TARGET_ANGLE && creature->Enemy->HitPoints > 0)
			{
				int damage = (creature->Enemy->IsLara() && GetLaraInfo(creature->Enemy)->Context.Vehicle != NO_VALUE) ? 10 : 50;
				
				ShotLara(skidooItem, &ai, SkidooBiteLeft, 0, damage);

				auto jointPos = GetJointPosition(skidooItem, SkidooBiteSmokeLeft);
				SpawnGunSmokeParticles(jointPos.ToVector3(), Vector3::Zero, skidooItem->RoomNumber, 1, LaraWeaponType::Snowmobile, 16);

				creature->MuzzleFlash[0].Bite = SkidooBiteLeft;
				creature->MuzzleFlash[0].Delay = 1;
				creature->MuzzleFlash[0].SwitchToMuzzle2 = true;
				creature->MuzzleFlash[0].ApplyXRotation = false;
				creature->MuzzleFlash[0].UseSmoke = false;

				ShotLara(skidooItem, &ai, SkidooBiteRight, 0, damage);

				jointPos = GetJointPosition(skidooItem, SkidooBiteSmokeRight);
				SpawnGunSmokeParticles(jointPos.ToVector3(), Vector3::Zero, skidooItem->RoomNumber, 1, LaraWeaponType::Snowmobile, 16);

				creature->MuzzleFlash[1].Bite = SkidooBiteRight;
				creature->MuzzleFlash[1].Delay = 1;
				creature->MuzzleFlash[1].SwitchToMuzzle2 = true;
				creature->MuzzleFlash[1].ApplyXRotation = false;
				creature->MuzzleFlash[1].UseSmoke = false;

				creature->Flags = 4;
			}

			if (creature->Flags != 0)
			{
				SoundEffect(SFX_TR4_BADDY_UZI, &skidooItem->Pose);
				creature->Flags--;
			}
		}

		if (skidooItem->Animation.ActiveState == SMAN_STATE_WAIT)
		{
			SoundEffect(SFX_TR2_VEHICLE_SNOWMOBILE_IDLE, &skidooItem->Pose);
			creature->JointRotation[0] = 0;
		}
		else
		{
			creature->JointRotation[0] = (creature->JointRotation[0] == 1) ? 2 : 1;
			DoSnowEffect(skidooItem);
			SoundEffect(SFX_TR2_VEHICLE_SNOWMOBILE_IDLE, &skidooItem->Pose, SoundEnvironment::Land, 0.5f + skidooItem->Animation.Velocity.z / 100.0f); // SKIDOO_MAX_VELOCITY.  TODO: Check actual sound!
		}

		CreatureAnimation(skidooItemNumber, headingAngle, 0);

		if (riderItem.Animation.ActiveState != SMAN_STATE_DEATH)
		{
			riderItem.Pose.Position = skidooItem->Pose.Position;
			riderItem.Pose.Orientation.y = skidooItem->Pose.Orientation.y;

			if (skidooItem->RoomNumber != riderItem.RoomNumber)
				ItemNewRoom(riderItemNumber, skidooItem->RoomNumber);

			riderItem.Animation.AnimNumber = skidooItem->Animation.AnimNumber;
			riderItem.Animation.FrameNumber = skidooItem->Animation.FrameNumber;
		}
		else if (riderItem.Status == ITEM_DEACTIVATED &&
			skidooItem->Animation.Velocity.z == 0 &&
			skidooItem->Animation.Velocity.y == 0)
		{
			RemoveActiveItem(riderItemNumber);
			riderItem.Collidable = false;
			riderItem.HitPoints = NOT_TARGETABLE;
			riderItem.Flags |= IFLAG_INVISIBLE;

			DisableEntityAI(skidooItemNumber);
			skidooItem->ObjectNumber = ID_SNOWMOBILE;
			skidooItem->Status = ITEM_DEACTIVATED;

			InitializeSkidoo(skidooItemNumber);
			if (skidooItem->Data.is<SkidooInfo>())
			{
				auto* skidooData = (SkidooInfo*)skidooItem->Data;
				skidooData->Armed = true;
			}
		}
	}
}
