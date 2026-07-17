#include "framework.h"
#include "Objects/TR3/Entity/Willard.h"

#include "Game/Animation/Animation.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/effects/item_fx.h"
#include "Game/effects/Light.h"
#include "Game/effects/tomb4fx.h"
#include "Objects/Effects/Boss.h"
#include "Game/items.h"
#include "Game/itemdata/creature_info.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "Specific/level.h"
#include "Specific/trutils.h"
#include "Objects/Effects/enemy_missile.h"


using namespace TEN::Animation;
using namespace TEN::Math;
using namespace TEN::Effects::Boss;
using namespace TEN::Effects::Light;
using namespace TEN::Entities::Effects;
using namespace TEN::Utils;

namespace TEN::Entities::Creatures::TR3
{
	constexpr auto WILLARD_BITE_DAMAGE				= 220;
	constexpr auto WILLARD_TOUCH_DAMAGE				= 10;
	constexpr auto WILLARD_HP_AFTER_KO				= 200;
	constexpr auto WILLARD_KO_TIME					= 280;
	constexpr auto WILLARD_PATH_DISTANCE			= 1024;

	constexpr auto WILLARD_ATTACK_RANGE				= SQUARE(BLOCK(1.5f));
	constexpr auto WILLARD_LUNGE_RANGE				= SQUARE(BLOCK(2));
	constexpr auto WILLARD_FIRE_RANGE				= SQUARE(BLOCK(4));

	constexpr auto WILLARD_TURN						= ANGLE(5.0f);
	constexpr auto WILLARD_ATTACK_TURN				= ANGLE(2.0f);
	constexpr auto WILLARD_TOUCH					= 0x900000;


	constexpr auto WILLARD_EXPLOSION_NUM_MAX		= 60;
	constexpr auto WILLARD_SHOCKWAVE_COLOR			= Vector4(0.0f, 0.7f, 0.3f, 0.5f);
	constexpr auto WILLARD_EXPLOSION_MAIN_COLOR		= Vector4(0.0f, 0.7f, 0.2f, 0.5f);
	constexpr auto WILLARD_EXPLOSION_SECOND_COLOR	= Vector4(0.0f, 0.7f, 0.0f, 0.5f);

	constexpr auto NO_AI_PATH = -1;
	constexpr auto MAX_PATH_POINTS = 16;
	constexpr auto MAX_JUNCTIONS = 4;

	const auto WillardBiteLeft						= CreatureBiteInfo(Vector3(19, -13, 3), 20);
	const auto WillardBiteRight						= CreatureBiteInfo(Vector3(19, -13, 3), 23);
	const auto WillardBiteAttackJoints				= std::vector<unsigned int>{ 20, 21, 22, 23 };

	const auto WillardBloodSplatFramesLeft  = std::vector<int>{ 0, 43, 95, 105 };
	const auto WillardBloodSplatFramesRight = std::vector<int>{ 61, 91, 101 };

	const auto WillardExplosionPlasmaBallFrames = std::vector<int>{ 1, 15, 25, 35, 45, 55 };

	constexpr auto WILLARD_HEAD_JOINT				 = 17;
	constexpr auto WILLARD_SHOOT_CHARGE_INTENSITY_MAX = 16;

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

	static void SpawnWillardPlasma(int itemNumber, int nodeID, int size)
	{
		auto& plasma = *GetFreeParticle();

		plasma.on = true;
		plasma.sR = 48;
		plasma.sG = 255;
		plasma.sB = 48 + (GetRandomControl() & 31);

		plasma.dR = 32;
		plasma.dG = 192 + (GetRandomControl() & 63);
		plasma.dB = 128 + (GetRandomControl() & 63);

		plasma.colFadeSpeed = 12 + (GetRandomControl() & 3);
		plasma.fadeToBlack = 8;
		plasma.sLife =
		plasma.life = (GetRandomControl() & 7) + 24;

		plasma.blendMode = BlendMode::Additive;

		plasma.extras = 0;
		plasma.dynamic = -1;

		plasma.x = ((GetRandomControl() & 15) - 8);
		plasma.y = 0;
		plasma.z = ((GetRandomControl() & 15) - 8);

		plasma.xVel = ((GetRandomControl() & 31) - 16);
		plasma.yVel = (GetRandomControl() & 7) + 8;
		plasma.zVel = ((GetRandomControl() & 31) - 16);
		plasma.friction = 3;

		if (Random::TestProbability(1 / 2.0f))
		{
			plasma.flags = SP_SCALE | SP_DEF | SP_ROTATE | SP_EXPDEF | SP_ITEM | SP_NODEATTACH;
			plasma.rotAng = GetRandomControl() & 4095;

			if (Random::TestProbability(1 / 2.0f))
			{
				plasma.rotAdd = -(GetRandomControl() & 15) - 16;
			}
			else
			{
				plasma.rotAdd = (GetRandomControl() & 15) + 16;
			}
		}
		else
		{
			plasma.flags = SP_SCALE | SP_DEF | SP_EXPDEF | SP_ITEM | SP_NODEATTACH;
		}

		plasma.gravity = (GetRandomControl() & 7) + 8;
		plasma.maxYvel = (GetRandomControl() & 7) + 16;

		plasma.fxObj = itemNumber;
		plasma.nodeNumber = nodeID;

		plasma.SpriteSeqID = ID_DEFAULT_SPRITES;
		plasma.SpriteID = 0;
		plasma.scalar = 1;
		size += GetRandomControl() & 15;
		plasma.size =
		plasma.sSize = size;
		plasma.dSize = size / 4;
	}

	static void SpawnWillardShootChargeEffect(ItemInfo& item)
	{
		// Intensity ramps up at animation start and down near its end.
		int intensity = item.Animation.FrameNumber;
		if (intensity > WILLARD_SHOOT_CHARGE_INTENSITY_MAX)
		{
			intensity = GetAnimData(item).EndFrameNumber - item.Animation.FrameNumber;
			if (intensity > WILLARD_SHOOT_CHARGE_INTENSITY_MAX)
				intensity = WILLARD_SHOOT_CHARGE_INTENSITY_MAX;
		}

		auto pos = GetJointPosition(&item, WILLARD_HEAD_JOINT);
		int random = GetRandomControl();
		unsigned char r = (intensity * (random & 0x3F)) >> 4;
		unsigned char g = (intensity * (255 - ((random >> 4) & 0x1F))) >> 4;
		unsigned char b = (intensity * (192 - ((random >> 6) & 0x1F))) >> 4;
		SpawnDynamicLight(pos.x, pos.y, pos.z, 12, r, g, b);

		SpawnWillardPlasma(item.Index, ParticleNodeOffsetIDs::NodeWillardBossLeftPlasma, intensity << 2);
		SpawnWillardPlasma(item.Index, ParticleNodeOffsetIDs::NodeWillardBossRightPlasma, intensity << 2);
	}

	static void SpawnWillardPlasmaBall(ItemInfo* item, const CreatureBiteInfo& bite, short angleAdd)
	{
		auto pose = Pose(
			GetJointPosition(item, bite),
			EulerAngles(item->Pose.Orientation.x, item->Pose.Orientation.y + angleAdd, 0));

		int fxNumber = CreateNewEffect(item->RoomNumber, ID_ENERGY_BUBBLES, pose);
		if (fxNumber == NO_VALUE)
			return;

		auto& fx = g_Level.Items[fxNumber];
		auto& fxInfo = GetFXInfo(fx);

		fxInfo.Counter = 0;
		fxInfo.Flag1 = (short)MissileType::WillardPlasmaBall;
		fxInfo.Flag2 = 0;
		fx.Animation.Velocity.z = 16;
		fx.Model.MeshIndex = { (int)Objects[fx.ObjectNumber].meshIndex };
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
				TENLog("Willard AI setup is incomplete in current room. Paths=" + std::to_string(WillardAI.PathCount) +
					", Junctions=" + std::to_string(WillardAI.JunctionCount) + ".", LogLevel::Warning);
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
				TENLog("Willard AI path state invalid in UpdateAIPath. PathCount=" + std::to_string(WillardAI.PathCount) +
					", JunctionCount=" + std::to_string(WillardAI.JunctionCount) +
					", ClosestAIPath=" + std::to_string(WillardAI.ClosestAIPath) +
					", LaraAIPath=" + std::to_string(WillardAI.LaraAIPath) + ".", LogLevel::Warning);
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

	static int GetPathDelta(int fromPath, int toPath, int pathCount)
	{
		if (pathCount <= 0)
			return 0;

		int delta = toPath - fromPath;
		int halfCount = pathCount / 2;

		if (delta > halfCount)
			delta -= pathCount;
		else if (delta < -halfCount)
			delta += pathCount;

		return delta;
	}

	void InitializeWillard(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		InitializeCreature(itemNumber);
		CheckForRequiredObjects(item);

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
		item.ItemFlags[7] = 0;	// Explode count.
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
				TENLog("Willard AI path state invalid in WillardControl. ClosestAIPath=" + std::to_string(WillardAI.ClosestAIPath) +
					", LaraAIPath=" + std::to_string(WillardAI.LaraAIPath) +
					", LaraJunction=" + std::to_string(WillardAI.LaraJunction) + ".", LogLevel::Warning);
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

		int junctionPath = NO_AI_PATH;
		bool validJunctionPath = (WillardAI.LaraJunction >= 0 && WillardAI.LaraJunction < WillardAI.JunctionCount);
		if (validJunctionPath)
		{
			junctionPath = WillardAI.JunctionIndex[WillardAI.LaraJunction];
			validJunctionPath = (junctionPath >= 0 && junctionPath < WillardAI.PathCount);
		}

		bool laraAtJunctionPath = false;
		if (validJunctionPath)
			laraAtJunctionPath = abs(GetPathDelta(WillardAI.LaraAIPath, junctionPath, WillardAI.PathCount)) <= 1;

		bool inFireZone = validJunctionPath && laraAtJunctionPath &&
						  ((laraToJunctionDist < laraToPathDist) ||
						   (item.Pose.Position.y > LaraItem->Pose.Position.y + BLOCK(2)));

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

					if (item.ItemFlags[7] < 128)
						item.ItemFlags[7]++;

					// Spray plasma balls from body joints at explosion ticks.
					if (Contains(WillardExplosionPlasmaBallFrames, (int)item.ItemFlags[7]))
					{
						for (int jointIndex = 0; jointIndex < 24; jointIndex += 3)
						{
							auto pos = GetJointPosition(&item, jointIndex);
							SpawnWillardScatterPlasmaBall(pos, item.RoomNumber, (short)(GetRandomControl() << 1), 4);
						}
					}

					ExplodeBoss(item, WILLARD_EXPLOSION_NUM_MAX, WILLARD_SHOCKWAVE_COLOR, WILLARD_EXPLOSION_MAIN_COLOR, WILLARD_EXPLOSION_SECOND_COLOR);
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
				else if (inFireZone && willardToJunctionDist < WILLARD_FIRE_RANGE)
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
				if (Contains(WillardBloodSplatFramesLeft, item.Animation.FrameNumber))
					CreatureEffect(&item, WillardBiteLeft, DoBloodSplat);
				else if (Contains(WillardBloodSplatFramesRight, item.Animation.FrameNumber))
					CreatureEffect(&item, WillardBiteRight, DoBloodSplat);
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

				SpawnWillardShootChargeEffect(item);
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
