#include "framework.h"
#include "Objects/TR4/Entity/tr4_sas.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/control/volume.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_fire.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_one_gun.h"
#include "Game/people.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Objects/Generic/Object/objects.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;
using namespace TEN::Control::Volumes;
using namespace TEN::Input;
using namespace TEN::Math;

namespace TEN::Entities::TR4
{
	constexpr auto SAS_SHOT_DAMAGE = 15;
	
	constexpr auto SAS_WALK_RANGE  = SQUARE(BLOCK(2));
	constexpr auto SAS_SHOOT_RANGE = SQUARE(BLOCK(3));

	const auto SasGunBite = CreatureBiteInfo(Vector3(0, 420, 80), 7);

	const auto SasDragBodyPosition = Vector3i(0, 0, -460);
	const auto SasDragBounds = ObjectCollisionBounds
	{
		GameBoundingBox(
			-BLOCK(0.25f), BLOCK(0.25f),
			-100, 100,
			-BLOCK(0.5f), -460),
		std::pair(
			EulerAngles(ANGLE(-10.0f), ANGLE(-30.0f), 0),
			EulerAngles(ANGLE(10.0f), ANGLE(30.0f), 0))
	};

	enum SasState
	{
		// No state 0.
		SAS_STATE_IDLE = 1,
		SAS_STATE_WALK = 2,
		SAS_STATE_RUN = 3,
		SAS_STATE_WAIT = 4,
		SAS_STATE_SIGHT_SHOOT = 5,
		SAS_STATE_WALK_SHOOT = 6,
		SAS_STATE_DEATH = 7,
		SAS_STATE_SIGHT_AIM = 8,
		SAS_STATE_WALK_AIM = 9,
		SAS_STATE_HOLD_AIM = 10,
		SAS_STATE_HOLD_SHOOT = 11,
		SAS_STATE_KNEEL_AIM = 12,
		SAS_STATE_KNEEL_SHOOT = 13,
		SAS_STATE_KNEEL_STOP = 14,
		SAS_STATE_HOLD_PREPARE_GRENADE = 15,
		SAS_STATE_HOLD_SHOOT_GRENADE = 16,
		SAS_STATE_BLIND = 17
	};

	enum SasAnim
	{
		SAS_ANIM_WALK = 0,
		SAS_ANIM_RUN = 1,
		SAS_ANIM_SIGHT_SHOOT = 2,
		SAS_ANIM_STAND_TO_SIGHT_AIM = 3,
		SAS_ANIM_WALK_TO_WALK_AIM = 4,
		SAS_ANIM_RUN_TO_WALK = 5,
		SAS_ANIM_WALK_SHOOT = 6,
		SAS_ANIM_SIGHT_AIM_TO_STAND = 7,
		SAS_ANIM_WALK_AIM_TO_STAND = 8,
		SAS_ANIM_STAND_TO_RUN = 9,
		SAS_ANIM_STAND_TO_WAIT = 10,
		SAS_ANIM_STAND_TO_WALK = 11,
		SAS_ANIM_STAND = 12,
		SAS_ANIM_WAIT_TO_STAND = 13,
		SAS_ANIM_WAIT = 14,
		SAS_ANIM_WAIT_TO_SIGHT_AIM = 15,
		SAS_ANIM_WALK_TO_RUN = 16,
		SAS_ANIM_WALK_TO_STAND = 17,
		SAS_ANIM_WALK_AIM_TO_WALK = 18,
		SAS_ANIM_DEATH = 19,
		SAS_ANIM_STAND_TO_HOLD_AIM = 20,
		SAS_ANIM_HOLD_SHOOT = 21,
		SAS_ANIM_HOLD_AIM_TO_STAND = 22,
		SAS_ANIM_HOLD_PREPARE_GRENADE = 23,
		SAS_ANIM_HOLD_SHOOT_GRENADE = 24,
		SAS_ANIM_STAND_TO_KNEEL_AIM = 25,
		SAS_ANIM_KNEEL_SHOOT = 26,
		SAS_ANIM_KNEEL_AIM_TO_STAND = 27,
		SAS_ANIM_BLIND = 28,
		SAS_ANIM_BLIND_TO_STAND = 29
	};

	void InitializeSas(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(item, SAS_ANIM_STAND);
	}

	void InitializeInjuredSas(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (item.TriggerFlags)
		{
			item.Animation.AnimNumber = 0;
			item.Animation.TargetState = item.Animation.ActiveState = 1;
		}
		else
		{
			item.Animation.AnimNumber = 3;
			item.Animation.TargetState = item.Animation.ActiveState = 4;
		}

		item.Animation.FrameNumber = 0;
	}

	void SasControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto& item = g_Level.Items[itemNumber];
		auto& creature = *(CreatureInfo*)item.Data;
		auto& enemy = creature.Enemy;

		short tilt = 0;
		short angle = 0;
		short joint0 = 0;
		short joint1 = 0;
		short joint2 = 0;

		if (creature.MuzzleFlash[0].Delay != 0)
			creature.MuzzleFlash[0].Delay--;

		if (item.HitPoints > 0)
		{
			if (item.AIBits)
				GetAITarget(&creature);
			else
				creature.Enemy = LaraItem;

			AI_INFO AI;
			CreatureAIInfo(&item, &AI);

			float distance2D = 0;
			int angle = 0;
			if (creature.Enemy->IsLara())
			{
				angle = AI.angle;
				distance2D = AI.distance;
			}
			else
			{
				distance2D = Vector2::Distance(
					Vector2(item.Pose.Position.x, item.Pose.Position.z),
					Vector2(LaraItem->Pose.Position.x, LaraItem->Pose.Position.z));
			}

			GetCreatureMood(&item, &AI, !creature.Enemy->IsLara());

			// Vehicle handling
			if (Lara.Context.Vehicle != NO_VALUE && AI.bite)
				creature.Mood = MoodType::Escape;

			CreatureMood(&item, &AI, !creature.Enemy->IsLara());
			angle = CreatureTurn(&item, creature.MaxTurn);

			if (item.HitStatus)
				AlertAllGuards(itemNumber);

			int angle1 = 0;
			int angle2 = 0;

			switch (item.Animation.ActiveState)
			{
			case SAS_STATE_IDLE:
				creature.MaxTurn = 0;
				creature.Flags = 0;
				joint2 = angle;

				if (item.Animation.AnimNumber == SAS_ANIM_WALK_TO_STAND)
				{
					if (abs(AI.angle) < ANGLE(10.0f))
						item.Pose.Orientation.y += AI.angle;
					else if (AI.angle < 0)
						item.Pose.Orientation.y -= ANGLE(10.0f);
					else
						item.Pose.Orientation.y += ANGLE(10.0f);
				}
				else if (item.AIBits & MODIFY || Lara.Context.Vehicle != NO_VALUE)
				{
					if (abs(AI.angle) < ANGLE(2.0f))
						item.Pose.Orientation.y += AI.angle;
					else if (AI.angle < 0)
						item.Pose.Orientation.y -= ANGLE(2.0f);
					else
						item.Pose.Orientation.y += ANGLE(2.0f);
				}

				if (item.AIBits & GUARD)
				{
					joint2 = AIGuard(&creature);

					if (!(GetRandomControl() & 0xFF))
					{
						if (item.Animation.ActiveState == SAS_STATE_IDLE)
							item.Animation.TargetState = SAS_STATE_WAIT;
						else
							item.Animation.TargetState = SAS_STATE_IDLE;
					}
				}
				else if (item.AIBits & PATROL1 &&
					item.AIBits != MODIFY &&
					Lara.Context.Vehicle == NO_VALUE)
				{
					item.Animation.TargetState = SAS_STATE_WALK;
					joint2 = 0;
				}
				else if (Targetable(&item, &AI))
				{
					if (AI.distance >= SAS_SHOOT_RANGE &&
						AI.zoneNumber == AI.enemyZone)
					{
						if (item.AIBits != MODIFY)
							item.Animation.TargetState = SAS_STATE_WALK;
					}
					else if (Random::TestProbability(1 / 2.0f))
					{
						item.Animation.TargetState = SAS_STATE_SIGHT_AIM;
					}
					else if (Random::TestProbability(1 / 2.0f))
					{
						item.Animation.TargetState = SAS_STATE_HOLD_AIM;
					}
					else
					{
						item.Animation.TargetState = SAS_STATE_KNEEL_AIM;
					}
				}
				else if (item.AIBits == MODIFY)
				{
					item.Animation.TargetState = SAS_STATE_IDLE;
				}
				else if (creature.Mood == MoodType::Escape)
				{
					item.Animation.TargetState = SAS_STATE_RUN;
				}
				else if ((creature.Alerted || creature.Mood != MoodType::Bored) &&
					(!(item.AIBits & FOLLOW) || (!creature.ReachedGoal && distance2D <= SAS_WALK_RANGE)))
				{
					if (creature.Mood != MoodType::Bored &&
						AI.distance > SAS_WALK_RANGE)
					{
						item.Animation.TargetState = SAS_STATE_RUN;
					}
					else
					{
						item.Animation.TargetState = SAS_STATE_WALK;
					}
				}
				else
				{
					item.Animation.TargetState = SAS_STATE_IDLE;
				}

				break;

			case SAS_STATE_WAIT:
				creature.MaxTurn = 0;
				creature.Flags = 0;
				joint2 = angle;

				if (item.AIBits & GUARD)
				{
					joint2 = AIGuard(&creature);

					if (!(GetRandomControl() & 0xFF))
						item.Animation.TargetState = SAS_STATE_IDLE;
				}
				else if (Targetable(&item, &AI) ||
					creature.Mood == MoodType::Bored ||
					!AI.ahead ||
					item.AIBits & MODIFY ||
					Lara.Context.Vehicle != NO_VALUE)
				{
					item.Animation.TargetState = SAS_STATE_IDLE;
				}

				break;

			case SAS_STATE_WALK:
				creature.MaxTurn = ANGLE(5.0f);
				creature.Flags = 0;
				joint2 = angle;

				if (item.AIBits & PATROL1)
				{
					item.Animation.TargetState = SAS_STATE_WALK;
				}
				else if (Lara.Context.Vehicle != NO_VALUE &&
					(item.AIBits == MODIFY ||
						!item.AIBits))
				{
					item.Animation.TargetState = SAS_STATE_IDLE;
				}
				else if (creature.Mood == MoodType::Escape)
				{
					item.Animation.TargetState = SAS_STATE_RUN;
				}
				else if (item.AIBits & GUARD ||
					item.AIBits & FOLLOW &&
					(creature.ReachedGoal ||
						distance2D > SAS_WALK_RANGE))
				{
					item.Animation.TargetState = SAS_STATE_IDLE;
				}
				else if (Targetable(&item, &AI))
				{
					if (AI.distance >= SAS_SHOOT_RANGE &&
						AI.zoneNumber == AI.enemyZone)
					{
						item.Animation.TargetState = SAS_STATE_WALK_AIM;
					}
					else
					{
						item.Animation.TargetState = SAS_STATE_IDLE;
					}
				}
				else if (creature.Mood != MoodType::Bored)
				{
					if (AI.distance > SAS_WALK_RANGE)
						item.Animation.TargetState = SAS_STATE_RUN;
				}
				else if (AI.ahead)
				{
					item.Animation.TargetState = SAS_STATE_IDLE;
				}

				break;

			case SAS_STATE_RUN:
				creature.MaxTurn = ANGLE(10.0f);
				tilt = angle / 2;

				if (AI.ahead)
					joint2 = AI.angle;

				if (Lara.Context.Vehicle != NO_VALUE)
				{
					if (item.AIBits == MODIFY || !item.AIBits)
					{
						item.Animation.TargetState = SAS_STATE_WALK;
						break;
					}
				}

				if (item.AIBits & GUARD ||
					(item.AIBits & FOLLOW && (creature.ReachedGoal || distance2D > SAS_WALK_RANGE)))
				{
					item.Animation.TargetState = SAS_STATE_WALK;
				}
				else if (creature.Mood != MoodType::Escape)
				{
					if (Targetable(&item, &AI))
					{
						item.Animation.TargetState = SAS_STATE_WALK;
					}
					else if (creature.Mood == MoodType::Bored ||
						(creature.Mood == MoodType::Stalk &&
						!(item.AIBits & FOLLOW) &&
						AI.distance < SAS_WALK_RANGE))
					{
						item.Animation.TargetState = SAS_STATE_WALK;
					}
				}

				break;

			case SAS_STATE_SIGHT_AIM:
			case SAS_STATE_HOLD_AIM:
			case SAS_STATE_KNEEL_AIM:
				creature.Flags = 0;

				if (AI.ahead)
				{
					joint0 = AI.angle;
					joint1 = AI.xAngle;

					if (Targetable(&item, &AI))
					{
						if (item.Animation.ActiveState == SAS_STATE_SIGHT_AIM)
						{
							item.Animation.TargetState = SAS_STATE_SIGHT_SHOOT;
						}
						else if (item.Animation.ActiveState == SAS_STATE_KNEEL_AIM)
						{
							item.Animation.TargetState = SAS_STATE_KNEEL_SHOOT;
						}
						else if (Random::TestProbability(1 / 2.0f))
						{
							item.Animation.TargetState = SAS_STATE_HOLD_SHOOT;
						}
						else
						{
							item.Animation.TargetState = SAS_STATE_HOLD_PREPARE_GRENADE;
						}
					}
					else
					{
						item.Animation.TargetState = SAS_STATE_IDLE;
					}
				}

				break;

			case SAS_STATE_WALK_AIM:
				creature.Flags = 0;

				if (AI.ahead)
				{
					joint0 = AI.angle;
					joint1 = AI.xAngle;

					if (Targetable(&item, &AI))
						item.Animation.TargetState = SAS_STATE_WALK_SHOOT;
					else
						item.Animation.TargetState = SAS_STATE_WALK;
				}

				break;

			case SAS_STATE_HOLD_PREPARE_GRENADE:
				if (AI.ahead)
				{
					joint0 = AI.angle;
					joint1 = AI.xAngle;
				}

				break;

			case SAS_STATE_HOLD_SHOOT_GRENADE:
				if (AI.ahead)
				{
					angle1 = AI.angle;
					angle2 = AI.xAngle;
					joint0 = AI.angle;
					joint1 = AI.xAngle;

					if (AI.distance > SAS_SHOOT_RANGE)
					{
						angle2 = sqrt(AI.distance) + AI.xAngle - ANGLE(5.6f);
						joint1 = angle2;
					}
				}
				else
				{
					angle1 = 0;
					angle2 = 0;
				}

				if (item.Animation.FrameNumber == 20)
				{
					if (!creature.Enemy->Animation.Velocity.z)
					{
						angle1 += (GetRandomControl() & 0x1FF) - 256;
						angle2 += (GetRandomControl() & 0x1FF) - 256;
						joint0 = angle1;
						joint1 = angle2;
					}

					SasFireGrenade(item, angle2, angle1);

					if (Targetable(&item, &AI))
						item.Animation.TargetState = SAS_STATE_HOLD_PREPARE_GRENADE;
				}

				break;

			case SAS_STATE_HOLD_SHOOT:
			case SAS_STATE_KNEEL_SHOOT:
			case SAS_STATE_SIGHT_SHOOT:
			case SAS_STATE_WALK_SHOOT:
				if (item.Animation.ActiveState == SAS_STATE_HOLD_SHOOT ||
					item.Animation.ActiveState == SAS_STATE_KNEEL_SHOOT)
				{
					if (item.Animation.TargetState != SAS_STATE_IDLE &&
						item.Animation.TargetState != SAS_STATE_KNEEL_STOP &&
						(creature.Mood == MoodType::Escape ||
							!Targetable(&item, &AI)))
					{
						if (item.Animation.ActiveState == SAS_STATE_HOLD_SHOOT)
							item.Animation.TargetState = SAS_STATE_IDLE;
						else
							item.Animation.TargetState = SAS_STATE_KNEEL_STOP;
					}
				}

				if (AI.ahead)
				{
					joint0 = AI.angle;
					joint1 = AI.xAngle;
				}

				if (creature.Flags != 0)
				{
					creature.Flags -= 1;
				}
				else
				{
					ShotLara(&item, &AI, SasGunBite, joint0, SAS_SHOT_DAMAGE);
					creature.MuzzleFlash[0].Bite = SasGunBite;
					creature.MuzzleFlash[0].Delay = 2;
					creature.Flags = 5;
				}

				break;

			case SAS_STATE_BLIND:
				if (!FlashGrenadeAftershockTimer && !(GetRandomControl() & 0x7F)) // TODO: This is a probabliity of roughly 0.998f.
					item.Animation.TargetState = SAS_STATE_WAIT;

				break;
			}

			if (FlashGrenadeAftershockTimer > 100 &&
				item.Animation.ActiveState != SAS_STATE_BLIND)
			{
				SetAnimation(item, SAS_ANIM_BLIND, Random::GenerateInt(0, 8));
				creature.MaxTurn = 0;
			}
		}
		else
		{
			if (item.Animation.ActiveState != SAS_STATE_DEATH)
			{
				SetAnimation(item, SAS_ANIM_DEATH);
			}
		}

		CreatureTilt(&item, tilt);
		CreatureJoint(&item, 0, joint0);
		CreatureJoint(&item, 1, joint1);
		CreatureJoint(&item, 2, joint2);

		CreatureAnimation(itemNumber, angle, 0);
	}

	void InjuredSasControl(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (item.Animation.ActiveState == 1)
		{
			if (Random::TestProbability(1 / 128.0f))
			{
				item.Animation.TargetState = 2;
				AnimateItem(item);
			}
			else if (!(byte)GetRandomControl())
			{
				item.Animation.TargetState = 3;
			}
		}
		else if (item.Animation.ActiveState == 4 &&
			Random::TestProbability(1 / 128.0f))
		{
			item.Animation.TargetState = 5;
			AnimateItem(item);
		}
		else
		{
			AnimateItem(item);
		}
	}

	void SasDragBlokeCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto& item = g_Level.Items[itemNumber];
		auto& player = *GetLaraInfo(laraItem);

		if ((IsHeld(In::Action) &&
			laraItem->Animation.ActiveState == LS_IDLE &&
			laraItem->Animation.AnimNumber == LA_STAND_IDLE &&
			player.Control.HandStatus == HandStatus::Free &&
			!laraItem->Animation.IsAirborne &&
			!(item.Flags & IFLAG_ACTIVATION_MASK)) ||
			player.Control.IsMoving && player.Context.InteractedItem == itemNumber)
		{
			if (TestLaraPosition(SasDragBounds, &item, laraItem))
			{
				if (MoveLaraPosition(SasDragBodyPosition, &item, laraItem))
				{
					SetAnimation(*laraItem, LA_DRAG_BODY);
					ResetPlayerFlex(laraItem);
					laraItem->Pose.Orientation.y = item.Pose.Orientation.y;
					player.Control.HandStatus = HandStatus::Busy;
					player.Control.IsMoving = false;

					AddActiveItem(itemNumber);
					item.Flags |= IFLAG_ACTIVATION_MASK;
					item.Status = ITEM_ACTIVE;
				}
				else
				{
					player.Context.InteractedItem = itemNumber;
				}
			}
		}
		else
		{
			if (item.Status != ITEM_ACTIVE)
			{
				ObjectCollision(itemNumber, laraItem, coll);
				return;
			}

			if (!TestLastFrame(*&item))
				return;

			auto pos = GetJointPosition(&item, 0);
			TestTriggers(pos.x, pos.y, pos.z, item.RoomNumber, true);
			RemoveActiveItem(itemNumber);
			item.Status = ITEM_DEACTIVATED;
		}
	}

	void SasFireGrenade(ItemInfo& item, short angle1, short angle2)
	{
		short itemNumber = CreateItem();
		if (itemNumber == NO_VALUE)
			return;

		auto grenadeItem = &g_Level.Items[itemNumber];

		grenadeItem->Model.Color = Vector4(0.5f, 0.5f, 0.5f, 1.0f);
		grenadeItem->ObjectNumber = ID_GRENADE;
		grenadeItem->RoomNumber = item.RoomNumber;

		auto pos = GetJointPosition(&item, SasGunBite);
		grenadeItem->Pose.Position = pos;

		auto floorHeight = GetPointCollision(pos, grenadeItem->RoomNumber).GetFloorHeight();
		if (floorHeight < pos.y)
		{
			grenadeItem->Pose.Position = Vector3i(item.Pose.Position.x, pos.y, item.Pose.Position.z);
			grenadeItem->RoomNumber = item.RoomNumber;
		}

		for (int i = 0; i < 5; i++)
			TriggerGunSmoke(pos.x, pos.y, pos.z, 0, 0, 0, 1, LaraWeaponType::GrenadeLauncher, 32);

		InitializeItem(itemNumber);

		grenadeItem->Pose.Orientation = EulerAngles(
			angle1 + item.Pose.Orientation.x,
			angle2 + item.Pose.Orientation.y,
			0);
		grenadeItem->Animation.Velocity.y = -128 * phd_sin(grenadeItem->Pose.Orientation.x);
		grenadeItem->Animation.Velocity.z = 128;
		grenadeItem->Animation.ActiveState = grenadeItem->Pose.Orientation.x;
		grenadeItem->Animation.TargetState = grenadeItem->Pose.Orientation.y;
		grenadeItem->Animation.RequiredState = NO_VALUE;

		if (Random::TestProbability(3 / 4.0f))
			grenadeItem->ItemFlags[0] = (int)ProjectileType::Grenade;
		else
			grenadeItem->ItemFlags[0] = (int)ProjectileType::FragGrenade;

		grenadeItem->HitPoints = 120;
		grenadeItem->ItemFlags[2] = 1;

		AddActiveItem(itemNumber);
	}
}
