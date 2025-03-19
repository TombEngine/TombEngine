#include "framework.h"
#include "Objects/TR5/Entity/tr5_dog.h"

#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Specific/level.h"

namespace TEN::Entities::Creatures::TR5
{
	const auto DogBite = CreatureBiteInfo(Vector3(0, 0, 100), 3);
	static BYTE DogAnims[] = { 20, 21, 22, 20 };

	void InitializeTr5Dog(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		item->Animation.AnimNumber = 8;
		item->Animation.ActiveState = 1;

		if (!item->TriggerFlags)
		{
			item->Animation.AnimNumber = 1;
			// TODO: item->flags2 ^= (item->flags2 ^ ((item->flags2 & 0xFE) + 2)) & 6;
		}

		item->Animation.FrameNumber = 0;
	}

	void Tr5DogControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		short angle = 0;
		short joint2 = 0;
		short joint1 = 0;
		short joint0 = 0;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);
		auto* object = &Objects[item->ObjectNumber];

		if (item->HitPoints <= 0)
		{
			if (item->Animation.AnimNumber == 1)
				item->HitPoints = object->HitPoints;
			else if (item->Animation.ActiveState != 11)
			{
				item->Animation.AnimNumber = DogAnims[GetRandomControl() & 3];
				item->Animation.FrameNumber = 0;
				item->Animation.ActiveState = 11;
			}
		}
		else
		{
			if (item->AIBits)
				GetAITarget(creature);
			else
				creature->Enemy = LaraItem;

			AI_INFO AI;
			CreatureAIInfo(item, &AI);

			int distance;
			if (creature->Enemy == LaraItem)
				distance = AI.distance;
			else
			{
				int dx = LaraItem->Pose.Position.x - item->Pose.Position.x;
				int dz = LaraItem->Pose.Position.z - item->Pose.Position.z;
				phd_atan(dz, dx);
				distance = pow(dx, 2) + pow(dz, 2);
			}

			if (AI.ahead)
			{
				joint2 = AI.xAngle; // Maybe swapped
				joint1 = AI.angle;
			}

			GetCreatureMood(item, &AI, true);
			CreatureMood(item, &AI, true);

			if (creature->Mood == MoodType::Bored)
				creature->MaxTurn /= 2;

			angle = CreatureTurn(item, creature->MaxTurn);
			joint0 = 4 * angle;

			if (creature->HurtByLara || distance < pow(BLOCK(3), 2) && !(item->AIBits & MODIFY))
			{
				AlertAllGuards(itemNumber);
				item->AIBits &= ~MODIFY;
			}

			short random = GetRandomControl();
			int frame = item->Animation.FrameNumber;

			switch (item->Animation.ActiveState)
			{
			case 0:
			case 8:
				joint1 = 0;
				joint2 = 0;

				if (creature->Mood != MoodType::Bored && item->AIBits != MODIFY)
					item->Animation.TargetState = 1;
				else
				{
					creature->MaxTurn = 0;
					creature->Flags++;

					if (creature->Flags > 300 && random < 128)
						item->Animation.TargetState = 1;
				}

				break;

			case 1:
			case 9:
				if (item->Animation.ActiveState == 9 && item->Animation.RequiredState)
				{
					item->Animation.TargetState = item->Animation.RequiredState;
					break;
				}

				creature->MaxTurn = 0;

				if (item->AIBits & GUARD)
				{
					joint1 = AIGuard(creature);

					if (GetRandomControl())
						break;

					if (item->Animation.ActiveState == 1)
					{
						item->Animation.TargetState = 9;
						break;
					}
				}
				else
				{
					if (item->Animation.ActiveState == 9 && random < 128)
					{
						item->Animation.TargetState = 1;
						break;
					}

					if (item->AIBits & PATROL1)
					{
						if (item->Animation.ActiveState == 1)
							item->Animation.TargetState = 2;
						else
							item->Animation.TargetState = 1;

						break;
					}

					if (creature->Mood == MoodType::Escape)
					{
						if (Lara.TargetEntity == item || !AI.ahead || item->HitStatus)
						{
							item->Animation.RequiredState = 3;
							item->Animation.TargetState = 9;
						}
						else
							item->Animation.TargetState = 1;

						break;
					}

					if (creature->Mood != MoodType::Bored)
					{
						item->Animation.RequiredState = 3;

						if (item->Animation.ActiveState == 1)
							item->Animation.TargetState = 9;

						break;
					}

					creature->Flags = 0;
					creature->MaxTurn = ANGLE(1.0f);

					if (random < 256)
					{
						if (item->AIBits & MODIFY)
						{
							if (item->Animation.ActiveState == 1)
							{
								item->Animation.TargetState = 8;
								creature->Flags = 0;
								break;
							}
						}
					}

					if (random >= 4096)
					{
						if (!(random & 0x1F))
							item->Animation.TargetState = 7;

						break;
					}

					if (item->Animation.ActiveState == 1)
					{
						item->Animation.TargetState = 2;
						break;
					}
				}

				item->Animation.TargetState = 1;
				break;

			case 2:
				creature->MaxTurn = ANGLE(3.0f);

				if (item->AIBits & PATROL1)
				{
					item->Animation.TargetState = 2;
					break;
				}

				if (creature->Mood == MoodType::Bored && random < 256)
				{
					item->Animation.TargetState = 1;
					break;
				}

				item->Animation.TargetState = 5;
				break;

			case 3:
				creature->MaxTurn = ANGLE(6.0f);

				if (creature->Mood == MoodType::Escape)
				{
					if (Lara.TargetEntity != item && AI.ahead)
						item->Animation.TargetState = 9;
				}
				else if (creature->Mood != MoodType::Bored)
				{
					if (AI.bite && AI.distance < pow(BLOCK(1), 2))
						item->Animation.TargetState = 6;
					else if (AI.distance < pow(BLOCK(1.5f), 2))
					{
						item->Animation.RequiredState = 5;
						item->Animation.TargetState = 9;
					}
				}
				else
					item->Animation.TargetState = 9;

				break;

			case 5:
				creature->MaxTurn = ANGLE(3.0f);

				if (creature->Mood != MoodType::Bored)
				{
					if (creature->Mood == MoodType::Escape)
						item->Animation.TargetState = 3;
					else if (AI.bite && AI.distance < pow(341, 2))
					{
						item->Animation.TargetState = 12;
						item->Animation.RequiredState = 5;
					}
					else if (AI.distance > pow(BLOCK(1.5f), 2) || item->HitStatus)
						item->Animation.TargetState = 3;
				}
				else
					item->Animation.TargetState = 9;

				break;

			case 6:
				if (AI.bite &&
					item->TouchBits & 0x6648 &&
					frame >= 4 &&
					frame <= 14)
				{
					DoDamage(creature->Enemy, 20);
					CreatureEffect2(item, DogBite, 2, -1, DoBloodSplat);
				}

				item->Animation.TargetState = 3;
				break;

			case 7:
				joint1 = 0;
				joint2 = 0;
				break;

			case 12:
				if (AI.bite && item->TouchBits & 0x48 &&
					(frame >= 9 && frame <= 12 ||
						frame >= 22 && frame <= 25))
				{
					DoDamage(creature->Enemy, 10);
					CreatureEffect2(item, DogBite, 2, -1, DoBloodSplat);
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
		CreatureAnimation(itemNumber, angle, 0);
	}
}
