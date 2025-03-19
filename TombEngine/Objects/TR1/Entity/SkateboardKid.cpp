#include "framework.h"
#include "Objects/TR1/Entity/SkateboardKid.h"

#include "Game/Animation/Animation.h"
#include "Game/control/box.h"
#include "Game/control/lot.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Math/Math.h"

using namespace TEN::Animation;
using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR1
{
	// NOTES:
	// ItemFlags[0] = skateboard entity ID.

	constexpr auto KID_IDLE_SHOT_DAMAGE	 = 30;
	constexpr auto KID_SKATE_SHOT_DAMAGE = 20;

	constexpr auto KID_CLOSE_RANGE = SQUARE(BLOCK(1));
	constexpr auto KID_SKATE_RANGE = SQUARE(BLOCK(2.5f));
	constexpr auto KID_IDLE_RANGE  = SQUARE(BLOCK(4));

	constexpr auto KID_ACCEL_CHANCE = 1 / 128.0f;

	constexpr auto KID_TURN_RATE_MAX = ANGLE(4.0f);

	const auto KidGunBiteRight = CreatureBiteInfo(Vector3(0, 170, 34), 7);
	const auto KidGunBiteLeft  = CreatureBiteInfo(Vector3(0, 170, 37), 4);

	enum SkateKidState
	{
		KID_STATE_IDLE = 0,
		KID_STATE_SHOOT_IDLE = 1,
		KID_STATE_SKATE = 2,
		KID_STATE_SKATE_ACCEL = 3,
		KID_STATE_SKATE_SHOOT = 4,
		KID_STATE_DEATH = 5
	};

	enum SkateKidAnim
	{
		KID_ANIM_IDLE_TO_SKATE_CONT = 0,
		KID_ANIM_IDLE_TO_SKATE_END = 1,
		KID_ANIM_SKATE_ACCEL_END = 2,
		KID_ANIM_SKATE_ACCEL_CONT = 3,
		KID_ANIM_SKATE_TO_IDLE_START = 4,
		KID_ANIM_SKATE_TO_IDLE_CONT = 5,
		KID_ANIM_SKATE_TO_IDLE_END = 6,
		KID_ANIM_IDLE = 7,
		KID_ANIM_SKATE_ACCEL_START = 8,
		KID_ANIM_IDLE_TO_SKATE_START = 9, // Unused.
		KID_ANIM_IDLE_SHOOT = 10,
		KID_ANIM_SKATE_SHOOT = 11,
		KID_ANIM_SKATE = 12,
		KID_ANIM_DEATH = 13
	};

	static void SpawnSkateboard(ItemInfo& item)
	{
		int skateItemNumber = CreateItem();
		if (skateItemNumber == NO_VALUE)
			return;

		auto& skate = g_Level.Items[skateItemNumber];

		skate.ObjectNumber = ID_SKATEBOARD;
		skate.Pose.Position = item.Pose.Position;
		skate.Pose.Orientation = item.Pose.Orientation;
		skate.StartPose = item.StartPose;
		skate.Model.Color = item.Model.Color;
		skate.RoomNumber = item.RoomNumber;

		InitializeItem(skateItemNumber);
		AddActiveItem(skateItemNumber);

		skate.Active = false;
		skate.Status = ITEM_INVISIBLE;
		item.ItemFlags[0] = skateItemNumber;
	}

	void InitializeSkateboardKid(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		InitializeCreature(itemNumber);
		SpawnSkateboard(item);
	}

	static void SkateboardKidShoot(ItemInfo& item, AI_INFO& ai, short headingAngle, int damage)
	{
		auto& creature = *GetCreatureInfo(&item);

		if (creature.Flags == 0 && Targetable(&item, &ai))
		{
			ShotLara(&item, &ai, KidGunBiteLeft, headingAngle, damage);
			creature.MuzzleFlash[0].Bite = KidGunBiteLeft;
			creature.MuzzleFlash[0].Delay = 2;

			ShotLara(&item, &ai, KidGunBiteRight, headingAngle, damage);
			creature.MuzzleFlash[1].Bite = KidGunBiteRight;
			creature.MuzzleFlash[1].Delay = 2;
			creature.Flags = 1;
		}

		if (creature.Mood == MoodType::Escape || ai.distance < KID_CLOSE_RANGE)
			item.Animation.RequiredState = KID_STATE_SKATE;
	}

	void ControlSkateboardKid(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto& item = g_Level.Items[itemNumber];

		if (item.ItemFlags[0] == NO_VALUE)
		{
			TENLog("Failed to do the skateboard kid control (itemNumber: " + std::to_string(itemNumber) + "), the skateboard itemNumber is missing, probably failed to be created !");
			return;
		}

		auto& creature = *GetCreatureInfo(&item);
		auto& skateItem = g_Level.Items[item.ItemFlags[0]];
		short headingAngle = 0;
		auto extraHeadRot = EulerAngles::Identity;
		auto extraTorsoRot = EulerAngles::Identity;

		if (skateItem.Status == ITEM_INVISIBLE)
		{
			skateItem.Active = true;
			skateItem.Status = ITEM_ACTIVE;
		}

		for (auto& flash : creature.MuzzleFlash)
		{
			if (flash.Delay != 0)
				flash.Delay--;
		}

		if (item.HitPoints <= 0 && item.Animation.ActiveState != KID_STATE_DEATH)
		{
			creature.MaxTurn = 0;
			SetAnimation(item, KID_ANIM_DEATH);
		}
		else
		{
			AI_INFO ai;
			CreatureAIInfo(&item, &ai);

			if (ai.ahead)
			{
				extraHeadRot.y = ai.angle / 2;
				extraTorsoRot.x = ai.xAngle / 2;
				extraTorsoRot.y = ai.angle / 2;
			}

			GetCreatureMood(&item, &ai, false);
			CreatureMood(&item, &ai, false);

			headingAngle = CreatureTurn(&item, creature.MaxTurn);

			switch (item.Animation.ActiveState)
			{
			case KID_STATE_IDLE:
				creature.MaxTurn = KID_TURN_RATE_MAX;
				creature.Flags = 0;

				if (item.Animation.RequiredState != NO_VALUE)
				{
					item.Animation.TargetState = item.Animation.RequiredState;
				}
				else if (Targetable(&item, &ai))
				{
					item.Animation.TargetState = KID_STATE_SHOOT_IDLE;
				}
				else
				{
					item.Animation.TargetState = KID_STATE_SKATE;
				}

				break;

			case KID_STATE_SKATE:
				creature.Flags = 0;

				if (Random::TestProbability(KID_ACCEL_CHANCE))
				{
					item.Animation.TargetState = KID_STATE_SKATE_ACCEL;
				}
				else if (Targetable(&item, &ai))
				{
					if (ai.distance > KID_SKATE_RANGE && ai.distance < KID_IDLE_RANGE &&
						creature.Mood != MoodType::Escape)
					{
						item.Animation.TargetState = KID_STATE_IDLE;
					}
					else
					{
						item.Animation.TargetState = KID_STATE_SKATE_SHOOT;
					}
				}

				break;

			case KID_STATE_SKATE_ACCEL:
				if (Random::TestProbability(KID_ACCEL_CHANCE))
					item.Animation.TargetState = KID_STATE_SKATE;

				break;

			case KID_STATE_SHOOT_IDLE:
				SkateboardKidShoot(item, ai, extraHeadRot.y, KID_IDLE_SHOT_DAMAGE);
				break;

			case KID_STATE_SKATE_SHOOT:
				SkateboardKidShoot(item, ai, extraHeadRot.y, KID_SKATE_SHOT_DAMAGE);
				break;
			}
		}

		skateItem.Animation.AnimNumber = item.Animation.AnimNumber;
		skateItem.Animation.FrameNumber = item.Animation.FrameNumber;
		skateItem.Pose.Position = item.Pose.Position;
		skateItem.Pose.Orientation = item.Pose.Orientation;
		UpdateItemRoom(item.ItemFlags[0]);
		AnimateItem(skateItem);

		CreatureJoint(&item, 0, extraHeadRot.y);
		CreatureJoint(&item, 1, extraTorsoRot.x);
		CreatureJoint(&item, 2, extraTorsoRot.y);
		CreatureAnimation(itemNumber, headingAngle, 0);
	}
}
