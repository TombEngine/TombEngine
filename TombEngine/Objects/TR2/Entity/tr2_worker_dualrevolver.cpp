#include "framework.h"
#include "Objects/TR2/Entity/tr2_worker_dualrevolver.h"

#include "Game/control/box.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/people.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Specific/level.h"

namespace TEN::Entities::Creatures::TR2
{
	const auto WorkerDualGunBiteLeft  = CreatureBiteInfo(Vector3(-2, 340, 23), 6);
	const auto WorkerDualGunBiteRight = CreatureBiteInfo(Vector3(2, 340, 23), 10);

	// TODO
	enum WorkerDualGunState
	{

	};

	// TODO
	enum WorkerDualGunAnim
	{

	};

	void WorkerDualGunControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short headingAngle = 0;
		short tiltAngle = 0;
		auto extraHeadRot = EulerAngles::Identity;
		auto extraTorsoRot = EulerAngles::Identity;

		if (creature->MuzzleFlash[0].Delay != 0)
			creature->MuzzleFlash[0].Delay--;

		if (creature->MuzzleFlash[1].Delay != 0)
			creature->MuzzleFlash[1].Delay--;

		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState != 11)
				SetAnimation(*item, 32);
		}
		else if (LaraItem->HitPoints <= 0)
		{
			item->Animation.TargetState = 2;
		}
		else
		{
			AI_INFO ai;
			CreatureAIInfo(item, &ai);

			GetCreatureMood(item, &ai, true);
			CreatureMood(item, &ai, true);

			headingAngle = CreatureTurn(item, creature->MaxTurn);

			switch (item->Animation.ActiveState)
			{
			case 1:
			case 2:
				creature->MaxTurn = 0;

				if (ai.ahead)
				{
					extraHeadRot.x = ai.xAngle;
					extraHeadRot.y = ai.angle;
				}

				if (creature->Mood == MoodType::Attack || LaraItem->HitPoints > 0)
				{
					if (Targetable(item, &ai))
					{
						if (ai.distance <= pow(BLOCK(3), 2))
						{
							item->Animation.TargetState = 9;
						}
						else
						{
							item->Animation.TargetState = 3;
						}
					}
					else
					{
						switch (creature->Mood)
						{
						case MoodType::Attack:
							if (ai.distance > pow(BLOCK(20), 2) || !ai.ahead)
							{
								item->Animation.TargetState = 4;
							}
							else
							{
								item->Animation.TargetState = 3;
							}

							break;

						case MoodType::Escape:
							item->Animation.TargetState = 4;
							break;

						case MoodType::Stalk:
							item->Animation.TargetState = 3;
							break;

						default:
							if (!ai.ahead)
								item->Animation.TargetState = 3;

							break;
						}
					}
				}
				else
				{
					item->Animation.TargetState = 1;
				}

				break;

			case 3:
				creature->MaxTurn = ANGLE(3.0f);

				if (ai.ahead)
				{
					extraHeadRot.x = ai.xAngle;
					extraHeadRot.y = ai.angle;
				}

				if (Targetable(item, &ai))
				{
					if (ai.distance < pow(BLOCK(3), 2) || ai.zoneNumber != ai.enemyZone)
					{
						item->Animation.TargetState = 1;
					}
					else
					{
						if (ai.angle >= 0)
						{
							item->Animation.TargetState = 6;
						}
						else
						{
							item->Animation.TargetState = 5;
						}
					}
				}

				if (creature->Mood == MoodType::Escape)
				{
					item->Animation.TargetState = 4;
				}
				else if (creature->Mood == MoodType::Attack || creature->Mood == MoodType::Stalk)
				{
					if (ai.distance > pow(BLOCK(20), 2) || !ai.ahead)
						item->Animation.TargetState = 4;
				}
				else if (LaraItem->HitPoints > 0)
				{
					if (ai.ahead)
						item->Animation.TargetState = 1;
				}
				else
				{
					item->Animation.TargetState = 2;
				}

				break;

			case 4:
				creature->MaxTurn = ANGLE(6.0f);
				tiltAngle = headingAngle / 4;

				if (ai.ahead)
				{
					extraHeadRot.x = ai.xAngle;
					extraHeadRot.y = ai.angle;
				}

				if (Targetable(item, &ai))
				{
					if (ai.zoneNumber == ai.enemyZone)
					{
						if (ai.angle >= 0)
						{
							item->Animation.TargetState = 6;
						}
						else
						{
							item->Animation.TargetState = 5;
						}
					}
					else
					{
						item->Animation.TargetState = 3;
					}
				}
				else if (creature->Mood == MoodType::Attack)
				{
					if (ai.ahead && ai.distance < pow(BLOCK(20), 2))
						item->Animation.TargetState = 3;
				}
				else if (LaraItem->HitPoints > 0)
				{
					item->Animation.TargetState = 1;
				}
				else
				{
					item->Animation.TargetState = 2;
				}

				break;

			case 5:
				creature->Flags = 0;

				if (ai.ahead)
				{
					extraTorsoRot.x = ai.xAngle;
					extraTorsoRot.y = ai.angle;
				}

				if (Targetable(item, &ai))
				{
					item->Animation.TargetState = 7;
				}
				else
				{
					item->Animation.TargetState = 3;
				}

				break;

			case 6:
				creature->Flags = 0;

				if (ai.ahead)
				{
					extraTorsoRot.x = ai.xAngle;
					extraTorsoRot.y = ai.angle;
				}

				if (Targetable(item, &ai))
				{
					item->Animation.TargetState = 8;
				}
				else
				{
					item->Animation.TargetState = 3;
				}

				break;

			case 7:
				if (ai.ahead)
				{
					extraTorsoRot.x = ai.xAngle;
					extraTorsoRot.y = ai.angle;
				}

				if (creature->Flags == 0 && item->Animation.FrameNumber == 0)
				{
					ShotLara(item, &ai, WorkerDualGunBiteLeft, extraTorsoRot.y, 50);
					creature->MuzzleFlash[0].Bite = WorkerDualGunBiteLeft;
					creature->MuzzleFlash[0].Delay = 2;
					creature->Flags = 1;
				}

				break;

			case 8:
				if (ai.ahead)
				{
					extraTorsoRot.x = ai.xAngle;
					extraTorsoRot.y = ai.angle;
				}

				if (creature->Flags == 0 && item->Animation.FrameNumber == 0)
				{
					ShotLara(item, &ai, WorkerDualGunBiteRight, extraTorsoRot.y, 50);
					creature->MuzzleFlash[0].Bite = WorkerDualGunBiteRight;
					creature->MuzzleFlash[0].Delay = 2;
					creature->Flags = 1;
				}

				break;

			case 9:
				creature->Flags = 0;

				if (ai.ahead)
				{
					extraTorsoRot.x = ai.xAngle;
					extraTorsoRot.y = ai.angle;
				}

				if (Targetable(item, &ai))
				{
					item->Animation.TargetState = 10;
				}
				else
				{
					item->Animation.TargetState = 1;
				}

				break;

			case 10:
				if (ai.ahead)
				{
					extraTorsoRot.x = ai.xAngle;
					extraTorsoRot.y = ai.angle;
				}

				if (creature->Flags == 0 && item->Animation.FrameNumber == 0)
				{
					ShotLara(item, &ai, WorkerDualGunBiteLeft, extraTorsoRot.y, 50);
					ShotLara(item, &ai, WorkerDualGunBiteRight, extraTorsoRot.y, 50);
					creature->MuzzleFlash[0].Bite = WorkerDualGunBiteLeft;
					creature->MuzzleFlash[0].Delay = 1;
					creature->MuzzleFlash[1].Bite = WorkerDualGunBiteRight;
					creature->MuzzleFlash[1].Delay = 1;
					creature->Flags = 1;
				}

				break;
			}
		}

		CreatureTilt(item, tiltAngle);
		CreatureJoint(item, 0, extraTorsoRot.y);
		CreatureJoint(item, 1, extraTorsoRot.x);
		CreatureJoint(item, 2, extraHeadRot.y);
		CreatureJoint(item, 3, extraHeadRot.x);
		CreatureAnimation(itemNumber, headingAngle, tiltAngle);
	}
}
