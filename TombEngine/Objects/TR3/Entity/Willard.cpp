#include "framework.h"
#include "Objects/TR3/Entity/Willard.h"

#include "Game/Animation/Animation.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/effects/item_fx.h"
#include "Game/effects/tomb4fx.h"
#include "Game/items.h"
#include "Game/itemdata/creature_info.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "Specific/level.h"
#include "Objects/Effects/enemy_missile.h"

using namespace TEN::Animation;
using namespace TEN::Math;
using namespace TEN::Entities::Effects;

namespace TEN::Entities::Creatures::TR3
{
	constexpr auto WILLARD_BITE_DAMAGE		 = 220;
	constexpr auto WILLARD_TOUCH_DAMAGE		 = 10;
	constexpr auto WILLARD_HP_AFTER_KO		 = 200;
	constexpr auto WILLARD_KO_TIME			 = 280;
	constexpr auto WILLARD_PATH_DISTANCE	 = 1024;

	constexpr auto WILLARD_ATTACK_RANGE		 = SQUARE(BLOCK(1.5f));
	constexpr auto WILLARD_LUNGE_RANGE		 = SQUARE(BLOCK(2));
	constexpr auto WILLARD_FIRE_RANGE		 = SQUARE(BLOCK(4));

	constexpr auto WILLARD_TURN				 = ANGLE(5.0f);
	constexpr auto WILLARD_ATTACK_TURN		 = ANGLE(2.0f);

	constexpr auto WILLARD_TOUCH			 = 0x900000;

	constexpr auto NO_AI_PATH = -1;
	constexpr auto MAX_PATH_POINTS = 16;
	constexpr auto MAX_JUNCTIONS = 4;

	const auto WillardBiteLeft = CreatureBiteInfo(Vector3(19, -13, 3), 20);
	const auto WillardBiteRight = CreatureBiteInfo(Vector3(19, -13, 3), 23);
	const auto WillardBiteAttackJoints = std::vector<unsigned int>{ 20, 21, 22, 23 };

	enum WillardState
	{
		WILLARD_STATE_STOP = 0,
		WILLARD_STATE_WALK = 1,
		WILLARD_STATE_LUNGE = 2,
		WILLARD_STATE_BIGKILL = 3,
		WILLARD_STATE_STUNNED = 4,
		WILLARD_STATE_KNOCKOUT = 5,
		WILLARD_STATE_GETUP = 6,
		WILLARD_STATE_WALKATAK1 = 7,
		WILLARD_STATE_WALKATAK2 = 8,
		WILLARD_STATE_TURN_180 = 9,
		WILLARD_STATE_SHOOT = 10
	};

	enum WillardAnim
	{
		WILLARD_ANIM_STOP = 0,
		WILLARD_ANIM_KILL = 6,
		WILLARD_ANIM_STUN = 7
	};

	// Static data for AI path system.
	struct WillardData
	{
		Pose AIPath[MAX_PATH_POINTS];
		Pose AIJunction[MAX_JUNCTIONS];
		int JunctionIndex[MAX_JUNCTIONS];
		int PathCount = 0;
		int JunctionCount = 0;
		int ClosestAIPath = NO_AI_PATH;
		int LaraAIPath = NO_AI_PATH;
		int LaraJunction = NO_AI_PATH;
		int Direction = 1;
		int DesiredDirection = 1;
		bool MissingSetupLogged = false;
		bool InvalidStateLogged = false;
		bool Initialized = false;
	};

	static WillardData WillardAI;

	static void SpawnWillardPlasmaBall(ItemInfo* item, const CreatureBiteInfo& bite, short angleAdd)
	{
		int fxNumber = CreateNewEffect(item->RoomNumber);
		if (fxNumber == NO_VALUE)
			return;

		auto& fx = EffectList[fxNumber];
		fx.pos.Position = GetJointPosition(item, bite);
		fx.pos.Orientation.x = item->Pose.Orientation.x;
		fx.pos.Orientation.y = item->Pose.Orientation.y + angleAdd;
		fx.pos.Orientation.z = 0;
		fx.roomNumber = item->RoomNumber;
		fx.counter = 0;
		fx.flag1 = (short)MissileType::WillardPlasmaBall;
		fx.flag2 = 0;
		fx.speed = 16;
		fx.objectNumber = ID_ENERGY_BUBBLES;
		fx.frameNumber = Objects[fx.objectNumber].meshIndex;
	}

	static void InitializeWillardAI(ItemInfo* item)
	{
		if (WillardAI.Initialized)
			return;

		WillardAI.PathCount = 0;
		WillardAI.JunctionCount = 0;
		WillardAI.ClosestAIPath = NO_AI_PATH;
		WillardAI.LaraAIPath = NO_AI_PATH;
		WillardAI.LaraJunction = NO_AI_PATH;

		for (int i = 0; i < MAX_JUNCTIONS; i++)
			WillardAI.JunctionIndex[i] = NO_AI_PATH;

		// Find all AI_X1 (path) and AI_X2 (junction) objects in current room.
		for (const auto& aiObject : g_Level.AIObjects)
		{
			if (aiObject.roomNumber != item->RoomNumber)
				continue;

			if (aiObject.objectNumber == ID_AI_X1 && WillardAI.PathCount < MAX_PATH_POINTS)
			{
				WillardAI.AIPath[WillardAI.PathCount] = aiObject.pos;
				WillardAI.PathCount++;
			}
			else if (aiObject.objectNumber == ID_AI_X2 && WillardAI.JunctionCount < MAX_JUNCTIONS)
			{
				WillardAI.AIJunction[WillardAI.JunctionCount] = aiObject.pos;
				WillardAI.JunctionCount++;
			}
		}

		if (WillardAI.PathCount <= 0 || WillardAI.JunctionCount <= 0)
		{
			if (!WillardAI.MissingSetupLogged)
			{
				TENLog("Willard AI path markers (AI_X1/AI_X2) were not found in current room.", LogLevel::Warning);
				WillardAI.MissingSetupLogged = true;
			}

			WillardAI.Initialized = true;
			return;
		}

		// Find closest AI path point to Willard.
		int bestDistance = INT_MAX;

		for (int i = 0; i < WillardAI.PathCount; i++)
		{
			int x = (WillardAI.AIPath[i].Position.x - item->Pose.Position.x) >> 6;
			int z = (WillardAI.AIPath[i].Position.z - item->Pose.Position.z) >> 6;
			int distance = SQUARE(x) + SQUARE(z);

			if (distance < bestDistance)
			{
				WillardAI.ClosestAIPath = i;
				bestDistance = distance;
			}
		}

		// Find closest AI path point to Lara.
		bestDistance = INT_MAX;

		for (int i = 0; i < WillardAI.PathCount; i++)
		{
			int x = (WillardAI.AIPath[i].Position.x - LaraItem->Pose.Position.x) >> 6;
			int z = (WillardAI.AIPath[i].Position.z - LaraItem->Pose.Position.z) >> 6;
			int distance = SQUARE(x) + SQUARE(z);

			if (distance < bestDistance)
			{
				WillardAI.LaraAIPath = i;
				bestDistance = distance;
			}
		}

		// Find closest AI path point to each junction.
		for (int junc = 0; junc < WillardAI.JunctionCount; junc++)
		{
			int pathNum = NO_AI_PATH;
			bestDistance = INT_MAX;

			for (int i = 0; i < WillardAI.PathCount; i++)
			{
				int x = abs((WillardAI.AIPath[i].Position.x - WillardAI.AIJunction[junc].Position.x) >> 6);
				int z = abs((WillardAI.AIPath[i].Position.z - WillardAI.AIJunction[junc].Position.z) >> 6);
				int distance = (x > z) ? x + (z >> 1) : z + (x >> 1);

				if (distance < bestDistance)
				{
					pathNum = i;
					bestDistance = distance;
				}
			}

			WillardAI.JunctionIndex[junc] = pathNum;
		}

		WillardAI.Initialized = true;
	}

	static void UpdateAIPath(ItemInfo* item)
	{
		if (WillardAI.PathCount <= 0 || WillardAI.JunctionCount <= 0 ||
			WillardAI.ClosestAIPath == NO_AI_PATH || WillardAI.LaraAIPath == NO_AI_PATH)
		{
			if (!WillardAI.InvalidStateLogged)
			{
				TENLog("Willard AI path state is invalid; path tracking update was skipped.", LogLevel::Warning);
				WillardAI.InvalidStateLogged = true;
			}

			return;
		}

		// Update closest path point to Willard.
		int oldClosest = WillardAI.ClosestAIPath;
		int bestDistance = INT_MAX;

		for (int i = oldClosest - 1; i < oldClosest + 2; i++)
		{
			int pathNum;
			if (i < 0)
				pathNum = i + WillardAI.PathCount;
			else if (i > WillardAI.PathCount - 1)
				pathNum = i - WillardAI.PathCount;
			else
				pathNum = i;

			int x = (WillardAI.AIPath[pathNum].Position.x - item->Pose.Position.x) >> 6;
			int z = (WillardAI.AIPath[pathNum].Position.z - item->Pose.Position.z) >> 6;
			int distance = SQUARE(x) + SQUARE(z);

			if (distance < bestDistance)
			{
				WillardAI.ClosestAIPath = pathNum;
				bestDistance = distance;
			}
		}

		// Update closest path point to Lara.
		oldClosest = WillardAI.LaraAIPath;
		bestDistance = INT_MAX;

		for (int i = oldClosest - 1; i < oldClosest + 2; i++)
		{
			int pathNum;
			if (i < 0)
				pathNum = i + WillardAI.PathCount;
			else if (i > WillardAI.PathCount - 1)
				pathNum = i - WillardAI.PathCount;
			else
				pathNum = i;

			int x = (WillardAI.AIPath[pathNum].Position.x - LaraItem->Pose.Position.x) >> 6;
			int z = (WillardAI.AIPath[pathNum].Position.z - LaraItem->Pose.Position.z) >> 6;
			int distance = SQUARE(x) + SQUARE(z);

			if (distance < bestDistance)
			{
				WillardAI.LaraAIPath = pathNum;
				bestDistance = distance;
			}
		}

		// Find closest junction to Lara.
		int bestJunctionDistance = INT_MAX;
		for (int i = 0; i < WillardAI.JunctionCount; i++)
		{
			int x = (WillardAI.AIJunction[i].Position.x - LaraItem->Pose.Position.x) >> 6;
			int z = (WillardAI.AIJunction[i].Position.z - LaraItem->Pose.Position.z) >> 6;
			int distance = SQUARE(x) + SQUARE(z);

			if (distance < bestJunctionDistance)
			{
				WillardAI.LaraJunction = i;
				bestJunctionDistance = distance;
			}
		}
	}

	void InitializeWillard(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		InitializeCreature(itemNumber);

		WillardAI.PathCount = 0;
		WillardAI.JunctionCount = 0;
		WillardAI.ClosestAIPath = NO_AI_PATH;
		WillardAI.LaraAIPath = NO_AI_PATH;
		WillardAI.LaraJunction = NO_AI_PATH;
		WillardAI.Direction = 1;
		WillardAI.DesiredDirection = 1;
		WillardAI.MissingSetupLogged = false;
		WillardAI.InvalidStateLogged = false;
		WillardAI.Initialized = false;
		item.ItemFlags[1] = 0; // Death flag.
	}

	void WillardControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto& item = g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(&item);

		short angle = 0;
		bool laraAlive = LaraItem->HitPoints > 0;

		// Initialize AI path system on first run.
		InitializeWillardAI(&item);
		UpdateAIPath(&item);

		if (WillardAI.ClosestAIPath == NO_AI_PATH ||
			WillardAI.LaraAIPath == NO_AI_PATH ||
			WillardAI.LaraJunction == NO_AI_PATH)
		{
			if (!WillardAI.InvalidStateLogged)
			{
				TENLog("Willard AI path state is invalid; control fell back to default animation update.", LogLevel::Warning);
				WillardAI.InvalidStateLogged = true;
			}

			angle = CreatureTurn(&item, creature->MaxTurn);
			CreatureAnimation(itemNumber, angle, 0);
			return;
		}

		// Check if Lara is in fire zone (closer to junction than path).
		int x = (WillardAI.AIJunction[WillardAI.LaraJunction].Position.x - LaraItem->Pose.Position.x) >> 6;
		int z = (WillardAI.AIJunction[WillardAI.LaraJunction].Position.z - LaraItem->Pose.Position.z) >> 6;
		int laraToJunctionDist = SQUARE(x) + SQUARE(z);

		x = (WillardAI.AIPath[WillardAI.LaraAIPath].Position.x - LaraItem->Pose.Position.x) >> 6;
		z = (WillardAI.AIPath[WillardAI.LaraAIPath].Position.z - LaraItem->Pose.Position.z) >> 6;
		int laraToPathDist = SQUARE(x) + SQUARE(z);

		bool inFireZone = (laraToJunctionDist < laraToPathDist) ||
						  (item.Pose.Position.y > LaraItem->Pose.Position.y + BLOCK(2));

		x = WillardAI.AIJunction[WillardAI.LaraJunction].Position.x - item.Pose.Position.x;
		z = WillardAI.AIJunction[WillardAI.LaraJunction].Position.z - item.Pose.Position.z;
		int willardToJunctionDist = SQUARE(x) + SQUARE(z);

		if (item.HitPoints <= 0)
		{
			// Check if all 4 meteorite artifacts collected (PICKUP13-16) and death flag set.
			int artifactsCollected = 0;
			if (Lara.Inventory.Pickups[12]) artifactsCollected++; // PICKUP_ITEM13
			if (Lara.Inventory.Pickups[13]) artifactsCollected++; // PICKUP_ITEM14
			if (Lara.Inventory.Pickups[14]) artifactsCollected++; // PICKUP_ITEM15
			if (Lara.Inventory.Pickups[15]) artifactsCollected++; // PICKUP_ITEM16

			if (artifactsCollected != 4 || item.ItemFlags[1] == 0)
			{
				creature->MaxTurn = 0;

				switch (item.Animation.ActiveState)
				{
				case WILLARD_STATE_STOP:
					item.Animation.TargetState = WILLARD_STATE_STUNNED;
					break;

				case WILLARD_STATE_STUNNED:
					creature->Flags = WILLARD_KO_TIME;
					break;

				case WILLARD_STATE_KNOCKOUT:
					creature->Flags--;
					if (creature->Flags < 0)
						item.Animation.TargetState = WILLARD_STATE_GETUP;
					break;

				case WILLARD_STATE_GETUP:
					item.HitPoints = WILLARD_HP_AFTER_KO;
					if (artifactsCollected == 4)
						item.ItemFlags[1] = 1;
					creature->MaxTurn = WILLARD_ATTACK_TURN;
					break;

				default:
					item.Animation.TargetState = WILLARD_STATE_STOP;
					break;
				}
			}
			else
			{
				// Final death sequence.
				if (item.Animation.ActiveState != WILLARD_STATE_STUNNED)
				{
					SetAnimation(&item, WILLARD_ANIM_STUN);
				}
				else if (item.Animation.FrameNumber >= GetAnimData(item).EndFrameNumber - 2)
				{
					item.Animation.FrameNumber = GetAnimData(item).EndFrameNumber - 2;
					item.MeshBits.ClearAll();

					// TODO: Trigger explosion effects.

					CreatureDie(itemNumber, true);
					return;
				}
			}
		}
		else
		{
			AI_INFO ai;
			CreatureAIInfo(&item, &ai);

			// Touch damage.
			if (item.TouchBits.TestAny())
				DoDamage(LaraItem, WILLARD_TOUCH_DAMAGE);

			// Update direction based on Lara's position.
			int pathDiff = WillardAI.LaraAIPath - WillardAI.ClosestAIPath;

			if (WillardAI.Direction == -1 && ((pathDiff < 0 && pathDiff > -6) || pathDiff > 10))
				WillardAI.DesiredDirection = 1;
			else if (WillardAI.Direction == 1 && ((pathDiff > 0 && pathDiff < 6) || pathDiff < -10))
				WillardAI.DesiredDirection = -1;

			// Set target to path point.
			int pathAngle = WillardAI.AIPath[WillardAI.ClosestAIPath].Orientation.y;
			creature->Target.x = WillardAI.AIPath[WillardAI.ClosestAIPath].Position.x +
				(WillardAI.Direction * WILLARD_PATH_DISTANCE * phd_sin(pathAngle));
			creature->Target.z = WillardAI.AIPath[WillardAI.ClosestAIPath].Position.z +
				(WillardAI.Direction * WILLARD_PATH_DISTANCE * phd_cos(pathAngle));

			switch (item.Animation.ActiveState)
			{
			case WILLARD_STATE_STOP:
				creature->MaxTurn = 0;
				creature->Flags = 0;

				if (WillardAI.Direction != WillardAI.DesiredDirection)
				{
					item.Animation.TargetState = WILLARD_STATE_TURN_180;
				}
				else if (inFireZone && ai.ahead && willardToJunctionDist < WILLARD_FIRE_RANGE && laraAlive)
				{
					item.Animation.TargetState = WILLARD_STATE_SHOOT;
				}
				else if (ai.bite && ai.distance < WILLARD_LUNGE_RANGE)
				{
					item.Animation.TargetState = WILLARD_STATE_LUNGE;
				}
				else
				{
					item.Animation.TargetState = WILLARD_STATE_WALK;
				}
				break;

			case WILLARD_STATE_WALK:
				creature->MaxTurn = WILLARD_TURN;
				creature->Flags = 0;

				if (WillardAI.Direction != WillardAI.DesiredDirection)
				{
					item.Animation.TargetState = WILLARD_STATE_STOP;
				}
				else if (inFireZone && ai.ahead && willardToJunctionDist < WILLARD_FIRE_RANGE)
				{
					item.Animation.TargetState = WILLARD_STATE_STOP;
				}
				else if (ai.bite && ai.distance < WILLARD_ATTACK_RANGE)
				{
					if ((GetRandomControl() & 3) == 1)
					{
						item.Animation.TargetState = WILLARD_STATE_STOP;
					}
					else if (item.Animation.FrameNumber < 30)
					{
						item.Animation.TargetState = WILLARD_STATE_WALKATAK2;
					}
					else
					{
						item.Animation.TargetState = WILLARD_STATE_WALKATAK1;
					}
				}
				break;

			case WILLARD_STATE_TURN_180:
				creature->MaxTurn = 0;
				creature->Flags = 0;

				if (item.Animation.FrameNumber == 51)
				{
					item.Pose.Orientation.y += ANGLE(180.0f);
					WillardAI.Direction = -WillardAI.Direction;
				}
				break;

			case WILLARD_STATE_LUNGE:
				creature->Target.x = LaraItem->Pose.Position.x;
				creature->Target.z = LaraItem->Pose.Position.z;
				creature->MaxTurn = WILLARD_ATTACK_TURN;

				if (!creature->Flags && item.TouchBits.Test(WILLARD_TOUCH))
				{
					DoDamage(LaraItem, WILLARD_BITE_DAMAGE * 2);
					CreatureEffect(&item, WillardBiteLeft, DoBloodSplat);
					CreatureEffect(&item, WillardBiteRight, DoBloodSplat);
					creature->Flags = 1;
				}
				break;

			case WILLARD_STATE_WALKATAK1:
			case WILLARD_STATE_WALKATAK2:
				if (!creature->Flags && item.TouchBits.Test(WILLARD_TOUCH))
				{
					DoDamage(LaraItem, WILLARD_BITE_DAMAGE);
					CreatureEffect(&item, WillardBiteLeft, DoBloodSplat);
					CreatureEffect(&item, WillardBiteRight, DoBloodSplat);
					creature->Flags = 1;
				}

				if (inFireZone && ai.bite && willardToJunctionDist < WILLARD_FIRE_RANGE)
				{
					item.Animation.TargetState = WILLARD_STATE_WALK;
				}
				else if (ai.bite && ai.distance < WILLARD_ATTACK_RANGE)
				{
					if (item.Animation.ActiveState == WILLARD_STATE_WALKATAK1)
						item.Animation.TargetState = WILLARD_STATE_WALKATAK2;
					else
						item.Animation.TargetState = WILLARD_STATE_WALKATAK1;
				}
				else
				{
					item.Animation.TargetState = WILLARD_STATE_WALK;
				}
				break;

			case WILLARD_STATE_BIGKILL:
				// Blood splat timing.
				switch (item.Animation.FrameNumber)
				{
				case 0:
				case 43:
				case 95:
				case 105:
					CreatureEffect(&item, WillardBiteLeft, DoBloodSplat);
					break;
				case 61:
				case 91:
				case 101:
					CreatureEffect(&item, WillardBiteRight, DoBloodSplat);
					break;
				}
				break;

			case WILLARD_STATE_SHOOT:
				creature->Target.x = LaraItem->Pose.Position.x;
				creature->Target.z = LaraItem->Pose.Position.z;
				creature->MaxTurn = WILLARD_ATTACK_TURN;

				// Shoot plasma balls at frame 40.
				if (item.Animation.FrameNumber == 40 && laraAlive)
				{
					SpawnWillardPlasmaBall(&item, WillardBiteLeft, ANGLE(-11.25f));
					SpawnWillardPlasmaBall(&item, WillardBiteRight, ANGLE(11.25f));
				}
				break;
			}

			// Kill Lara animation.
			if (laraAlive && LaraItem->HitPoints <= 0)
			{
				CreatureKill(&item, WILLARD_ANIM_KILL, LEA_WILLARD_DEATH, WILLARD_STATE_BIGKILL, LS_DEATH);
				creature->MaxTurn = 0;
				return;
			}
		}

		angle = CreatureTurn(&item, creature->MaxTurn);
		CreatureAnimation(itemNumber, angle, 0);
	}

	void WillardPlasmaBallControl(short fxNumber)
	{
		ControlEnemyMissile(fxNumber);
	}
}
