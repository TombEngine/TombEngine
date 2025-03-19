#include "framework.h"
#include "Objects/TR5/Entity/tr5_brownbeast.h"

#include "Game/control/box.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/misc.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Sound/sound.h"
#include "Math/Math.h"
#include "Specific/level.h"

using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR5
{
	constexpr auto BROWN_BEAST_ATTACK_DAMAGE = 150;

	const auto BrownBeastBite1 = CreatureBiteInfo(Vector3::Zero, 16);
	const auto BrownBeastBite2 = CreatureBiteInfo(Vector3::Zero, 22);
	const auto BrownBeastAttackJoints1 = std::vector<unsigned int>{ 14, 15, 16, 17 };
	const auto BrownBeastAttackJoints2 = std::vector<unsigned int>{ 20, 21, 22, 23 };

	// TODO
	enum BrownBeastState
	{

	};

	// TODO
	enum BrownBeastAnim
	{

	};

	void InitializeBrownBeast(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(*item, 0);
	}

	void ControlBrowsBeast(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short headingAngle = 0;

		if (item->HitPoints <= 0)
		{
			item->HitPoints = 0;
			if (item->Animation.ActiveState != 7)
				SetAnimation(*item, 10);
		}
		else
		{
			if (item->AIBits)
				GetAITarget(creature);
			else if (creature->HurtByLara)
				creature->Enemy = LaraItem;

			AI_INFO ai;
			CreatureAIInfo(item, &ai);

			int distance;

			if (creature->Enemy == LaraItem)
				distance = ai.distance;
			else
			{
				int dx = LaraItem->Pose.Position.x - item->Pose.Position.x;
				int dz = LaraItem->Pose.Position.z - item->Pose.Position.z;
				phd_atan(dz, dz);

				distance = pow(dx, 2) + pow(dz, 2);
			}

			GetCreatureMood(item, &ai, true);
			CreatureMood(item, &ai, true);

			headingAngle = CreatureTurn(item, creature->MaxTurn);
			creature->MaxTurn = ANGLE(7.0f);

			switch (item->Animation.ActiveState)
			{
			case 1:
				creature->Flags = 0;

				if (creature->Mood == MoodType::Attack)
				{
					if (distance <= pow(BLOCK(1), 2))
					{
						if (Random::TestProbability(1 / 2.0f))
							item->Animation.TargetState = 4;
						else
							item->Animation.TargetState = 6;
					}
					else if (Random::TestProbability(1 / 2.0f))
						item->Animation.TargetState = 2;
					else
						item->Animation.TargetState = 3;
				}
				else
					item->Animation.TargetState = 1;

				break;

			case 2:
			case 3:
				if (distance < pow(BLOCK(1), 2) || creature->Mood != MoodType::Attack)
					item->Animation.TargetState = 1;

				SoundEffect(SFX_TR5_IMP_BARREL_ROLL, &item->Pose);
				break;

			case 4:
			case 6:
				creature->MaxTurn = 0;

				if (abs(ai.angle) >= ANGLE(2.0f))
				{
					if (ai.angle > 0)
						item->Pose.Orientation.y += ANGLE(2.0f);
					else
						item->Pose.Orientation.y -= ANGLE(2.0f);
				}
				else
					item->Pose.Orientation.y += ai.angle;

				if (creature->Flags)
					break;

				if (item->TouchBits.Test(BrownBeastAttackJoints1))
				{
					if (item->Animation.AnimNumber == 8)
					{
						if (TestAnimFrameRange(*item, 20, 24))
						{
							DoDamage(creature->Enemy, BROWN_BEAST_ATTACK_DAMAGE);
							CreatureEffect2(item, BrownBeastBite1, 20, item->Pose.Orientation.y, DoBloodSplat);
							creature->Flags |= 1;
							break;
						}
					}

					if (item->Animation.AnimNumber == 2)
					{
						if (TestAnimFrameRange(*item, 7, 15))
						{
							DoDamage(creature->Enemy, BROWN_BEAST_ATTACK_DAMAGE);
							CreatureEffect2(item, BrownBeastBite1, 20, item->Pose.Orientation.y, DoBloodSplat);
							creature->Flags |= 1;
							break;
						}
					}
				}

				if (!item->TouchBits.Test(BrownBeastAttackJoints2))
					break;

				if (item->Animation.AnimNumber == 8)
				{
					if (TestAnimFrameRange(*item, 14, 19))
					{
						DoDamage(creature->Enemy, BROWN_BEAST_ATTACK_DAMAGE);
						CreatureEffect2(item, BrownBeastBite2, 20, item->Pose.Orientation.y, DoBloodSplat);
						creature->Flags |= 2;
						break;
					}
				}

				if (item->Animation.AnimNumber == 2)
				{
					if (TestAnimFrameRange(*item, 34, 42))
					{
						DoDamage(creature->Enemy, BROWN_BEAST_ATTACK_DAMAGE);
						CreatureEffect2(item, BrownBeastBite2, 20, item->Pose.Orientation.y, DoBloodSplat);
						creature->Flags |= 2;
						break;
					}
				}

				break;

			default:
				break;
			}
		}

		CreatureAnimation(itemNumber, headingAngle, 0);
	}
}
