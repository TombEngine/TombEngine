#include "framework.h"
#include "Objects/TR5/Entity/tr5_chef.h"

#include "Game/items.h"
#include "Game/control/box.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/itemdata/creature_info.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Sound/sound.h"
#include "Specific/level.h"

namespace TEN::Entities::Creatures::TR5
{
	const auto ChefBite = CreatureBiteInfo(Vector3(0, 200, 0), 13);

	// TODO
	enum ChefState
	{
		// No state 0.
		CHEF_STATE_COOKING = 1,
		CHEF_STATE_TURN_180 = 2,
		CHEF_STATE_ATTACK = 3,
		CHEF_STATE_AIM = 4,
		CHEF_STATE_WALK = 5,

		CHEF_STATE_DEATH = 8
	};

	// TODO
	enum ChefAnim
	{
		CHEF_ANIM_IDLE = 0,
		CHEF_ANIM_DEATH = 16
	};

	void InitializeChef(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(*item, CHEF_ANIM_IDLE);
		item->Pose.Position.x += 192 * phd_sin(item->Pose.Orientation.y);
		item->Pose.Position.z += 192 * phd_cos(item->Pose.Orientation.y);
	}

	void ControlChef(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short headingAngle = 0;
		short joint0 = 0;
		short joint1 = 0;
		short joint2 = 0;

		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState != CHEF_STATE_DEATH)
			{
				SetAnimation(*item, CHEF_ANIM_DEATH);
				item->HitPoints = 0;
			}
		}
		else
		{
			if (item->AIBits)
			{
				GetAITarget(creature);
			}
			else if (creature->HurtByLara)
			{
				creature->Enemy = LaraItem;
			}

			AI_INFO ai;
			AI_INFO aiPlayerInfo;
			CreatureAIInfo(item, &ai);

			if (creature->Enemy == LaraItem)
			{
				aiPlayerInfo.angle = ai.angle;
				aiPlayerInfo.distance = ai.distance;
			}
			else
			{
				int dx = LaraItem->Pose.Position.x - item->Pose.Position.x;
				int dz = LaraItem->Pose.Position.z - item->Pose.Position.z;

				aiPlayerInfo.angle = phd_atan(dz, dx) - item->Pose.Orientation.y;
				aiPlayerInfo.ahead = true;

				if (aiPlayerInfo.angle <= -ANGLE(90.0f) || aiPlayerInfo.angle >= ANGLE(90.0f))
					aiPlayerInfo.ahead = false;

				aiPlayerInfo.distance = pow(dx, 2) + pow(dz, 2);
			}

			GetCreatureMood(item, &ai, true);
			CreatureMood(item, &ai, true);

			short angle = CreatureTurn(item, creature->MaxTurn);

			if (ai.ahead)
			{
				joint0 = ai.angle / 2;
				joint2 = ai.angle / 2;
			}

			creature->MaxTurn = 0;

			switch (item->Animation.ActiveState)
			{
			case CHEF_STATE_COOKING:
				if (abs(LaraItem->Pose.Position.y - item->Pose.Position.y) < BLOCK(1) &&
					ai.distance < pow(BLOCK(1.5f), 2) &&
					(item->TouchBits.TestAny() ||
						item->HitStatus ||
						LaraItem->Animation.Velocity.z > 15 ||
						TargetVisible(item, &aiPlayerInfo)))
				{
					item->Animation.TargetState = CHEF_STATE_TURN_180;
					creature->Alerted = true;
					item->AIBits = 0;
				}

				break;

			case CHEF_STATE_TURN_180:
				creature->MaxTurn = 0;

				if (ai.angle > 0)
				{
					item->Pose.Orientation.y -= ANGLE(2.0f);
				}
				else
				{
					item->Pose.Orientation.y += ANGLE(2.0f);
				}
				if (TestLastFrame(*item))
				{
					item->Pose.Orientation.y += -ANGLE(180.0f);
				}

				break;

			case CHEF_STATE_ATTACK:
				creature->MaxTurn = 0;

				if (abs(ai.angle) >= ANGLE(2.0f))
				{
					if (ai.angle > 0)
					{
						item->Pose.Orientation.y += ANGLE(2.0f);
					}
					else
					{
						item->Pose.Orientation.y -= ANGLE(2.0f);
					}
				}
				else
				{
					item->Pose.Orientation.y += ai.angle;
				}

				if (!creature->Flags)
				{
					if (item->TouchBits & 0x2000)
					{
						if (item->Animation.FrameNumber > 10)
						{
							DoDamage(creature->Enemy, 80);
							CreatureEffect2(item, ChefBite, 20, item->Pose.Orientation.y, DoBloodSplat);
							SoundEffect(SFX_TR4_LARA_THUD, &item->Pose);
							creature->Flags = 1;
						}
					}
				}

				break;

			case CHEF_STATE_AIM:
				creature->MaxTurn = ANGLE(2.0f);
				creature->Flags = 0;

				if (ai.distance >= pow(682, 2))
				{
					if (ai.angle > ANGLE(112.5f) || ai.angle < -ANGLE(112.5f))
					{
						item->Animation.TargetState = CHEF_STATE_TURN_180;
					}
					else if (creature->Mood == MoodType::Attack)
					{
						item->Animation.TargetState = CHEF_STATE_WALK;
					}
				}
				else if (ai.bite)
				{
					item->Animation.TargetState = CHEF_STATE_ATTACK;
				}

				break;

			case CHEF_STATE_WALK:
				creature->MaxTurn = ANGLE(7.0f);

				if (ai.distance < pow(682, 2) ||
					ai.angle > ANGLE(112.5f) ||
					ai.angle < -ANGLE(112.5f) ||
					creature->Mood != MoodType::Attack)
				{
					item->Animation.TargetState = CHEF_STATE_AIM;
				}

				break;

			default:
				break;
			}
		}

		CreatureTilt(item, 0);
		CreatureJoint(item, 0, joint0);
		CreatureJoint(item, 1, joint1);
		CreatureJoint(item, 2, joint2);
		CreatureAnimation(itemNumber, headingAngle, 0);
	}
}
