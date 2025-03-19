#include "framework.h"
#include "Objects/TR5/Entity/tr5_imp.h"

#include "Game/Animation/Animation.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Objects/Generic/Object/burning_torch.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Entities::Generic;
using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR5
{
	constexpr auto IMP_ATTACK_DAMAGE = 3;

	constexpr auto IMP_WALK_RANGE			  = SQUARE(BLOCK(2));
	constexpr auto IMP_ATTACK_RANGE			  = SQUARE(BLOCK(0.25f));
	constexpr auto IMP_LIT_TORCH_SCARED_RANGE = BLOCK(2);

	constexpr auto IMP_WALK_TURN_RATE_MAX	= ANGLE(7.0f);
	constexpr auto IMP_RUN_TURN_RATE_MAX	= ANGLE(7.0f);
	constexpr auto IMP_SCARED_TURN_RATE_MAX = ANGLE(7.0f);
	constexpr auto IMP_ATTACK_TURN_RATE_MAX = ANGLE(2.0f);

	constexpr auto IMP_HEAD_MESH_SWAP_INTERVAL = 16;

	const auto ImpLeftHandBite	= CreatureBiteInfo(Vector3(0, 100, 0), 7);
	const auto ImpRightHandBite = CreatureBiteInfo(Vector3(0, 100, 0), 9);
	const auto ImpHeadMeshSwapJoints = std::vector<unsigned int>{ 10 };
	
	enum ImpState
	{
		IMP_STATE_WALK = 0,
		IMP_STATE_IDLE = 1,
		IMP_STATE_RUN = 2,
		IMP_STATE_ATTACK_1 = 3,
		IMP_STATE_JUMP_ATTACK = 5,
		IMP_STATE_SCARED = 6,
		IMP_STATE_VAULT_UP_1_STEP = 7,
		IMP_STATE_ROLL = 8,
		IMP_STATE_DEATH = 9,
		IMP_STATE_STONE_ATTACK = 11
	};

	enum ImpAnim
	{
		IMP_ANIM_WALK = 0,
		IMP_ANIM_IDLE = 1,
		IMP_ANIM_RUN = 2,
		IMP_ANIM_ATTACK = 3,
		IMP_ANIM_CRY = 4,
		IMP_ANIM_JUMP_ATTACK = 5,  
		IMP_ANIM_SCARED_RUN_AWAY = 6,
		IMP_ANIM_VAULT_UP_1_STEP = 7,
		IMP_ANIM_ROLL = 8,
		IMP_ANIM_BACKWARD_DEATH = 9,
		IMP_ANIM_FORWARD_DEATH = 10,
		IMP_ANIM_IDLE_TO_WALK_LEFT = 11,
		IMP_ANIM_IDLE_TO_WALK_RIGHT = 12,
		IMP_ANIM_SLOW_TO_WAIT_LEFT = 13,
		IMP_ANIM_SLOW_TO_WAIT_RIGHT = 14,
		IMP_ANIM_SLOW_TO_RUN = 15,
		IMP_ANIM_SLOW_TO_WALK = 16,
		IMP_ANIM_STONE_ATTACK = 17
	};

	enum ImpOcb
	{
		IMP_OCB_CLIMB_UP = 1,
		IMP_OCB_ROLL = 2,
		IMP_OCB_STONE_ATTACK = 3
	};

	static void RotateTowardTarget(ItemInfo& item, const AI_INFO& ai, short turnRate)
	{
		if (abs(ai.angle) < turnRate)
		{
			item.Pose.Orientation.y += ai.angle;
		}
		else if (ai.angle < 0)
		{
			item.Pose.Orientation.y -= turnRate;
		}
		else
		{
			item.Pose.Orientation.y += turnRate;
		}
	}

	static void DoImpStoneAttack(ItemInfo* item)
	{
		auto pos1 = GetJointPosition(item, ImpRightHandBite);
		auto pos2 = GetJointPosition(LaraItem, LM_HEAD);
		auto orient = Geometry::GetOrientToPoint(pos1.ToVector3(), pos2.ToVector3());

		int distance = Vector3i::Distance(pos1, pos2);
		if (distance < 8)
			distance = 8;

		orient.x += short(GetRandomControl() % (distance / 2) - (distance / 4));
		orient.y += short(GetRandomControl() % (distance / 4) - (distance / 8));

		int fxNumber = CreateNewEffect(item->RoomNumber);
		if (fxNumber == NO_VALUE)
			return;

		auto& fx = EffectList[fxNumber];

		fx.objectNumber = ID_IMP_ROCK;
		fx.frameNumber = Objects[ID_IMP_ROCK].meshIndex;

		fx.pos.Position = pos1;
		fx.roomNumber = item->RoomNumber;
		fx.speed = sqrt(distance) * 4;

		fx.pos.Orientation = EulerAngles(orient.x + (distance / 2), orient.y, 0);

		if (fx.speed < BLOCK(0.25f))
			fx.speed = BLOCK(0.25f);

		fx.fallspeed = 0;
		fx.color = Vector4::One;
		fx.counter = 0;
		fx.flag1 = 2;
		fx.flag2 = 0x2000;
	}

	static bool IsTorchLitNearby(ItemInfo& item)
	{
		const auto& creature = *GetCreatureInfo(&item);

		if (creature.Enemy->IsLara())
		{
			const auto& player = *GetLaraInfo(creature.Enemy);

			float distance = Vector3i::Distance(item.Pose.Position, creature.Enemy->Pose.Position);
			if (player.Torch.IsLit && distance <= IMP_LIT_TORCH_SCARED_RANGE)
				return true;
		}

		auto torchItemsNumbers = FindCreatedItems(ID_BURNING_TORCH_ITEM);
		for (auto& itemNumber : torchItemsNumbers)
		{
			if (itemNumber == NO_VALUE)
				continue;

			const auto& torchItem = g_Level.Items[itemNumber];

			float distance = Vector3i::Distance(item.Pose.Position, torchItem.Pose.Position);
			if (torchItem.ItemFlags[3] != 0 && distance <= IMP_LIT_TORCH_SCARED_RANGE)
				return true;
		}

		return false;
	}

	void InitializeImp(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);

		switch (item.TriggerFlags)
		{
		case IMP_OCB_ROLL:
			SetAnimation(item, IMP_ANIM_ROLL);
			break;

		case IMP_OCB_CLIMB_UP:
			SetAnimation(item, IMP_ANIM_VAULT_UP_1_STEP);
			break;

		default:
			SetAnimation(item, IMP_ANIM_IDLE);
			break;
		}
	}

	void ImpControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short headingAngle = 0;

		auto extraHeadRot = EulerAngles::Identity;
		auto extraTorsoRot = EulerAngles::Identity;

		AI_INFO ai;
		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState != IMP_STATE_DEATH)
			{
				CreatureAIInfo(item, &ai);

				if (ai.angle > -ANGLE(67.5f) && ai.angle < ANGLE(67.5f))
					SetAnimation(*item, IMP_ANIM_BACKWARD_DEATH);
				else
					SetAnimation(*item, IMP_ANIM_FORWARD_DEATH);
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

			CreatureAIInfo(item, &ai);

			int elevation = (item->Pose.Position.y - creature->Enemy->Pose.Position.y) + CLICK(1.5f);
			if (creature->Enemy->IsLara())
			{
				if (creature->Enemy->Animation.ActiveState == LS_CROUCH_IDLE ||
					creature->Enemy->Animation.ActiveState == LS_CROUCH_ROLL ||
					creature->Enemy->Animation.ActiveState > LS_MONKEY_TURN_180 &&
					creature->Enemy->Animation.ActiveState < LS_HANG_TO_CRAWL ||
					creature->Enemy->Animation.ActiveState == LS_CROUCH_TURN_LEFT ||
					creature->Enemy->Animation.ActiveState == LS_CROUCH_TURN_RIGHT)
				{
					elevation = item->Pose.Position.y - creature->Enemy->Pose.Position.y;
				}
			}
			
			ai.xAngle = phd_atan(sqrt(ai.distance), elevation);

			GetCreatureMood(item, &ai, true);
			if (item->Animation.ActiveState == IMP_STATE_SCARED)
				creature->Mood = MoodType::Escape;

			CreatureMood(item, &ai, true);

			headingAngle = CreatureTurn(item, creature->MaxTurn);

			if (Wibble & IMP_HEAD_MESH_SWAP_INTERVAL)
				item->SetMeshSwapFlags (ImpHeadMeshSwapJoints);
			else
				item->SetMeshSwapFlags(NO_JOINT_BITS);

			if (ai.ahead)
			{
				extraTorsoRot.x = ai.xAngle / 2;
				extraTorsoRot.y = ai.angle / 2;
				extraHeadRot.x = ai.xAngle / 2;
				extraHeadRot.y = ai.angle / 2;
			}

			bool isScared = IsTorchLitNearby(*item);

			switch (item->Animation.ActiveState)
			{
			case IMP_STATE_WALK:
				creature->MaxTurn = IMP_RUN_TURN_RATE_MAX;

				if (isScared)
				{
					item->Animation.TargetState = IMP_STATE_SCARED;
					break;
				}

				if (ai.distance > IMP_WALK_RANGE)
				{
					item->Animation.TargetState = IMP_STATE_RUN;
				}
				else if (ai.bite && ai.distance < IMP_ATTACK_RANGE)
				{
					item->Animation.TargetState = IMP_STATE_IDLE;
				}

				break;

			case IMP_STATE_IDLE:
				creature->MaxTurn = 0;
				creature->Flags = 0;

				if (isScared)
				{
					item->Animation.TargetState = IMP_STATE_SCARED;
					break;
				}

				if (ai.bite && ai.distance < IMP_ATTACK_RANGE)
				{
					if (Random::TestProbability(1 / 2.0f))
						item->Animation.TargetState = IMP_STATE_ATTACK_1;
					else
						item->Animation.TargetState = IMP_STATE_JUMP_ATTACK;
				}
				else if (item->AIBits == FOLLOW)
				{
					item->Animation.TargetState = IMP_STATE_WALK;
				}
				else if(item->TriggerFlags == IMP_OCB_STONE_ATTACK)
				{
					item->Animation.TargetState = IMP_STATE_STONE_ATTACK;
				}
				else if (ai.distance > IMP_WALK_RANGE)
				{
					item->Animation.TargetState = IMP_STATE_RUN;
				}
				else if (ai.distance > IMP_ATTACK_RANGE)
				{
					item->Animation.TargetState = IMP_STATE_WALK;
				}

				break;

			case IMP_STATE_RUN:
				creature->MaxTurn = IMP_RUN_TURN_RATE_MAX;

				if (isScared)
				{
					item->Animation.TargetState = IMP_STATE_SCARED;
					break;
				}

				if (ai.distance < IMP_ATTACK_RANGE)
				{
					item->Animation.TargetState = IMP_STATE_IDLE;
				}
				else if (ai.distance < IMP_WALK_RANGE)
				{
					item->Animation.TargetState = IMP_STATE_WALK;
				}

				break;

			case IMP_STATE_ATTACK_1:
				creature->MaxTurn = 0;

				if (!(creature->Flags & 1) && item->TouchBits.Test(ImpRightHandBite.BoneID))
				{
					DoDamage(creature->Enemy, IMP_ATTACK_DAMAGE);
					CreatureEffect2(item, ImpRightHandBite, 10, item->Pose.Orientation.y, DoBloodSplat);
					creature->Flags |= 1;
				}
				
				break;

			case IMP_STATE_JUMP_ATTACK:
				RotateTowardTarget(*item, ai, IMP_ATTACK_TURN_RATE_MAX);

				if (!(creature->Flags & 1) && item->TouchBits.Test(ImpRightHandBite.BoneID))
				{
					DoDamage(creature->Enemy, IMP_ATTACK_DAMAGE);
					CreatureEffect(item, ImpRightHandBite, DoBloodSplat);
					creature->Flags |= 1;
				}

				if (!(creature->Flags & 2) && item->TouchBits.Test(ImpLeftHandBite.BoneID))
				{
					DoDamage(creature->Enemy, IMP_ATTACK_DAMAGE);
					CreatureEffect(item, ImpLeftHandBite, DoBloodSplat);
					creature->Flags |= 2;
				}

				break;

			case IMP_STATE_SCARED:
				creature->MaxTurn = IMP_SCARED_TURN_RATE_MAX;

				if (!isScared)
				{
					if (ai.distance > IMP_WALK_RANGE)
						item->Animation.TargetState = IMP_STATE_RUN;
					else
						item->Animation.TargetState = IMP_STATE_WALK;
				}

				break;

			case IMP_STATE_VAULT_UP_1_STEP:
			case IMP_STATE_ROLL:
				creature->MaxTurn = 0;
				break;

			case IMP_STATE_STONE_ATTACK:
				RotateTowardTarget(*item, ai, IMP_ATTACK_TURN_RATE_MAX);
				
				if (item->Animation.FrameNumber == 40)
					DoImpStoneAttack(item);

				break;
			}
		}

		CreatureJoint(item, 0, extraTorsoRot.x);
		CreatureJoint(item, 1, extraTorsoRot.y);
		CreatureJoint(item, 2, extraHeadRot.x);
		CreatureJoint(item, 3, extraHeadRot.y);
		CreatureAnimation(itemNumber, headingAngle, 0);
	}
}
