#include "framework.h"
#include "Objects/TR2/Entity/tr2_shark.h"

#include "Game/Animation/Animation.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Specific/level.h"

using namespace TEN::Animation;

namespace TEN::Entities::Creatures::TR2
{
	constexpr auto SHARK_BITE_ATTACK_DAMAGE = 400;

	const auto SharkBite = CreatureBiteInfo(Vector3(17, -22, 344), 12);
	const auto SharkBiteAttackJoints = std::vector<unsigned int>{ 10, 12, 13 };

	void SharkControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short angle = 0;
		short head = 0;

		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState != 5)
				SetAnimation(*item, 4);

			CreatureFloat(itemNumber);
			return;
		}
		else
		{
			AI_INFO AI;
			CreatureAIInfo(item, &AI);
			GetCreatureMood(item, &AI, true);
			CreatureMood(item, &AI, true);

			angle = CreatureTurn(item, creature->MaxTurn);

			switch (item->Animation.ActiveState)
			{
			case 0:
				creature->MaxTurn = 0;
				creature->Flags = 0;

				if (AI.ahead && AI.distance < pow(BLOCK(0.75f), 2) && AI.zoneNumber == AI.enemyZone)
					item->Animation.TargetState = 3;
				else
					item->Animation.TargetState = 1;

				break;

			case 1:
				creature->MaxTurn = ANGLE(0.5f);

				if (creature->Mood == MoodType::Bored)
					break;
				else if (AI.ahead && AI.distance < pow(BLOCK(0.75f), 2))
					item->Animation.TargetState = 0;
				else if (creature->Mood == MoodType::Escape || AI.distance > pow(BLOCK(3), 2) || !AI.ahead)
					item->Animation.TargetState = 2;

				break;

			case 2:
				creature->MaxTurn = ANGLE(2.0f);
				creature->Flags = 0;

				if (creature->Mood == MoodType::Bored)
					item->Animation.TargetState = 1;
				else if (creature->Mood == MoodType::Escape)
					break;
				else if (AI.ahead && AI.distance < pow(1365, 2) && AI.zoneNumber == AI.enemyZone)
				{
					if (GetRandomControl() < 0x800)
						item->Animation.TargetState = 0;
					else if (AI.distance < pow(BLOCK(0.75f), 2))
						item->Animation.TargetState = 4;
				}

				break;

			case 3:
			case 4:
				if (AI.ahead)
					head = AI.angle;

				if (!creature->Flags && item->TouchBits.Test(SharkBiteAttackJoints))
				{
					DoDamage(creature->Enemy, SHARK_BITE_ATTACK_DAMAGE);
					CreatureEffect(item, SharkBite, DoBloodSplat);
					creature->Flags = 1;
				}

				break;
			}
		}

		if (item->Animation.ActiveState != 6)
		{
			CreatureJoint(item, 0, head);
			CreatureAnimation(itemNumber, angle, 0);
			CreatureUnderwater(item, 340);
		}
		else
			AnimateItem(*item);
	}
}
