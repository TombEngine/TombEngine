#include "framework.h"
#include "Objects/TR5/Entity/tr5_ghost.h"

#include "Game/control/box.h"
#include "Game/effects/effects.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Sound/sound.h"
#include "Specific/level.h"

namespace TEN::Entities::Creatures::TR5
{
	const auto InvisibleGhostBite = CreatureBiteInfo(Vector3::Zero, 17);

	void InitializeInvisibleGhost(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(*item, 0);
		item->Pose.Position.y += CLICK(2);
	}

	void InvisibleGhostControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short angle = 0;
		short joint0 = 0;
		short joint2 = 0;
		short joint1 = 0;

		if (item->AIBits)
			GetAITarget(creature);
		else if (creature->HurtByLara)
			creature->Enemy = LaraItem;

		AI_INFO AI;
		CreatureAIInfo(item, &AI);

		angle = CreatureTurn(item, creature->MaxTurn);
		if (abs(AI.angle) >= ANGLE(3.0f))
		{
			if (AI.angle > 0)
				item->Pose.Orientation.y += ANGLE(3.0f);
			else
				item->Pose.Orientation.y -= ANGLE(3.0f);
		}
		else
			item->Pose.Orientation.y += AI.angle;

		if (AI.ahead)
		{
			joint0 = AI.angle / 2;
			joint2 = AI.angle / 2;
			joint1 = AI.xAngle;
		}

		creature->MaxTurn = 0;

		if (item->Animation.ActiveState == 1)
		{
			creature->Flags = 0;

			if (AI.distance < pow(614, 2))
			{
				if (GetRandomControl() & 1)
					item->Animation.TargetState = 2;
				else
					item->Animation.TargetState = 3;
			}
		}
		else if (item->Animation.ActiveState > 1 &&
			item->Animation.ActiveState <= 3 &&
			!creature->Flags &&
			item->TouchBits & 0x9470 &&
			item->Animation.FrameNumber > 18)
		{
			DoDamage(creature->Enemy, 400);
			CreatureEffect2(item, InvisibleGhostBite, 10, item->Pose.Orientation.y, DoBloodSplat);
			creature->Flags = 1;
		}

		CreatureJoint(item, 0, joint0);
		CreatureJoint(item, 1, joint1);
		CreatureJoint(item, 2, joint2);

		if (AI.distance >= pow(BLOCK(1.5f), 2))
		{
			item->AfterDeath = 125;
			item->ItemFlags[0] = 0;
		}
		else
		{
			item->AfterDeath = sqrt(AI.distance) / 16;
			if (item->ItemFlags[0] == 0)
			{
				item->ItemFlags[0] = 1;
				SoundEffect(SFX_TR5_SKELETON_GHOST_APPEAR, &item->Pose);
			}
		}

		CreatureAnimation(itemNumber, angle, 0);
	}
}
