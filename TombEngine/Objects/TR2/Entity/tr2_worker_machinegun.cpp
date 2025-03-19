#include "framework.h"
#include "Objects/TR2/Entity/tr2_worker_machinegun.h"

#include "Game/Animation/Animation.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/items.h"
#include "Game/itemdata/creature_info.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Specific/level.h"

using namespace TEN::Animation;

namespace TEN::Entities::Creatures::TR2
{
	const auto WorkerMachineGunBite = CreatureBiteInfo(Vector3(0, 380, 37), 9);

	// TODO
	enum WorkerMachineGunState
	{

	};

	// TODO
	enum WorkerMachineGunAnim
	{

	};

	void InitializeWorkerMachineGun(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(item, 12);
	}

	static void ShootWorkerMachineGun(ItemInfo& item, AI_INFO& ai, const EulerAngles& extraTorsoRot)
	{
		auto& creature = *GetCreatureInfo(&item);

		ShotLara(&item, &ai, WorkerMachineGunBite, extraTorsoRot.y, 30);
		creature.MuzzleFlash[0].Bite = WorkerMachineGunBite;
		creature.MuzzleFlash[0].Delay = 2;
	}

	void WorkerMachineGunControl(short itemNumber)
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

		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState != 7)
				SetAnimation(*item, 19);
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
				creature->MaxTurn = 0;
				creature->Flags = 0;

				if (ai.ahead)
				{
					extraHeadRot.x = ai.xAngle;
					extraHeadRot.y = ai.angle;
				}

				if (creature->Mood == MoodType::Escape)
				{
					item->Animation.TargetState = 3;
				}
				else if (Targetable(item, &ai))
				{
					if (ai.distance < SQUARE(BLOCK(3)) || ai.zoneNumber != ai.enemyZone)
					{
						item->Animation.TargetState = (GetRandomControl() < 0x4000) ? 8 : 10;
					}
					else
					{
						item->Animation.TargetState = 2;
					}
				}
				else if (creature->Mood == MoodType::Attack || !ai.ahead)
				{
					if (ai.distance <= SQUARE(BLOCK(2)))
					{
						item->Animation.TargetState = 2;
					}
					else
					{
						item->Animation.TargetState = 3;
					}
				}
				else
				{
					item->Animation.TargetState = 4;
				}

				break;

			case 2:
				creature->MaxTurn = ANGLE(3.0f);

				if (ai.ahead)
				{
					extraHeadRot.x = ai.xAngle;
					extraHeadRot.y = ai.angle;
				}

				if (creature->Mood == MoodType::Escape)
				{
					item->Animation.TargetState = 3;
				}
				else if (Targetable(item, &ai))
				{
					if (ai.distance < SQUARE(BLOCK(3)) || ai.zoneNumber != ai.enemyZone)
					{
						item->Animation.TargetState = 1;
					}
					else
					{
						item->Animation.TargetState = 6;
					}
				}
				else if (creature->Mood == MoodType::Attack || !ai.ahead)
				{
					if (ai.distance > SQUARE(BLOCK(2)))
						item->Animation.TargetState = 3;
				}
				else
				{
					item->Animation.TargetState = 4;
				}

				break;

			case 3:
				creature->MaxTurn = ANGLE(5.0f);

				if (ai.ahead)
				{
					extraHeadRot.x = ai.xAngle;
					extraHeadRot.y = ai.angle;
				}

				if (creature->Mood != MoodType::Escape)
				{
					if (Targetable(item, &ai))
					{
						item->Animation.TargetState = 2;
					}
					else if (creature->Mood == MoodType::Bored || creature->Mood == MoodType::Stalk)
					{
						item->Animation.TargetState = 2;
					}
				}

				break;

			case 4:
				if (ai.ahead)
				{
					extraHeadRot.x = ai.xAngle;
					extraHeadRot.y = ai.angle;
				}

				if (Targetable(item, &ai))
				{
					item->Animation.TargetState = 5;
				}
				else
				{
					if (creature->Mood == MoodType::Attack)
					{
						item->Animation.TargetState = 1;
					}
					else if (!ai.ahead)
					{
						item->Animation.TargetState = 1;
					}
				}

				break;

			case 8:
			case 10:
				creature->Flags = 0;

				if (ai.ahead)
				{
					extraTorsoRot.x = ai.xAngle;
					extraTorsoRot.y = ai.angle;
				}

				if (Targetable(item, &ai))
				{
					item->Animation.TargetState = (item->Animation.ActiveState == 8) ? 5 : 11;
				}
				else
				{
					item->Animation.TargetState = 1;
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
					item->Animation.TargetState = 6;
				}
				else
				{
					item->Animation.TargetState = 2;
				}

				break;

			case 5:
			case 11:
				if (ai.ahead)
				{
					extraTorsoRot.x = ai.xAngle;
					extraTorsoRot.y = ai.angle;
				}

				if (item->Animation.AnimNumber == 2)
				{
					if (item->Animation.FrameNumber == 0)
					{
						ShootWorkerMachineGun(*item, ai, extraTorsoRot);
					}
					else if (item->Animation.FrameNumber == 6)
					{
						ShootWorkerMachineGun(*item, ai, extraTorsoRot);
					}
					else if (item->Animation.FrameNumber == 12)
					{
						ShootWorkerMachineGun(*item, ai, extraTorsoRot);
					}
				}
				else if (item->Animation.AnimNumber == 21 &&
					item->Animation.FrameNumber == 0)
				{
					ShootWorkerMachineGun(*item, ai, extraTorsoRot);
				}

				if (item->Animation.TargetState != 1 &&
					(creature->Mood == MoodType::Escape || ai.distance > SQUARE(BLOCK(3)) || !Targetable(item, &ai)))
				{
					item->Animation.TargetState = 1;
				}

				break;

			case 6:
				if (ai.ahead)
				{
					extraTorsoRot.x = ai.xAngle;
					extraTorsoRot.y = ai.angle;
				}

				if (item->Animation.FrameNumber == 0)
				{
					ShootWorkerMachineGun(*item, ai, extraTorsoRot);
				}
				else if (item->Animation.FrameNumber == 2)
				{
					ShootWorkerMachineGun(*item, ai, extraTorsoRot);
				}
				else if (item->Animation.FrameNumber == 6)
				{
					ShootWorkerMachineGun(*item, ai, extraTorsoRot);
				}
				else if (item->Animation.FrameNumber == 12)
				{
					ShootWorkerMachineGun(*item, ai, extraTorsoRot);
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
