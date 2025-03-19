#include "framework.h"
#include "Objects/TR2/Entity/tr2_monk.h"

#include "Game/control/box.h"
#include "Game/effects/effects.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Sound/sound.h"
#include "Specific/level.h"

namespace TEN::Entities::Creatures::TR2
{
	const auto MonkBite = CreatureBiteInfo(Vector3(-23, 16, 265), 14);

	bool MonksAttackLara;

	// TODO
	enum MonkState
	{

	};

	// TODO
	enum MonkAnim
	{

	};

	void MonkControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short angle = 0;
		short tilt = 0;
		short torso = 0;

		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState != 9)
			{
				item->Animation.AnimNumber = 20 + (GetRandomControl() / 0x4000);
				item->Animation.FrameNumber = 0;
				item->Animation.ActiveState = 9;
			}
		}
		else
		{
			if (MonksAttackLara)
				creature->Enemy = LaraItem;

			AI_INFO AI;
			CreatureAIInfo(item, &AI);

			if (!MonksAttackLara && creature->Enemy == LaraItem)
				creature->Enemy = nullptr;

			GetCreatureMood(item, &AI, true);
			CreatureMood(item, &AI, true);
			angle = CreatureTurn(item, creature->MaxTurn);

			if (AI.ahead)
				torso = AI.angle;

			switch (item->Animation.ActiveState)
			{
			case 1:
				creature->Flags &= 0x0FFF;

				if (!MonksAttackLara && AI.ahead && Lara.TargetEntity == item)
					break;
				else if (creature->Mood == MoodType::Bored)
					item->Animation.TargetState = 2;
				else if (creature->Mood == MoodType::Escape)
					item->Animation.TargetState = 3;
				else if (AI.ahead && AI.distance < pow(BLOCK(0.5f), 2))
				{
					if (GetRandomControl() < 0x7000)
						item->Animation.TargetState = 4;
					else
						item->Animation.TargetState = 11;
				}
				else if (AI.ahead && AI.distance < pow(BLOCK(1), 2))
					item->Animation.TargetState = 7;
				else if (AI.ahead && AI.distance < pow(BLOCK(2), 2))
					item->Animation.TargetState = 2;
				else
					item->Animation.TargetState = 3;

				break;

			case 11:
				creature->Flags &= 0x0FFF;

				if (!MonksAttackLara && AI.ahead && Lara.TargetEntity == item)
					break;
				else if (creature->Mood == MoodType::Bored)
					item->Animation.TargetState = 2;
				else if (creature->Mood == MoodType::Escape)
					item->Animation.TargetState = 3;
				else if (AI.ahead && AI.distance < pow(BLOCK(0.5f), 2))
				{
					auto random = GetRandomControl();
					if (random < 0x3000)
						item->Animation.TargetState = 5;
					else if (random < 0x6000)
						item->Animation.TargetState = 8;
					else
						item->Animation.TargetState = 1;
				}
				else if (AI.ahead && AI.distance < pow(BLOCK(2), 2))
					item->Animation.TargetState = 2;
				else
					item->Animation.TargetState = 3;

				break;

			case 2:
				creature->MaxTurn = ANGLE(3.0f);

				if (creature->Mood == MoodType::Bored)
				{
					if (!MonksAttackLara && AI.ahead && Lara.TargetEntity == item)
					{
						if (GetRandomControl() < 0x4000)
							item->Animation.TargetState = 1;
						else
							item->Animation.TargetState = 11;
					}
				}
				else if (creature->Mood == MoodType::Escape)
					item->Animation.TargetState = 3;
				else if (AI.ahead && AI.distance < pow(BLOCK(0.5f), 2))
				{
					if (GetRandomControl() < 0x4000)
						item->Animation.TargetState = 1;
					else
						item->Animation.TargetState = 11;
				}
				else if (!AI.ahead || AI.distance > pow(BLOCK(2), 2))
					item->Animation.TargetState = 3;

				break;

			case 3:
				tilt = angle / 4;
				creature->MaxTurn = ANGLE(4.0f);
				creature->Flags &= 0x0FFF;

				if (MonksAttackLara)
					creature->MaxTurn += ANGLE(1.0f);

				if (creature->Mood == MoodType::Bored)
					item->Animation.TargetState = 1;
				else if (creature->Mood == MoodType::Escape)
					break;
				else if (AI.ahead && AI.distance < pow(BLOCK(0.5f), 2))
				{
					if (GetRandomControl() < 0x4000)
						item->Animation.TargetState = 1;
					else
						item->Animation.TargetState = 11;
				}
				else if (AI.ahead && AI.distance < pow(BLOCK(3), 2))
					item->Animation.TargetState = 10;
				else if (AI.ahead && AI.distance < pow(BLOCK(2), 2))
				{
					if (GetRandomControl() < 0x4000)
						item->Animation.TargetState = 1;
					else
						item->Animation.TargetState = 11;
				}

				break;

			case 8:
				if (!AI.ahead || AI.distance > pow(BLOCK(0.5f), 2))
					item->Animation.TargetState = 11;
				else
					item->Animation.TargetState = 6;

				break;

			case 4:
			case 5:
			case 6:
			case 7:
			case 10:
				auto * enemy = creature->Enemy;
				if (enemy == LaraItem)
				{
					if (!(creature->Flags & 0xF000) && item->TouchBits & 0x4000)
					{
						creature->Flags |= 0x1000;
						DoDamage(enemy, 150);
						CreatureEffect(item, MonkBite, DoBloodSplat);
						SoundEffect(SFX_TR2_CRUNCH1, &item->Pose);
					}
				}
				else
				{
					if (!(creature->Flags & 0xf000) && enemy != nullptr)
					{
						float distance = Vector3i::Distance(item->Pose.Position, enemy->Pose.Position);
						if (distance <= CLICK(2))
						{
							creature->Flags |= 0x1000;
							DoDamage(enemy, 5);
							SoundEffect(SFX_TR2_CRUNCH1, &item->Pose);
						}
					}
				}

				break;
			}
		}

		CreatureTilt(item, tilt);
		CreatureJoint(item, 0, torso);
		CreatureAnimation(itemNumber, angle, tilt);
	}
}
