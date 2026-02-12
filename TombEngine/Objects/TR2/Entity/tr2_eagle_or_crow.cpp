#include "framework.h"
#include "Objects/TR2/Entity/tr2_eagle_or_crow.h"

#include "Game/collision/collide_room.h"
#include "Game/control/box.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/itemdata/creature_info.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Specific/level.h"

namespace TEN::Entities::Creatures::TR2
{
	const auto EagleBite = CreatureBiteInfo(Vector3(15, 46, 21), 6);
	const auto CrowBite	 = CreatureBiteInfo(Vector3(2, 10, 60), 14);

	enum EagleOrCrowState
	{
		// No state 0.
		EAGLE_CROW_STATE_FLY = 1,
		EAGLE_CROW_STATE_IDLE = 2,
		EAGLE_CROW_STATE_PLANE = 3,
		EAGLE_CROW_STATE_DEATH_START = 4,
		EAGLE_CROW_STATE_DEATH_END = 5,
		EAGLE_CROW_STATE_ATTACK = 6
	};

	enum EagleOrCrowAnim
	{

	};

	void InitializeEagle(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);

		if (item->ObjectNumber == ID_CROW)
		{
			item->Animation.AnimNumber = 14;
			item->Animation.FrameNumber = 0;
			item->Animation.ActiveState = item->Animation.TargetState = 7;
		}
		else
		{
			item->Animation.AnimNumber = 5;
			item->Animation.FrameNumber = 0;
			item->Animation.ActiveState = item->Animation.TargetState = 2;
		}
	}

	void EagleControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short angle = 0;

		if (item->HitPoints <= 0)
		{
			switch (item->Animation.ActiveState)
			{
			case 4:
				if (item->Pose.Position.y >= item->Floor)
				{
					item->Animation.Velocity.y = 0.0f;
					item->Animation.IsAirborne = false;
					item->Animation.TargetState = 5;
					item->Pose.Position.y = item->Floor;
					AlignEntityToSurface(item, Vector2(Objects[item->ObjectNumber].radius));
				}

				break;

			case 5:
				item->Pose.Position.y = item->Floor;
				break;

			default:
				if (item->ObjectNumber == ID_CROW)
					item->Animation.AnimNumber = 1;
				else
					item->Animation.AnimNumber = 8;

				item->Animation.FrameNumber = 0;
				item->Animation.ActiveState = 4;
				item->Animation.Velocity.z = 0;
				item->Animation.IsAirborne = true;
				break;
			}

			item->Pose.Orientation.x = 0;
		}
		else
		{
			AI_INFO AI;
			CreatureAIInfo(item, &AI);

			GetCreatureMood(item, &AI, true);
			CreatureMood(item, &AI, false);

			angle = CreatureTurn(item, ANGLE(3.0f));

			switch (item->Animation.ActiveState)
			{
			case 7:
				item->Pose.Position.y = item->Floor;

				if (creature->Mood != MoodType::Bored)
					item->Animation.TargetState = 1;

				break;

			case 2:
				item->Pose.Position.y = item->Floor;

				if (creature->Mood == MoodType::Bored)
					break;
				else
					item->Animation.TargetState = 1;

				break;

			case 1:
				creature->Flags = 0;

				if (item->Animation.RequiredState != NO_VALUE)
					item->Animation.TargetState = item->Animation.RequiredState;
				if (creature->Mood == MoodType::Bored)
					item->Animation.TargetState = 2;
				else if (AI.ahead && AI.distance < pow(BLOCK(0.5f), 2))
					item->Animation.TargetState = 6;
				else
					item->Animation.TargetState = 3;

				break;

			case 3:
				if (creature->Mood == MoodType::Bored)
				{
					item->Animation.RequiredState = 2;
					item->Animation.TargetState = 1;
				}
				else if (AI.ahead && AI.distance < pow(BLOCK(0.5f), 2))
					item->Animation.TargetState = 6;

				break;

			case 6:
				if (!creature->Flags && item->TouchBits.TestAny())
				{
					DoDamage(creature->Enemy, 20);

					if (item->ObjectNumber == ID_CROW)
						CreatureEffect(item, CrowBite, DoBloodSplat);
					else
						CreatureEffect(item, EagleBite, DoBloodSplat);

					creature->Flags = 1;
				}

				break;
			}
		}

		CreatureAnimation(itemNumber, angle, 0);
	}
}
