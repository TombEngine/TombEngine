#include "framework.h"
#include "Objects/TR3/Entity/Shiva.h"

#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/control/box.h"
#include "Game/effects/effects.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Renderer/RendererEnums.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;
using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR3
{
	constexpr auto SHIVA_DOWNWARD_ATTACK_DAMAGE = 180;
	constexpr auto SHIVA_GRAB_ATTACK_DAMAGE		= 150;

	constexpr auto SHIVA_DOWNWARD_ATTACK_RANGE = SQUARE(BLOCK(4) / 3);
	constexpr auto SHIVA_GRAB_ATTACK_RANGE	   = SQUARE(BLOCK(1.25f));

	constexpr auto SHIVA_WALK_TURN_RATE_MAX	  = ANGLE(4.0f);
	constexpr auto SHIVA_ATTACK_TURN_RATE_MAX = ANGLE(4.0f);

	constexpr auto SHIVA_SWAPMESH_TIME = 3;

	const auto ShivaBiteLeft  = CreatureBiteInfo(Vector3(0, 0, 920), 13);
	const auto ShivaBiteRight = CreatureBiteInfo(Vector3(0, 0, 920), 22);
	const auto ShivaAttackLeftJoints  = std::vector<unsigned int>{ 10, 13 };
	const auto ShivaAttackRightJoints = std::vector<unsigned int>{ 22, 25 };

	enum ShivaState
	{
		SHIVA_STATE_IDLE = 0,
		SHIVA_STATE_WALK_FORWARD = 1,
		SHIVA_STATE_GUARD_IDLE = 2,
		SHIVA_STATE_WALK_FORWARD_GUARDING = 3,
		SHIVA_STATE_INACTIVE = 4,
		SHIVA_STATE_GRAB_ATTACK = 5,
		SHIVA_STATE_KILL = 6,
		SHIVA_STATE_DOWNWARD_ATTACK = 7,
		SHIVA_STATE_WALK_BACK = 8,
		SHIVA_STATE_DEATH = 9
	};

	enum ShivaAnim
	{
		SHIVA_ANIM_IDLE = 0,
		SHIVA_ANIM_IDLE_TO_WALK_RIGHT = 1,
		SHIVA_ANIM_WALK_TO_IDLE_RIGHT = 2,
		SHIVA_ANIM_WALK_TO_IDLE_LEFT = 3,
		SHIVA_ANIM_IDLE_TO_GUARD_IDLE = 4,
		SHIVA_ANIM_GUARD_IDLE = 5,
		SHIVA_ANIM_GUARD_IDLE_TO_IDLE = 6,
		SHIVA_ANIM_WALK_FORWARD = 7,
		SHIVA_ANIM_WALK_FORWARD_GUARDED = 8,
		SHIVA_ANIM_WALK_FORWARD_TO_GUARDED_RIGHT_3 = 9,
		SHIVA_ANIM_WALK_FORWARD_TO_GUARDED_LEFT_1 = 10,
		SHIVA_ANIM_WALK_FORWARD_TO_GUARDED_LEFT_2 = 11,
		SHIVA_ANIM_WALK_FORWARD_TO_GUARDED_RIGHT_1 = 12,
		SHIVA_ANIM_WALK_FORWARD_TO_GUARDED_RIGHT_2 = 13,
		SHIVA_ANIM_INACTIVE = 14,
		SHIVA_ANIM_WALK_FORWARD_UNGUARD_RIGHT = 15,
		SHIVA_ANIM_WALK_FORWARD_UNGUARD_LEFT = 16,
		SHIVA_ANIM_GRAB_ATTACK = 17,
		SHIVA_ANIM_KILL = 18,
		SHIVA_ANIM_DOWNWARD_ATTACK = 19,
		SHIVA_ANIM_WALK_BACK_LEFT = 20,
		SHIVA_ANIM_WALK_BACK_RIGHT = 21,
		SHIVA_ANIM_DEATH = 22
	};

	static void ShivaDamage(ItemInfo& item, int damage)
	{
		auto& creature = *GetCreatureInfo(&item);

		if (!creature.Flags && item.TouchBits.Test(ShivaAttackRightJoints))
		{
			DoDamage(creature.Enemy, damage);
			CreatureEffect(&item, ShivaBiteRight, DoBloodSplat);
			SoundEffect(SFX_TR2_CRUNCH2, &item.Pose);
			creature.Flags = 1;
		}

		if (!creature.Flags && item.TouchBits.Test(ShivaAttackLeftJoints))
		{
			DoDamage(creature.Enemy, damage);
			CreatureEffect(&item, ShivaBiteLeft, DoBloodSplat);
			SoundEffect(SFX_TR2_CRUNCH2, &item.Pose);
			creature.Flags = 1;
		}
	}

	static void SpawnShivaSmoke(const Vector3& pos, int roomNumber)
	{
		auto& smoke = *GetFreeParticle();

		bool isUnderwater = TestEnvironment(ENV_FLAG_WATER, Vector3i(pos), roomNumber);
		auto sphere = BoundingSphere(pos, 16);
		auto effectPos = Random::GeneratePointInSphere(sphere);

		smoke.on = true;
		smoke.blendMode = BlendMode::Additive;

		smoke.x = effectPos.x;
		smoke.y = effectPos.y;
		smoke.z = effectPos.z;
		smoke.xVel = Random::GenerateInt(-BLOCK(0.5f), BLOCK(0.5f));
		smoke.yVel = Random::GenerateInt(-BLOCK(1 / 8.0f), BLOCK(1 / 8.0f));
		smoke.zVel = Random::GenerateInt(-BLOCK(0.5f), BLOCK(0.5f));

		if (isUnderwater)
		{
			smoke.sR = 144;
			smoke.sG = 144;
			smoke.sB = 144;
			smoke.dR = 64;
			smoke.dG = 64;
			smoke.dB = 64;
		}
		else
		{
			smoke.sR = 0;
			smoke.sG = 0;
			smoke.sB = 0;
			smoke.dR = 192;
			smoke.dG = 192;
			smoke.dB = 208;
		}

		smoke.colFadeSpeed = 8;
		smoke.fadeToBlack = 64;
		smoke.sLife =
		smoke.life = Random::GenerateInt(96, 128);
		smoke.extras = 0;
		smoke.dynamic = -1;

		if (isUnderwater)
		{
			smoke.yVel /= 16;
			smoke.y += 32;
			smoke.friction = 4 | 16;
		}
		else
		{
			smoke.friction = 6;
		}

		smoke.rotAng = Random::GenerateAngle();
		smoke.rotAdd = Random::GenerateAngle(ANGLE(-0.2f), ANGLE(0.2f));

		smoke.scalar = 3;

		if (isUnderwater)
		{
			smoke.gravity = 0;
			smoke.maxYvel = 0;
		}
		else
		{
			smoke.gravity = Random::GenerateInt(-8, -4);
			smoke.maxYvel = Random::GenerateInt(-8, -4);
		}

		int scale = Random::GenerateInt(128, 160);
		smoke.size =
		smoke.sSize = scale / 4;
		smoke.dSize = scale;

		scale += Random::GenerateInt(32, 64);
		smoke.size =
		smoke.sSize = scale / 8;
		smoke.dSize = scale;
		smoke.flags = SP_SCALE | SP_DEF | SP_ROTATE | SP_EXPDEF;
	}

	static void SwapShivaMeshToStone(ItemInfo& item, int jointIndex, bool spawnSmoke = true)
	{
		if (spawnSmoke)
			SpawnShivaSmoke(GetJointPosition(&item, jointIndex).ToVector3(), item.RoomNumber);

		item.Model.MeshIndex[jointIndex] = Objects[ID_SHIVA_STATUE].meshIndex + jointIndex;
	}

	static void SwapShivaMeshToNormal(ItemInfo& item, int jointIndex, bool spawnSmoke = true)
	{
		if (spawnSmoke)
			SpawnShivaSmoke(GetJointPosition(&item, jointIndex).ToVector3(), item.RoomNumber);

		item.Model.MeshIndex[jointIndex] = Objects[ID_SHIVA].meshIndex + jointIndex;
	}

	static bool DoShivaMeshSwap(ItemInfo& item, bool isDead)
	{
		const auto& object = Objects[item.ObjectNumber];
		auto& creature = *GetCreatureInfo(&item);

		// Do mesh swaps.
		if (creature.Flags == 0)
		{
			switch (item.ItemFlags[3])
			{
			// Stone to normal.
			case 0:
				SwapShivaMeshToNormal(item, item.ItemFlags[0]);
				item.ItemFlags[0]++;
				if (item.ItemFlags[0] >= object.nmeshes)
				{
					item.ItemFlags[0] = 0;
					item.ItemFlags[1] = 0;
					item.ItemFlags[3] = 0;
					return true;
				}

				break;

			// Normal to stone.
			case 1:
				SwapShivaMeshToStone(item, item.ItemFlags[0]);
				item.ItemFlags[0]--;

				if (item.ItemFlags[0] < 0)
				{
					item.ItemFlags[0] = 0;
					item.ItemFlags[1] = 1;
					item.ItemFlags[3] = 1;
					return true;
				}

				break;
			}

			creature.Flags = SHIVA_SWAPMESH_TIME;
		}
		else
		{
			creature.Flags--;
		}

		return false;
	}

	void InitializeShiva(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		item.Status = ITEM_NOT_ACTIVE; // Draw the statue from the start.

		InitializeCreature(itemNumber);
		SetAnimation(item, SHIVA_ANIM_INACTIVE);
		item.ItemFlags[0] = 0; // Joint index when swapping mesh.
		item.ItemFlags[1] = 1; // Immune state. true = immune to damage.
		item.ItemFlags[2] = 1; // If 1, swap to stone. If 2, swap to normal.
		item.ItemFlags[3] = 0; // If mesh is swapped to stone, then it's true. Otherwise false.

		const auto& object = Objects[item.ObjectNumber];
		for (int jointIndex = 0; jointIndex < object.nmeshes; jointIndex++)
			SwapShivaMeshToStone(item, jointIndex, false);
	}

	void ShivaControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		const auto& object = Objects[item->ObjectNumber];
		auto& creature = *GetCreatureInfo(item);

		short headingAngle = 0;
		short tiltAngle = 0;
		auto extraHeadRot = EulerAngles::Identity;
		auto extraTorsoRot = EulerAngles::Identity;

		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState != SHIVA_STATE_DEATH)
			{
				SetAnimation(*item, SHIVA_ANIM_DEATH);
				item->ItemFlags[0] = object.nmeshes - 1;
				item->ItemFlags[2] = 1; // Do mesh swap to stone.
				item->ItemFlags[3] = 1;
			}

			if (TestLastFrame(*item))
			{
				// Block last frame until mesh is swapped.
				item->Animation.FrameNumber -= 1;

				if (DoShivaMeshSwap(*item, true))
					CreatureDie(itemNumber, false);
			}
		}
		else
		{
			AI_INFO ai;
			CreatureAIInfo(item, &ai);

			GetCreatureMood(item, &ai, true);
			CreatureMood(item, &ai, true);

			// Shiva isn't fearful.
			if (creature.Mood == MoodType::Escape)
			{
				creature.Target.x = creature.Enemy->Pose.Position.x;
				creature.Target.z = creature.Enemy->Pose.Position.z;
			}

			bool isPlayerAlive = (creature.Enemy->HitPoints > 0);
			headingAngle = CreatureTurn(item, creature.MaxTurn);

			switch (item->Animation.ActiveState)
			{
			case SHIVA_STATE_INACTIVE:
				creature.MaxTurn = 0;

				if (DoShivaMeshSwap(*item, false))
					item->Animation.TargetState = SHIVA_STATE_IDLE;

				break;

			case SHIVA_STATE_IDLE:
				creature.MaxTurn = 0;
				creature.Flags = 0;

				if (ai.ahead)
					extraHeadRot.y = ai.angle;

				if (creature.Mood == MoodType::Escape)
				{
					int x = item->Pose.Position.x + BLOCK(1) * phd_sin(item->Pose.Orientation.y + ANGLE(180.0f));
					int z = item->Pose.Position.z + BLOCK(1) * phd_cos(item->Pose.Orientation.y + ANGLE(180.0f));
					auto box = GetPointCollision(Vector3i(x, item->Pose.Position.y, z), item->RoomNumber).GetBottomSector().PathfindingBoxID;

					if (box != NO_VALUE && !(g_Level.PathfindingBoxes[box].flags & BLOCKABLE) && !creature.Flags)
						item->Animation.TargetState = SHIVA_STATE_WALK_BACK;
					else
						item->Animation.TargetState = SHIVA_STATE_GUARD_IDLE;
				}
				else if (creature.Mood == MoodType::Bored)
				{
					if (Random::TestProbability(1 / 32.0f))
						item->Animation.TargetState = SHIVA_STATE_WALK_FORWARD;
				}
				else if (ai.bite && ai.distance < SHIVA_GRAB_ATTACK_RANGE)
				{
					item->Animation.TargetState = SHIVA_STATE_GRAB_ATTACK;
					creature.Flags = 0;
				}
				else if (ai.bite && ai.distance < SHIVA_DOWNWARD_ATTACK_RANGE)
				{
					item->Animation.TargetState = SHIVA_STATE_DOWNWARD_ATTACK;
					creature.Flags = 0;
				}
				else if (item->HitStatus && ai.ahead)
				{
					item->Animation.TargetState = SHIVA_STATE_GUARD_IDLE;
					creature.Flags = 4;
				}
				else
				{
					item->Animation.TargetState = SHIVA_STATE_WALK_FORWARD;
				}

				break;

			case SHIVA_STATE_GUARD_IDLE:
				creature.MaxTurn = 0;

				if (ai.ahead)
					extraHeadRot.y = ai.angle;

				if (item->HitStatus || creature.Mood == MoodType::Escape)
					creature.Flags = 4;

				if (ai.bite && ai.distance < SHIVA_DOWNWARD_ATTACK_RANGE ||
				   (item->Animation.FrameNumber == 0 && !creature.Flags) ||
				   !ai.ahead)
				{
					item->Animation.TargetState = SHIVA_STATE_IDLE;
					creature.Flags = 0;
				}
				else if (creature.Flags)
				{
					item->Animation.TargetState = SHIVA_STATE_GUARD_IDLE;
				}

				if (item->Animation.FrameNumber == 0 && creature.Flags > 1)
					creature.Flags -= 2;

				break;

			case SHIVA_STATE_WALK_FORWARD:
				creature.MaxTurn = SHIVA_WALK_TURN_RATE_MAX;

				if (ai.ahead)
					extraHeadRot.y = ai.angle;

				if (creature.Mood == MoodType::Escape)
				{
					item->Animation.TargetState = SHIVA_STATE_IDLE;
				}
				else if (creature.Mood == MoodType::Bored)
				{
					item->Animation.TargetState = SHIVA_STATE_IDLE;
				}
				else if (ai.bite && ai.distance < SHIVA_GRAB_ATTACK_RANGE)
				{
					item->Animation.TargetState = SHIVA_STATE_IDLE;
					creature.Flags = 0;
				}
				else if (item->HitStatus)
				{
					item->Animation.TargetState = SHIVA_STATE_WALK_FORWARD_GUARDING;
					creature.Flags = 4;
				}

				break;

			case SHIVA_STATE_WALK_FORWARD_GUARDING:
				creature.MaxTurn = SHIVA_WALK_TURN_RATE_MAX;

				if (ai.ahead)
					extraHeadRot.y = ai.angle;

				if (item->HitStatus)
					creature.Flags = 4;

				if ((ai.bite && ai.distance < SHIVA_DOWNWARD_ATTACK_RANGE) ||
					(item->Animation.FrameNumber == 0 && !creature.Flags))
				{
					item->Animation.TargetState = SHIVA_STATE_WALK_FORWARD;
					creature.Flags = 0;
				}
				else if (creature.Flags)
				{
					item->Animation.TargetState = SHIVA_STATE_WALK_FORWARD_GUARDING;
				}

				if (item->Animation.FrameNumber == 0)
					creature.Flags = 0;

				break;

			case SHIVA_STATE_WALK_BACK:
				creature.MaxTurn = SHIVA_WALK_TURN_RATE_MAX;

				if (ai.ahead)
					extraHeadRot.y = ai.angle;

				if ((ai.ahead && ai.distance < SHIVA_DOWNWARD_ATTACK_RANGE) ||
					(item->Animation.FrameNumber == 0 && !creature.Flags))
				{
					item->Animation.TargetState = SHIVA_STATE_IDLE;
				}
				else if (item->HitStatus)
				{
					item->Animation.TargetState = SHIVA_STATE_IDLE;
					creature.Flags = 4;
				}

				break;

			case SHIVA_STATE_GRAB_ATTACK:
				creature.MaxTurn = SHIVA_ATTACK_TURN_RATE_MAX;

				if (ai.ahead)
				{
					extraHeadRot.y = ai.angle;
					extraTorsoRot = EulerAngles(ai.xAngle, ai.angle, 0);
				}

				ShivaDamage(*item, SHIVA_GRAB_ATTACK_DAMAGE);
				break;

			case SHIVA_STATE_DOWNWARD_ATTACK:
				creature.MaxTurn = SHIVA_ATTACK_TURN_RATE_MAX;
				extraHeadRot.y = ai.angle;
				extraTorsoRot.y = ai.angle;

				if (ai.xAngle > 0)
					extraTorsoRot.x = ai.xAngle;

				ShivaDamage(*item, SHIVA_DOWNWARD_ATTACK_DAMAGE);
				break;

			case SHIVA_STATE_KILL:
				creature.MaxTurn = 0;
				extraHeadRot = EulerAngles::Identity;
				extraTorsoRot = EulerAngles::Identity;

				if (item->Animation.FrameNumber == 10 ||
					item->Animation.FrameNumber == 21 ||
					item->Animation.FrameNumber == 33)
				{
					CreatureEffect(item, ShivaBiteRight, DoBloodSplat);
					CreatureEffect(item, ShivaBiteLeft, DoBloodSplat);
				}

				break;
			}

			// Dispatch kill animation.
			if (isPlayerAlive && LaraItem->HitPoints <= 0)
			{
				CreatureKill(item, SHIVA_ANIM_KILL, LEA_SHIVA_DEATH, SHIVA_STATE_KILL, LS_DEATH);
				return;
			}
		}

		CreatureTilt(item, tiltAngle);
		extraHeadRot.y -= extraTorsoRot.y;

		CreatureJoint(item, 0, extraTorsoRot.y);
		CreatureJoint(item, 1, extraTorsoRot.x);
		CreatureJoint(item, 2, extraHeadRot.y);
		CreatureJoint(item, 3, extraHeadRot.x);
		CreatureAnimation(itemNumber, headingAngle, tiltAngle);
	}

	void ShivaHit(ItemInfo& target, ItemInfo& source, std::optional<GameVector> pos, int damage, bool isExplosive, int jointIndex)
	{
		if (pos.has_value())
		{
			// If immune, ricochet without damage.
			if (target.ItemFlags[1] != 0)
			{
				SoundEffect(SFX_TR4_WEAPON_RICOCHET, &target.Pose);
				TriggerRicochetSpark(*pos, source.Pose.Orientation.y, false);
				return;
			}

			// If guarded, ricochet without damage.
			if (target.Animation.ActiveState == SHIVA_STATE_WALK_FORWARD_GUARDING ||
				target.Animation.ActiveState == SHIVA_STATE_GUARD_IDLE)
			{
				SoundEffect(SFX_TR4_BADDY_SWORD_RICOCHET, &target.Pose);
				TriggerRicochetSpark(*pos, source.Pose.Orientation.y, false);
				return;
			}
		}

		DefaultItemHit(target, source, pos, damage, isExplosive, jointIndex);
	}
}
