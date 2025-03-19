#include "framework.h"
#include "Objects/TR3/Entity/TwinAutoGun.h"

#include "Game/control/box.h"
#include "Game/control/los.h"
#include "Game/control/lot.h"
#include "Game/effects/tomb4fx.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Specific/level.h"

// NOTES:
// ItemFlags[0]: duplicate shooting light diffusion.
// ItemFlags[3]: indicate if turret is hostile or neutral.

namespace TEN::Entities::Creatures::TR3
{
	constexpr auto TWIN_AUTO_GUN_SHOT_DAMAGE	= 5;
	constexpr auto TWIN_AUTO_GUN_HIT_POINTS_MAX = 28;

	const auto TwinAutoGunLeftBite	= CreatureBiteInfo(Vector3(110.0f, -30.0f, 530.0f), 2);
	const auto TwinAutoGunRightBite = CreatureBiteInfo(Vector3(-110.0f, -30.0f, 530.0f), 2);

	enum TwinAutoGunAnim
	{
		TWIN_AUTO_GUN_ANIM_FIRE = 0,
		TWIN_AUTO_GUN_ANIM_IDLE = 1
	};

	enum TwinAutoGunState
	{
		TWIN_AUTO_GUN_STATE_FIRE = 0,
		TWIN_AUTO_GUN_STATE_IDLE = 1
	};

	void InitializeTwinAutoGun(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		SetAnimation(item, TWIN_AUTO_GUN_ANIM_IDLE);
		item.ItemFlags[0] = 0;
		item.ItemFlags[3] = 0;		
	}

	void ControlTwinAutoGun(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		auto& playerItem = *LaraItem;

		if (!CreatureActive(itemNumber))
			return;

		auto& autoGun = *GetCreatureInfo(&item);

		if (item.HitStatus || item.HitPoints < TWIN_AUTO_GUN_HIT_POINTS_MAX)
			item.ItemFlags[3] = 1;

		if (autoGun.MuzzleFlash[0].Delay >= 1)
			autoGun.MuzzleFlash[0].Delay--;

		if (autoGun.MuzzleFlash[1].Delay >= 1)
			autoGun.MuzzleFlash[1].Delay--;

		if (item.HitPoints <= 0)
		{
			ExplodingDeath(itemNumber, BODY_DO_EXPLOSION | BODY_NO_BOUNCE);
			DisableEntityAI(itemNumber);
			KillItem(itemNumber);

			item.Flags |= 1;
			item.Status = ITEM_DEACTIVATED;

			TriggerExplosionSparks(item.Pose.Position.x, item.Pose.Position.y - CLICK(3), item.Pose.Position.z, 3, -2, 0, item.RoomNumber);
			for (int i = 0; i < 2; i++)
				TriggerExplosionSparks(item.Pose.Position.x, item.Pose.Position.y - CLICK(3), item.Pose.Position.z, 3, -1, 0, item.RoomNumber);

			SoundEffect(SFX_TR4_EXPLOSION1, &item.Pose, SoundEnvironment::Land, 1.5f);
			SoundEffect(SFX_TR4_EXPLOSION2, &item.Pose);
		}

		if (!item.ItemFlags[3])
			return;

		AI_INFO ai;
		CreatureAIInfo(&item, &ai);
		short tiltAngle = ai.xAngle;

		auto origin = GameVector(GetJointPosition(&item, 2), item.RoomNumber);
		auto target = GameVector(GetJointPosition(&playerItem, LM_TORSO), playerItem.RoomNumber);
		bool los = LOS(&origin, &target);

		switch (item.Animation.ActiveState)
		{
		case TWIN_AUTO_GUN_STATE_FIRE:
			if (!los)
			{
				item.Animation.TargetState = TWIN_AUTO_GUN_STATE_IDLE;
			}
			else if (item.Animation.FrameNumber == 0)
			{
				item.ItemFlags[0] = 1;

				if (autoGun.MuzzleFlash[0].Delay == 0)
				{
					DoDamage(&playerItem, TWIN_AUTO_GUN_SHOT_DAMAGE);

					auto bloodPos = GetJointPosition(&playerItem, Random::GenerateInt(0, NUM_LARA_MESHES - 1));
					float bloodVel = Random::GenerateFloat(4.0f, 8.0f);
					DoBloodSplat(bloodPos.x, bloodPos.y, bloodPos.z, bloodVel, Random::GenerateAngle(), playerItem.RoomNumber);

					auto lightPos = GetJointPosition(item, TwinAutoGunLeftBite);
					auto lightColor = Color(
						Random::GenerateFloat(0.75f, 1.0f),
						Random::GenerateFloat(0.5f, 0.6f),
						Random::GenerateFloat(0.0f, 0.25f));
					SpawnDynamicLight(lightPos.x, lightPos.y, lightPos.z, 2 * item.ItemFlags[0] + 8, lightColor.R() * UCHAR_MAX, lightColor.G() * UCHAR_MAX, lightColor.B() * UCHAR_MAX);

					autoGun.MuzzleFlash[0].Bite = TwinAutoGunLeftBite;
					autoGun.MuzzleFlash[0].SwitchToMuzzle2 = true;
					autoGun.MuzzleFlash[0].ApplyXRotation = false;
					autoGun.MuzzleFlash[0].ApplyZRotation = true;
					autoGun.MuzzleFlash[0].UseSmoke = true;
					autoGun.MuzzleFlash[0].Delay = 1;
				}

				if (autoGun.MuzzleFlash[1].Delay == 0)
				{
					DoDamage(&playerItem, TWIN_AUTO_GUN_SHOT_DAMAGE);

					auto bloodPos = GetJointPosition(&playerItem, Random::GenerateInt(0, NUM_LARA_MESHES - 1));
					float bloodVel = Random::GenerateFloat(4.0f, 8.0f);
					DoBloodSplat(bloodPos.x, bloodPos.y, bloodPos.z, bloodVel, Random::GenerateAngle(), playerItem.RoomNumber);

					auto lightPos = GetJointPosition(item, TwinAutoGunRightBite);
					auto lightColor = Color(
						Random::GenerateFloat(0.75f, 1.0f),
						Random::GenerateFloat(0.5f, 0.6f),
						Random::GenerateFloat(0.0f, 0.25f));
					SpawnDynamicLight(lightPos.x, lightPos.y, lightPos.z, 2 * item.ItemFlags[0] + 8, lightColor.R() * UCHAR_MAX, lightColor.G() * UCHAR_MAX, lightColor.B() * UCHAR_MAX);

					autoGun.MuzzleFlash[1].Bite = TwinAutoGunRightBite;
					autoGun.MuzzleFlash[1].SwitchToMuzzle2 = true;
					autoGun.MuzzleFlash[1].ApplyXRotation = false;
					autoGun.MuzzleFlash[1].ApplyZRotation = true;
					autoGun.MuzzleFlash[1].UseSmoke = true;					
					autoGun.MuzzleFlash[1].Delay = 1;
				}

				SoundEffect(SFX_TR4_HK_FIRE, &item.Pose, SoundEnvironment::Land, 0.8f);				
			}

			break;

		case TWIN_AUTO_GUN_STATE_IDLE:
			if (los && item.ItemFlags[0] == 0)
				item.Animation.TargetState = TWIN_AUTO_GUN_STATE_FIRE;

			else if (item.ItemFlags[0])
			{
				if (item.AIBits == MODIFY)
				{
					item.ItemFlags[0] = 1;
					item.Animation.TargetState = TWIN_AUTO_GUN_STATE_FIRE;
				}
				else
				{
					item.ItemFlags[0] = 0;
					item.ItemFlags[1] = 0;
					autoGun.Flags |= 0;
				}
			}

			break;
		}

		short deltaAngle = Geometry::GetShortestAngle(autoGun.JointRotation[0], ai.angle);
		if (deltaAngle > ANGLE(10.0f))
		{
			deltaAngle = ANGLE(10.0f);
		}
		else if (deltaAngle < ANGLE(-10.0f))
		{
			deltaAngle = ANGLE(-10.0f);
		}

		autoGun.JointRotation[0] += deltaAngle;
		CreatureJoint(&item, 1, tiltAngle);
		AnimateItem(item);
	}

	void HitTwinAutoGun(ItemInfo& target, ItemInfo& source, std::optional<GameVector> pos, int damage, bool isExplosive, int jointIndex)
	{
		DefaultItemHit(target, source, pos, damage, isExplosive, jointIndex);
	}
}
