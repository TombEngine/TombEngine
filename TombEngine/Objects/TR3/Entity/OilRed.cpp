#include "framework.h"
#include "Objects/TR3/Entity/OilRed.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/control/box.h"
#include "Game/effects/effects.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Collision::Point;
using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR3
{
	constexpr auto OILRED_SHOT_DAMAGE = 35;

	constexpr auto OILRED_WALK_RANGE				= SQUARE(BLOCK(2));
	constexpr auto OILRED_AWARE_DISTANCE			= SQUARE(BLOCK(1));
	constexpr auto OILRED_FEELER_DISTANCE			= BLOCK(1);

	constexpr auto OILRED_WALK_TURN_RATE_MAX		= ANGLE(6.0f);
	constexpr auto OILRED_RUN_TURN_RATE_MAX			= ANGLE(10.0f);

	constexpr auto OILRED_DEATH_SHOT_ANGLE = ANGLE(45.0f);

	const auto OilRedGunBite = CreatureBiteInfo(Vector3(0, 160, 40), 13);

	enum class OilRedAnim
	{
		OneHandShoot = 1,
		OneHandAim = 12,
		Die = 14,
		WalkToStop = 17,
		WalkShootRight = 18,
		WalkShootLeft = 19,
		RunStopLeft = 27,
		RunStopRight = 28,
	};
	
	enum class OilRedState
	{
		Empty = 0,
		Wait = 1,
		Walk = 2,
		Run = 3,
		OneHandAim = 4,
		OneHandShoot = 5,
		TwoHandAim = 6,
		TwoHandShoot = 7,
		HorizontalShootRight = 8,
		HorizontalShootLeft = 9,
		WalkShootRight = 10,
		HorizontalShootPrepare = 11,
		WalkToWalkShoot = 12,
		Death = 13,
		WalkShootLeft = 14,
		StandToCrouch = 15,
		Crouch = 16,
		CrouchAim = 17,
		CrouchShoot = 18,
		CrouchWalk = 19,
		CrouchToStand = 20
	};

    void ControlOilRed(short itemNumber)
    {
        if (!CreatureActive(itemNumber))
            return;

        auto* item = &g_Level.Items[itemNumber];
        auto* creature = GetCreatureInfo(item);

        short angle = 0;
        short tilt = 0;
        short head = 0;
        auto extraTorsoRot = EulerAngles::Identity;

        if (creature->MuzzleFlash[0].Delay != 0)
            creature->MuzzleFlash[0].Delay--;
        
        if (item->HitPoints <= 0)
        {
            item->HitPoints = 0;
            if (item->Animation.ActiveState != (int)OilRedState::Death)
            {
                SetAnimation(*item, (int)OilRedAnim::Die);
                item->ItemFlags[FINAL_SHOT_FLAG_INDEX] = Random::GenerateInt(1, FINAL_SHOT_COUNT);
            }
            else
            {
                PerformFinalAttack(*item, OilRedGunBite, 8, (int)OilRedAnim::Die, OILRED_SHOT_DAMAGE * 3, SFX_TR3_OIL_SMG_FIRE);
            }
        }
        else
        {
            if (item->AIBits)
                GetAITarget(creature);
            else
                creature->Enemy = LaraItem;

            AI_INFO ai;
            CreatureAIInfo(item, &ai);

            AI_INFO laraAI;
            if (creature->Enemy == LaraItem)
            {
                laraAI.angle = ai.angle;
                laraAI.distance = ai.distance;
            }
            else
            {
                int dx = LaraItem->Pose.Position.x - item->Pose.Position.x;
                int dz = LaraItem->Pose.Position.z - item->Pose.Position.z;
                laraAI.angle = phd_atan(dz, dx) - item->Pose.Orientation.y;
                laraAI.distance = SQUARE(dx) + SQUARE(dz);
            }

            bool violent = (creature->Enemy != LaraItem);
            GetCreatureMood(item, &ai, violent);
            CreatureMood(item, &ai, violent);

            angle = CreatureTurn(item, creature->MaxTurn);

            // Calculate near cover
            int x = item->Pose.Position.x + OILRED_FEELER_DISTANCE * phd_sin(item->Pose.Orientation.y + laraAI.angle);
            int y = item->Pose.Position.y;
            int z = item->Pose.Position.z + OILRED_FEELER_DISTANCE * phd_cos(item->Pose.Orientation.y + laraAI.angle);
            int height = GetPointCollision(Vector3i(x, y, z), item->RoomNumber).GetFloorHeight();
            bool nearCover = (item->Pose.Position.y > (height + CLICK(1.5f)) &&
                item->Pose.Position.y < (height + CLICK(4.5f)) &&
                laraAI.distance > OILRED_AWARE_DISTANCE);

            auto* realEnemy = creature->Enemy;
            creature->Enemy = LaraItem;

            if ((laraAI.distance < OILRED_AWARE_DISTANCE || item->HitStatus || TargetVisible(item, &laraAI)) &&
                !(item->AIBits & FOLLOW))
            {
                if (!creature->Alerted)
                    SoundEffect(SFX_TR3_AMERCAN_HOY, &item->Pose);

                AlertAllGuards(itemNumber);
            }

            creature->Enemy = realEnemy;

            switch ((OilRedState)item->Animation.ActiveState)
            {
            case OilRedState::Wait:
                head = laraAI.angle;
                creature->MaxTurn = 0;

                if (item->Animation.AnimNumber == (int)OilRedAnim::WalkToStop ||
                    item->Animation.AnimNumber == (int)OilRedAnim::RunStopLeft ||
                    item->Animation.AnimNumber == (int)OilRedAnim::RunStopRight)
                {
                    if (abs(ai.angle) < OILRED_RUN_TURN_RATE_MAX)
                        item->Pose.Orientation.y += ai.angle;
                    else if (ai.angle < 0)
                        item->Pose.Orientation.y -= OILRED_RUN_TURN_RATE_MAX;
                    else
                        item->Pose.Orientation.y += OILRED_RUN_TURN_RATE_MAX;
                }

                if (item->AIBits & GUARD)
                {
                    head = AIGuard(creature);
                    item->Animation.TargetState = (int)OilRedState::Wait;
                }
                else if (item->AIBits & PATROL1)
                {
                    item->Animation.TargetState = (int)OilRedState::Walk;
                }
                else if (nearCover && (Lara.TargetEntity == item || item->HitStatus))
                {
                    item->Animation.TargetState = (int)OilRedState::StandToCrouch;
                }
                else if (item->Animation.RequiredState == (int)OilRedState::StandToCrouch)
                {
                    item->Animation.TargetState = (int)OilRedState::StandToCrouch;
                }
                else if (creature->Mood == MoodType::Escape)
                {
                    item->Animation.TargetState = (int)OilRedState::Run;
                }
                else if (Targetable(item, &ai))
                {
                    if (ai.distance > OILRED_WALK_RANGE)
                    {
                        item->Animation.TargetState = (int)OilRedState::Walk;
                    }
                    else
                    {
                        int random = GetRandomControl();
                        if (random < 0x2000)
                            item->Animation.TargetState = (int)OilRedState::OneHandShoot;
                        else if (random < 0x4000)
                            item->Animation.TargetState = (int)OilRedState::TwoHandShoot;
                        else
                            item->Animation.TargetState = (int)OilRedState::HorizontalShootPrepare;
                    }
                }
                else if (creature->Mood == MoodType::Bored ||
                    ((item->AIBits & FOLLOW) && (creature->ReachedGoal || laraAI.distance > SQUARE(BLOCK(2)))))
                {
                    if (ai.ahead)
                        item->Animation.TargetState = (int)OilRedState::Wait;
                    else
                        item->Animation.TargetState = (int)OilRedState::Walk;
                }
                else
                {
                    item->Animation.TargetState = (int)OilRedState::Run;
                }

                break;

            case OilRedState::Walk:
                head = laraAI.angle;
                creature->MaxTurn = OILRED_WALK_TURN_RATE_MAX;

                if (item->AIBits & PATROL1)
                {
                    item->Animation.TargetState = (int)OilRedState::Walk;
                    head = 0;
                }
                else if (nearCover && (Lara.TargetEntity == item || item->HitStatus))
                {
                    item->Animation.RequiredState = (int)OilRedState::StandToCrouch;
                    item->Animation.TargetState = (int)OilRedState::Wait;
                }
                else if (creature->Mood == MoodType::Escape)
                {
                    item->Animation.TargetState = (int)OilRedState::Run;
                }
                else if (Targetable(item, &ai))
                {
                    if (ai.distance > OILRED_WALK_RANGE && ai.zoneNumber == ai.enemyZone)
                        item->Animation.TargetState = (int)OilRedState::WalkToWalkShoot;
                    else
                        item->Animation.TargetState = (int)OilRedState::Wait;
                }
                else if (creature->Mood == MoodType::Bored)
                {
                    if (ai.ahead)
                        item->Animation.TargetState = (int)OilRedState::Walk;
                    else
                        item->Animation.TargetState = (int)OilRedState::Wait;
                }
                else
                {
                    item->Animation.TargetState = (int)OilRedState::Run;
                }

                break;

            case OilRedState::Run:
                if (ai.ahead)
                    head = ai.angle;

                creature->MaxTurn = OILRED_RUN_TURN_RATE_MAX;
                tilt = angle / 2;

                if (item->AIBits & GUARD)
                {
                    item->Animation.TargetState = (int)OilRedState::Wait;
                }
                else if (nearCover && (Lara.TargetEntity == item || item->HitStatus))
                {
                    item->Animation.RequiredState = (int)OilRedState::StandToCrouch;
                    item->Animation.TargetState = (int)OilRedState::Wait;
                }
                else if (creature->Mood == MoodType::Escape)
                {
                    break;
                }
                else if (Targetable(item, &ai) ||
                    ((item->AIBits & FOLLOW) && (creature->ReachedGoal || laraAI.distance > SQUARE(BLOCK(2)))))
                {
                    item->Animation.TargetState = (int)OilRedState::Wait;
                }
                else if (creature->Mood == MoodType::Bored)
                {
                    item->Animation.TargetState = (int)OilRedState::Walk;
                }

                break;

            case OilRedState::OneHandAim:
                if (ai.ahead)
                {
                    extraTorsoRot.y = ai.angle;
                    extraTorsoRot.x = ai.xAngle;
                }

                if ((item->Animation.AnimNumber == (int)OilRedAnim::OneHandAim) ||
                    (item->Animation.AnimNumber == (int)OilRedAnim::OneHandShoot &&
                        item->Animation.FrameNumber == 10))
                {
                    if (!ShotLara(item, &ai, OilRedGunBite, extraTorsoRot.y, OILRED_SHOT_DAMAGE))
                        item->Animation.RequiredState = (int)OilRedState::Wait;

                    creature->MuzzleFlash[0].Bite = OilRedGunBite;
                    creature->MuzzleFlash[0].Delay = 2;
                }
                else if (item->HitStatus && Random::TestProbability(0.25f) && nearCover)
                {
                    item->Animation.RequiredState = (int)OilRedState::StandToCrouch;
                    item->Animation.TargetState = (int)OilRedState::Wait;
                }

                break;

            case OilRedState::OneHandShoot:
                if (ai.ahead)
                {
                    extraTorsoRot.y = ai.angle;
                    extraTorsoRot.x = ai.xAngle;
                }

                if (item->Animation.FrameNumber == 0)
                {
                    creature->MuzzleFlash[0].Bite = OilRedGunBite;
                    creature->MuzzleFlash[0].Delay = 1;
                }

                if (item->Animation.RequiredState == (int)OilRedState::Wait)
                    item->Animation.TargetState = (int)OilRedState::Wait;

                break;

            case OilRedState::TwoHandShoot:
                if (ai.ahead)
                {
                    extraTorsoRot.y = ai.angle;
                    extraTorsoRot.x = ai.xAngle;
                }

                if (item->Animation.FrameNumber == 0)
                {
                    if (!ShotLara(item, &ai, OilRedGunBite, extraTorsoRot.y, OILRED_SHOT_DAMAGE))
                        item->Animation.TargetState = (int)OilRedState::Wait;

                    creature->MuzzleFlash[0].Bite = OilRedGunBite;
                    creature->MuzzleFlash[0].Delay = 2;
                }
                else if (item->HitStatus && Random::TestProbability(0.25f) && nearCover)
                {
                    item->Animation.RequiredState = (int)OilRedState::StandToCrouch;
                    item->Animation.TargetState = (int)OilRedState::Wait;
                }

                break;

            case OilRedState::HorizontalShootRight:
            case OilRedState::HorizontalShootLeft:
                if (ai.ahead)
                {
                    extraTorsoRot.y = ai.angle;
                    extraTorsoRot.x = ai.xAngle;
                }

                if (item->Animation.FrameNumber == 0 ||
                    item->Animation.FrameNumber == 11)
                {
                    if (!ShotLara(item, &ai, OilRedGunBite, extraTorsoRot.y, OILRED_SHOT_DAMAGE))
                        item->Animation.TargetState = (int)OilRedState::Wait;

                    creature->MuzzleFlash[0].Bite = OilRedGunBite;
                    creature->MuzzleFlash[0].Delay = 2;
                }
                else if (item->HitStatus && Random::TestProbability(0.25f) && nearCover)
                {
                    item->Animation.RequiredState = (int)OilRedState::StandToCrouch;
                    item->Animation.TargetState = (int)OilRedState::Wait;
                }

                break;

            case OilRedState::WalkToWalkShoot:
                if (ai.ahead)
                {
                    extraTorsoRot.y = ai.angle;
                    extraTorsoRot.x = ai.xAngle;
                }

                if ((item->Animation.AnimNumber == (int)OilRedAnim::WalkShootRight &&
                    item->Animation.FrameNumber == 17) ||
                    (item->Animation.AnimNumber == (int)OilRedAnim::WalkShootLeft &&
                        item->Animation.FrameNumber == 6))
                {
                    if (!ShotLara(item, &ai, OilRedGunBite, extraTorsoRot.y, OILRED_SHOT_DAMAGE))
                        item->Animation.RequiredState = (int)OilRedState::Walk;

                    creature->MuzzleFlash[0].Bite = OilRedGunBite;
                    creature->MuzzleFlash[0].Delay = 2;
                }
                else if (item->HitStatus && Random::TestProbability(0.25f) && nearCover)
                {
                    item->Animation.RequiredState = (int)OilRedState::StandToCrouch;
                    item->Animation.TargetState = (int)OilRedState::Wait;
                }

                if (ai.distance < OILRED_WALK_RANGE)
                    item->Animation.RequiredState = (int)OilRedState::Walk;

                break;

            case OilRedState::WalkShootRight:
            case OilRedState::WalkShootLeft:
                if (ai.ahead)
                {
                    extraTorsoRot.y = ai.angle;
                    extraTorsoRot.x = ai.xAngle;
                }

                if (item->Animation.RequiredState == (int)OilRedState::Walk)
                    item->Animation.TargetState = (int)OilRedState::Walk;

                if (item->Animation.FrameNumber == 16)
                {
                    if (!ShotLara(item, &ai, OilRedGunBite, extraTorsoRot.y, OILRED_SHOT_DAMAGE))
                        item->Animation.TargetState = (int)OilRedState::Walk;

                    creature->MuzzleFlash[0].Bite = OilRedGunBite;
                    creature->MuzzleFlash[0].Delay = 2;
                }

                if (ai.distance < OILRED_WALK_RANGE)
                    item->Animation.TargetState = (int)OilRedState::Walk;

                break;

            case OilRedState::Crouch:
                if (ai.ahead)
                    head = ai.angle;

                creature->MaxTurn = 0;

                if (Targetable(item, &ai))
                {
                    item->Animation.TargetState = (int)OilRedState::CrouchAim;
                }
                else if (item->HitStatus || !nearCover || (ai.ahead && Random::TestProbability(1 / 32.0f)))
                {
                    item->Animation.TargetState = (int)OilRedState::CrouchToStand;
                }
                else
                {
                    item->Animation.TargetState = (int)OilRedState::CrouchWalk;
                }

                break;

            case OilRedState::CrouchAim:
                creature->MaxTurn = ANGLE(1.0f);

                if (ai.ahead)
                    extraTorsoRot.y = ai.angle;

                if (Targetable(item, &ai))
                    item->Animation.TargetState = (int)OilRedState::CrouchShoot;
                else
                    item->Animation.TargetState = (int)OilRedState::Crouch;

                break;

            case OilRedState::CrouchShoot:
                if (ai.ahead)
                    extraTorsoRot.y = ai.angle;

                if (item->Animation.FrameNumber == 0)
                {
                    if (!ShotLara(item, &ai, OilRedGunBite, extraTorsoRot.y, OILRED_SHOT_DAMAGE) ||
                        Random::TestProbability(1 / 8.0f))
                    {
                        item->Animation.TargetState = (int)OilRedState::Crouch;
                    }

                    creature->MuzzleFlash[0].Bite = OilRedGunBite;
                    creature->MuzzleFlash[0].Delay = 2;
                }

                break;

            case OilRedState::CrouchWalk:
                if (ai.ahead)
                    head = ai.angle;

                creature->MaxTurn = OILRED_WALK_TURN_RATE_MAX;

                if (Targetable(item, &ai) || item->HitStatus || !nearCover ||
                    (ai.ahead && Random::TestProbability(1 / 32.0f)))
                {
                    item->Animation.TargetState = (int)OilRedState::Crouch;
                }

                break;

            case OilRedState::CrouchToStand:
                if (abs(ai.angle) < OILRED_WALK_TURN_RATE_MAX)
                    item->Pose.Orientation.y += ai.angle;
                else if (ai.angle < 0)
                    item->Pose.Orientation.y -= OILRED_WALK_TURN_RATE_MAX;
                else
                    item->Pose.Orientation.y += OILRED_WALK_TURN_RATE_MAX;

                break;
            }
        }

        CreatureTilt(item, tilt);
        CreatureJoint(item, 0, extraTorsoRot.y);
        CreatureJoint(item, 1, extraTorsoRot.x);
        CreatureJoint(item, 2, head);
        CreatureAnimation(itemNumber, angle, 0);
    }
}
