#include "framework.h"
#include "Game/control/box.h"

#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/control/control.h"
#include "Game/control/lot.h"
#include "Game/effects/smoke.h"
#include "Game/effects/tomb4fx.h"
#include "Game/itemdata/creature_info.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/items.h"
#include "Game/misc.h"
#include "Game/pickup/pickup.h"
#include "Game/room.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Objects/objectslist.h"
#include "Objects/Generic/Object/Pushable/PushableObject.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;
using namespace TEN::Collision::Room;
using namespace TEN::Effects::Smoke;

constexpr auto ESCAPE_DIST = BLOCK(5);
constexpr auto STALK_DIST = BLOCK(3);
constexpr auto REACHED_GOAL_RADIUS = BLOCK(0.625);
constexpr auto ATTACK_RANGE = SQUARE(BLOCK(3));
constexpr auto ESCAPE_CHANCE = 0x800;
constexpr auto RECOVER_CHANCE = 0x100;
constexpr auto BIFF_AVOID_TURN = ANGLE(11.25f);
constexpr auto CREATURE_AI_ROTATION_MAX = ANGLE(90.0f);
constexpr auto CREATURE_JOINT_ROTATION_MAX = ANGLE(70.0f);

constexpr auto CREATURE_GUN_EFFECT_VERTICAL_OFFSET = 75;

#ifdef CREATURE_AI_PRIORITY_OPTIMIZATION
constexpr int HIGH_PRIO_RANGE = 8;
constexpr int MEDIUM_PRIO_RANGE = HIGH_PRIO_RANGE + HIGH_PRIO_RANGE * (HIGH_PRIO_RANGE / 6.0f);
constexpr int LOW_PRIO_RANGE = MEDIUM_PRIO_RANGE + MEDIUM_PRIO_RANGE * (MEDIUM_PRIO_RANGE / 24.0f);
constexpr int NONE_PRIO_RANGE = LOW_PRIO_RANGE + LOW_PRIO_RANGE * (LOW_PRIO_RANGE / 32.0f);
constexpr auto FRAME_PRIO_BASE = 4;
constexpr auto FRAME_PRIO_EXP = 1.5;
#endif // CREATURE_AI_PRIORITY_OPTIMIZATION

void DrawBox(int boxIndex, Vector3 color)
{
	if (boxIndex == NO_VALUE)
		return;

	auto& currBox = g_Level.PathfindingBoxes[boxIndex];

	float x = ((float)currBox.left + (float)(currBox.right - currBox.left) / 2.0f) * 1024.0f;
	auto  y = currBox.height - CLICK(1);
	float z = ((float)currBox.top + (float)(currBox.bottom - currBox.top) / 2.0f) * 1024.0f;

	auto center = Vector3(z, y, x);
	auto corner = Vector3(currBox.bottom * BLOCK(1), currBox.height + CLICK(1), currBox.right * BLOCK(1));
	auto extents = (corner - center) * 0.9f;
	auto dBox = BoundingOrientedBox(center, extents, Vector4::UnitY);

	for (int i = 0; i <= 10; i++)
	{
		dBox.Extents = extents + Vector3(i);
		DrawDebugBox(dBox, Vector4(color.x, color.y, color.z, 1), RendererDebugPage::PathfindingStats);
	}
}

void DrawNearbyPathfinding(int boxIndex)
{
	if (boxIndex == NO_VALUE)
		return;

	auto& currBox = g_Level.PathfindingBoxes[boxIndex];
	auto index = currBox.overlapIndex;

	// Grey flag box.
	auto currentBoxColor = Vector3(0.0f, 1.0f, 1.0f);
	if (currBox.flags & BLOCKABLE)
		currentBoxColor = (currBox.flags & BLOCKED) ? Vector3(1.0f, 0.0f, 0.0f) : Vector3(0.0f, 1.0f, 0.0f);

	DrawBox(boxIndex, currentBoxColor);

	while (true)
	{
		if (index >= g_Level.Overlaps.size())
			break;

		auto overlap = g_Level.Overlaps[index];

		DrawBox(overlap.box, Vector3(1, 1, 0));

		if (overlap.flags & BOX_END_BIT)
			break;
		else
			index++;
	}
}

bool MoveCreature3DPos(Pose* fromPose, Pose* toPose, int velocity, short angleDif, int angleAdd)
{
	auto differenceVector = toPose->Position - fromPose->Position;
	float distance = Vector3::Distance(fromPose->Position.ToVector3(), toPose->Position.ToVector3());

	if (velocity < distance)
		fromPose->Position += differenceVector * (velocity / distance);
	else
		fromPose->Position = toPose->Position;

	if (angleDif <= angleAdd)
	{
		if (angleDif >= -angleAdd)
			fromPose->Orientation.y = toPose->Orientation.y;
		else
			fromPose->Orientation.y -= angleAdd;
	}
	else
		fromPose->Orientation.y += angleAdd;

	if (fromPose->Position == toPose->Position &&
		fromPose->Orientation.y == toPose->Orientation.y)
	{
		return true;
	}

	return false;
}

void CreatureYRot2(Pose* fromPose, short angle, short angleAdd)
{
	if (angle > angleAdd)
	{
		fromPose->Orientation.y += angleAdd;
	}
	else if (angle < -angleAdd)
	{
		fromPose->Orientation.y -= angleAdd;
	}
	else
	{
		fromPose->Orientation.y += angle;
	}
}

bool SameZone(CreatureInfo* creature, ItemInfo* target)
{
	auto& item = g_Level.Items[creature->ItemNumber];
	auto* zone = g_Level.Zones[(int)creature->LOT.Zone][(int)FlipStatus].data();

	auto& roomSource = g_Level.Rooms[item.RoomNumber];
	auto& boxSource = GetSector(&roomSource, item.Pose.Position.x - roomSource.Position.x, item.Pose.Position.z - roomSource.Position.z)->PathfindingBoxID;
	if (boxSource == NO_VALUE)
		return false;
	item.BoxNumber = boxSource;

	auto& roomTarget = g_Level.Rooms[target->RoomNumber];
	auto& boxTarget = GetSector(&roomTarget, target->Pose.Position.x - roomTarget.Position.x, target->Pose.Position.z - roomTarget.Position.z)->PathfindingBoxID;
	if (boxTarget == NO_VALUE)
		return false;
	target->BoxNumber = boxTarget;

	return (zone[item.BoxNumber] == zone[target->BoxNumber]);
}

short AIGuard(CreatureInfo* creature) 
{
	auto& item = g_Level.Items[creature->ItemNumber];
	if (item.AIBits & MODIFY)
		return 0;

	if (Random::TestProbability(1.0f / 128.0f))
	{
		creature->HeadRight = true;
		creature->HeadLeft = true;
	}
	else if (Random::TestProbability(1.0f / 96.0f))
	{
		creature->HeadRight = false;
		creature->HeadLeft = true;
	}
	else if (Random::TestProbability(1.0f / 64.0f))
	{
		creature->HeadRight = true;
		creature->HeadLeft = false;
	}

	if (creature->HeadLeft && creature->HeadRight)
		return 0;

	if (creature->HeadLeft)
		return -CREATURE_AI_ROTATION_MAX;

	if (creature->HeadRight)
		return CREATURE_AI_ROTATION_MAX;

	return 0;
}

void AlertNearbyGuards(ItemInfo* item) 
{
	for (int i = 0; i < ActiveCreatures.size(); i++)
	{
		auto* currentCreature = ActiveCreatures[i];
		if (currentCreature->ItemNumber == NO_VALUE)
			continue;

		auto* currentTarget = &g_Level.Items[currentCreature->ItemNumber + i];
		if (item->RoomNumber == currentTarget->RoomNumber)
		{
			currentCreature->Alerted = true;
			continue;
		}

		int x = (currentTarget->Pose.Position.x - item->Pose.Position.x) / 64;
		int y = (currentTarget->Pose.Position.y - item->Pose.Position.y) / 64;
		int z = (currentTarget->Pose.Position.z - item->Pose.Position.z) / 64;

		float distance = SQUARE(z) + SQUARE(y) + SQUARE(x);
		if (distance < BLOCK(8))
			currentCreature->Alerted = true;
	}
}

void AlertAllGuards(short itemNumber) 
{
	for (int i = 0; i < ActiveCreatures.size(); i++)
	{
		auto* creature = ActiveCreatures[i];
		if (creature->ItemNumber == NO_VALUE)
			continue;

		auto* target = &g_Level.Items[creature->ItemNumber];
		short objNumber = g_Level.Items[itemNumber].ObjectNumber;
		if (objNumber == target->ObjectNumber)
		{
			if (target->Status == ITEM_ACTIVE)
				creature->Alerted = true;
		}
	}
}

bool CreaturePathfind(ItemInfo* item, Vector3i prevPos, short angle, short tilt)
{
	int xPos, zPos, ceiling, shiftX, shiftZ;
	short top;

	auto* creature = GetCreatureInfo(item);
	auto* LOT = &creature->LOT;
	int* zone = g_Level.Zones[(int)LOT->Zone][(int)FlipStatus].data();

	int boxHeight;
	if (item->BoxNumber != NO_VALUE)
		boxHeight = g_Level.PathfindingBoxes[item->BoxNumber].height;
	else
		boxHeight = item->Floor;

	auto bounds = GameBoundingBox(item);
	int y = item->Pose.Position.y + bounds.Y1;
	short roomNumber = item->RoomNumber;

	GetFloor(prevPos.x, y, prevPos.z, &roomNumber);
	auto* floor = GetFloor(item->Pose.Position.x, y, item->Pose.Position.z, &roomNumber);
	if (floor->PathfindingBoxID == NO_VALUE)
		return false;

	int height = g_Level.PathfindingBoxes[floor->PathfindingBoxID].height;
	int nextHeight = 0;

	int nextBox;
	if (!Objects[item->ObjectNumber].nonLot)
	{
		nextBox = LOT->Node[floor->PathfindingBoxID].exitBox;
	}
	else
	{
		floor = GetFloor(item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z, &roomNumber);
		height = g_Level.PathfindingBoxes[floor->PathfindingBoxID].height;
		nextBox = floor->PathfindingBoxID;
	}

	if (nextBox == NO_VALUE)
		nextHeight = height;
	else
		nextHeight = g_Level.PathfindingBoxes[nextBox].height;

	if (floor->PathfindingBoxID == NO_VALUE || !LOT->IsJumping &&
		(LOT->Fly == NO_FLYING && item->BoxNumber != NO_VALUE && zone[item->BoxNumber] != zone[floor->PathfindingBoxID] ||
			boxHeight - height > LOT->Step ||
			boxHeight - height < LOT->Drop))
	{
		xPos = item->Pose.Position.x / BLOCK(1);
		zPos = item->Pose.Position.z / BLOCK(1);
		shiftX = prevPos.x / BLOCK(1);
		shiftZ = prevPos.z / BLOCK(1);

		if (xPos < shiftX)
			item->Pose.Position.x = prevPos.x & (~WALL_MASK);
		else if (xPos > shiftX)
			item->Pose.Position.x = prevPos.x | WALL_MASK;

		if (zPos < shiftZ)
			item->Pose.Position.z = prevPos.z & (~WALL_MASK);
		else if (zPos > shiftZ)
			item->Pose.Position.z = prevPos.z | WALL_MASK;

		floor = GetFloor(item->Pose.Position.x, y, item->Pose.Position.z, &roomNumber);

		if (floor->PathfindingBoxID != NO_VALUE)
		{
			height = g_Level.PathfindingBoxes[floor->PathfindingBoxID].height;
			if (!Objects[item->ObjectNumber].nonLot)
			{
				nextBox = LOT->Node[floor->PathfindingBoxID].exitBox;
			}
			else
			{
				floor = GetFloor(item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z, &roomNumber);
				height = g_Level.PathfindingBoxes[floor->PathfindingBoxID].height;
				nextBox = floor->PathfindingBoxID;
			}
		}

		if (nextBox == NO_VALUE)
			nextHeight = height;
		else
			nextHeight = g_Level.PathfindingBoxes[nextBox].height;
	}

	int x = item->Pose.Position.x;
	int z = item->Pose.Position.z;
	xPos = x & WALL_MASK;
	zPos = z & WALL_MASK;
	short radius = Objects[item->ObjectNumber].radius;
	shiftX = 0;
	shiftZ = 0;

	if (zPos < radius)
	{
		if (BadFloor(x, y, z - radius, height, nextHeight, roomNumber, LOT))
			shiftZ = radius - zPos;

		if (xPos < radius)
		{
			if (BadFloor(x - radius, y, z, height, nextHeight, roomNumber, LOT))
				shiftX = radius - xPos;
			else if (!shiftZ && BadFloor(x - radius, y, z - radius, height, nextHeight, roomNumber, LOT))
			{
				if (item->Pose.Orientation.y > -ANGLE(135.0f) && item->Pose.Orientation.y < ANGLE(45.0f))
					shiftZ = radius - zPos;
				else
					shiftX = radius - xPos;
			}
		}
		else if (xPos > BLOCK(1) - radius)
		{
			if (BadFloor(x + radius, y, z, height, nextHeight, roomNumber, LOT))
				shiftX = BLOCK(1) - radius - xPos;
			else if (!shiftZ && BadFloor(x + radius, y, z - radius, height, nextHeight, roomNumber, LOT))
			{
				if (item->Pose.Orientation.y > -ANGLE(45.0f) && item->Pose.Orientation.y < ANGLE(135.0f))
					shiftZ = radius - zPos;
				else
					shiftX = BLOCK(1) - radius - xPos;
			}
		}
	}
	else if (zPos > BLOCK(1) - radius)
	{
		if (BadFloor(x, y, z + radius, height, nextHeight, roomNumber, LOT))
			shiftZ = BLOCK(1) - radius - zPos;

		if (xPos < radius)
		{
			if (BadFloor(x - radius, y, z, height, nextHeight, roomNumber, LOT))
				shiftX = radius - xPos;
			else if (!shiftZ && BadFloor(x - radius, y, z + radius, height, nextHeight, roomNumber, LOT))
			{
				if (item->Pose.Orientation.y > -ANGLE(45.0f) && item->Pose.Orientation.y < ANGLE(135.0f))
					shiftX = radius - xPos;
				else
					shiftZ = BLOCK(1) - radius - zPos;
			}
		}
		else if (xPos > BLOCK(1) - radius)
		{
			if (BadFloor(x + radius, y, z, height, nextHeight, roomNumber, LOT))
				shiftX = BLOCK(1) - radius - xPos;
			else if (!shiftZ && BadFloor(x + radius, y, z + radius, height, nextHeight, roomNumber, LOT))
			{
				if (item->Pose.Orientation.y > -ANGLE(135.0f) && item->Pose.Orientation.y < ANGLE(45.0f))
					shiftX = BLOCK(1) - radius - xPos;
				else
					shiftZ = BLOCK(1) - radius - zPos;
			}
		}
	}
	else if (xPos < radius)
	{
		if (BadFloor(x - radius, y, z, height, nextHeight, roomNumber, LOT))
			shiftX = radius - xPos;
	}
	else if (xPos > BLOCK(1) - radius)
	{
		if (BadFloor(x + radius, y, z, height, nextHeight, roomNumber, LOT))
			shiftX = BLOCK(1) - radius - xPos;
	}

	item->Pose.Position.x += shiftX;
	item->Pose.Position.z += shiftZ;

	if (shiftX || shiftZ)
	{
		floor = GetFloor(item->Pose.Position.x, y, item->Pose.Position.z, &roomNumber);
		item->Pose.Orientation.y += angle;

		if (tilt)
			CreatureTilt(item, (tilt * 2));
	}

	short biffAngle;
	if (item->ObjectNumber != ID_TYRANNOSAUR && item->Animation.Velocity.z && item->HitPoints > 0)
		biffAngle = CreatureCreature(item->Index);
	else
		biffAngle = 0;

	if (biffAngle)
	{
		if (abs(biffAngle) < BIFF_AVOID_TURN)
			item->Pose.Orientation.y -= BIFF_AVOID_TURN;
		else if (biffAngle > 0)
			item->Pose.Orientation.y -= BIFF_AVOID_TURN;
		else
			item->Pose.Orientation.y += BIFF_AVOID_TURN;

		return true;
	}

	if (LOT->Fly != NO_FLYING && item->HitPoints > 0)
	{
		int dy = creature->Target.y - item->Pose.Position.y;
		if (dy > LOT->Fly)
			dy = LOT->Fly;
		else if (dy < -LOT->Fly)
			dy = -LOT->Fly;

		height = GetFloorHeight(floor, item->Pose.Position.x, y, item->Pose.Position.z);
		if (item->Pose.Position.y + dy <= height)
		{
			if (Objects[item->ObjectNumber].waterCreature)
			{
				ceiling = GetCeiling(floor, item->Pose.Position.x, y, item->Pose.Position.z);

				if (item->ObjectNumber == ID_WHALE)
					top = CLICK(0.5f);
				else
					top = bounds.Y1;

				if (item->Pose.Position.y + top + dy < ceiling)
				{
					if (item->Pose.Position.y + top < ceiling)
					{
						item->Pose.Position.x = prevPos.x;
						item->Pose.Position.z = prevPos.z;
						dy = LOT->Fly;
					}
					else
						dy = 0;
				}
			}
			else
			{
				floor = GetFloor(item->Pose.Position.x, y + CLICK(1), item->Pose.Position.z, &roomNumber);
				if (TestEnvironment(ENV_FLAG_WATER, roomNumber) ||
					TestEnvironment(ENV_FLAG_SWAMP, roomNumber))
				{
					dy = -LOT->Fly;
				}
			}
		}
		else if (item->Pose.Position.y <= height)
		{
			item->Pose.Position.y = height;
			dy = 0;
		}
		else
		{
			item->Pose.Position.x = prevPos.x;
			item->Pose.Position.z = prevPos.z;
			dy = -LOT->Fly;
		}

		item->Pose.Position.y += dy;
		floor = GetFloor(item->Pose.Position.x, y, item->Pose.Position.z, &roomNumber);
		item->Floor = GetFloorHeight(floor, item->Pose.Position.x, y, item->Pose.Position.z);

		angle = (item->Animation.Velocity.z) ? phd_atan(item->Animation.Velocity.z, -dy) : 0;
		if (angle < -ANGLE(20.0f))
			angle = -ANGLE(20.0f);
		else if (angle > ANGLE(20.0f))
			angle = ANGLE(20.0f);

		if (angle < item->Pose.Orientation.x - ANGLE(1.0f))
			item->Pose.Orientation.x -= ANGLE(1.0f);
		else if (angle > item->Pose.Orientation.x + ANGLE(1.0f))
			item->Pose.Orientation.x += ANGLE(1.0f);
		else
			item->Pose.Orientation.x = angle;
	}
	else if (LOT->IsJumping)
	{
		floor = GetFloor(item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z, &roomNumber);
		int height2 = GetFloorHeight(floor, item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z);
		item->Floor = height2;

		if (LOT->IsMonkeying)
		{
			ceiling = GetCeiling(floor, item->Pose.Position.x, y, item->Pose.Position.z);
			item->Pose.Position.y = ceiling - bounds.Y1;
		}
		else
		{
			if (item->Pose.Position.y > item->Floor)
			{
				if (item->Pose.Position.y > (item->Floor + CLICK(1)))
					item->Pose.Position = prevPos;
				else
					item->Pose.Position.y = item->Floor;
			}
		}
	}
	else
	{
		floor = GetFloor(item->Pose.Position.x, y, item->Pose.Position.z, &roomNumber);
		ceiling = GetCeiling(floor, item->Pose.Position.x, y, item->Pose.Position.z);

		if (item->ObjectNumber == ID_TYRANNOSAUR || item->ObjectNumber == ID_SHIVA || item->ObjectNumber == ID_MUTANT2)
			top = CLICK(3);
		else
			top = bounds.Y1; // TODO: check if Y1 or Y2

		if (item->Pose.Position.y + top < ceiling)
			item->Pose.Position = prevPos;

		floor = GetFloor(item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z, &roomNumber);
		item->Floor = GetFloorHeight(floor, item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z);

		if (item->Pose.Position.y > item->Floor)
			item->Pose.Position.y = item->Floor;
		else if (item->Floor - item->Pose.Position.y > CLICK(0.25f))
			item->Pose.Position.y += CLICK(0.25f);
		else if (item->Pose.Position.y < item->Floor)
			item->Pose.Position.y = item->Floor;

		item->Pose.Orientation.x = 0;
	}

	UpdateItemRoom(item->Index);
	return true;
}

void CreatureKill(ItemInfo* creatureItem, int creatureAnimNumber, int playerAnimNumber, int creatureState, int playerState)
{
	auto& playerItem = *LaraItem;
	auto& player = GetLaraInfo(playerItem);

	SetAnimation(*creatureItem, creatureAnimNumber);
	SetAnimation(playerItem, ID_LARA_EXTRA_ANIMS, playerAnimNumber);

	playerItem.Pose = creatureItem->Pose;
	playerItem.Animation.IsAirborne = false;
	playerItem.Animation.Velocity = Vector3::Zero;

	if (creatureItem->RoomNumber != playerItem.RoomNumber)
		ItemNewRoom(playerItem.Index, creatureItem->RoomNumber);

	AnimateItem(playerItem);
	playerItem.HitPoints = -1;
	player.Control.HandStatus = HandStatus::Busy;
	player.Control.Weapon.GunType = LaraWeaponType::None;
	player.ExtraAnim = 1;
	player.HitDirection = -1;

	Camera.pos.RoomNumber = playerItem.RoomNumber;
	Camera.flags = CF_FOLLOW_CENTER;
	Camera.targetAngle = ANGLE(170.0f);
	Camera.targetElevation = ANGLE(-25.0f);
	Camera.targetDistance = BLOCK(2);
}

short CreatureEffect2(ItemInfo* item, const CreatureBiteInfo& bite, short velocity, short angle, std::function<CreatureEffectFunction> func)
{
	auto pos = GetJointPosition(item, bite);
	return func(pos.x, pos.y, pos.z, velocity, angle, item->RoomNumber);
}

short CreatureEffect(ItemInfo* item, const CreatureBiteInfo& bite, std::function<CreatureEffectFunction> func)
{
	auto pos = GetJointPosition(item, bite);
	return func(pos.x, pos.y, pos.z, item->Animation.Velocity.z, item->Pose.Orientation.y, item->RoomNumber);
}

void CreatureUnderwater(ItemInfo* item, int depth)
{
	int waterLevel = depth;
	int waterHeight = 0;

	if (depth < 0)
	{
		waterHeight = abs(depth);
		waterLevel = 0;
	}
	else
	{
		waterHeight = GetPointCollision(*item).GetWaterTopHeight();
	}

	int y = waterHeight + waterLevel;

	if (item->Pose.Position.y < y)
	{
		int height = GetPointCollision(*item).GetFloorHeight();

		item->Pose.Position.y = y;
		if (y > height)
			item->Pose.Position.y = height;

		if (item->Pose.Orientation.x > ANGLE(2.0f))
		{
			item->Pose.Orientation.x -= ANGLE(2.0f);
		}
		else if (item->Pose.Orientation.x > 0)
		{
			item->Pose.Orientation.x = 0;
		}
	}
}

void CreatureFloat(short itemNumber) 
{
	auto* item = &g_Level.Items[itemNumber];

	auto pointColl = GetPointCollision(*item);

	item->HitPoints = NOT_TARGETABLE;
	item->Pose.Orientation.x = 0;

	int y = item->Pose.Position.y;
	int waterLevel = GetPointCollision(*item).GetWaterTopHeight();
	if (waterLevel == NO_HEIGHT)
		return;

	if (y > waterLevel)
		item->Pose.Position.y = y - 32;

	if (item->Pose.Position.y < waterLevel)
		item->Pose.Position.y = waterLevel;

	AnimateItem(*item);

	item->Floor = pointColl.GetFloorHeight();
	if (pointColl.GetRoomNumber() != item->RoomNumber)
		ItemNewRoom(itemNumber, pointColl.GetRoomNumber());

	if (item->Pose.Position.y <= waterLevel)
	{
		if (item->Animation.FrameNumber == 0)
		{
			item->Pose.Position.y = waterLevel;
			item->Collidable = false;
			item->Status = ITEM_DEACTIVATED;
			DisableEntityAI(itemNumber);
			RemoveActiveItem(itemNumber);
			item->AfterDeath = 1;
		}
	}
}

void CreatureJoint(ItemInfo* item, short joint, short required, short maxAngle)
{
	if (!item->IsCreature())
		return;

	auto* creature = GetCreatureInfo(item);

	short change = required - creature->JointRotation[joint];
	if (change > ANGLE(3.0f))
		change = ANGLE(3.0f);
	else if (change < ANGLE(-3.0f))
		change = ANGLE(-3.0f);

	creature->JointRotation[joint] += change;
	if (creature->JointRotation[joint] > maxAngle)
		creature->JointRotation[joint] = maxAngle;
	else if (creature->JointRotation[joint] < -maxAngle)
		creature->JointRotation[joint] = -maxAngle;
}

void CreatureTilt(ItemInfo* item, short angle) 
{
	angle = (angle << 2) - item->Pose.Orientation.z;

	if (angle < -ANGLE(3.0f))
		angle = -ANGLE(3.0f);
	else if (angle > ANGLE(3.0f))
		angle = ANGLE(3.0f);

	short absRot = abs(item->Pose.Orientation.z);
	if (absRot < ANGLE(15.0f) || absRot > ANGLE(30.0f))
		angle >>= 1;
	
	item->Pose.Orientation.z += angle;
}

short CreatureTurn(ItemInfo* item, short maxTurn)
{
	if (!item->IsCreature() || maxTurn == 0)
		return 0;

	auto* creature = GetCreatureInfo(item);
	short angle = 0;

	int x = creature->Target.x - item->Pose.Position.x;
	int z = creature->Target.z - item->Pose.Position.z;
	angle = phd_atan(z, x) - item->Pose.Orientation.y;
	int range = (item->Animation.Velocity.z * 16384) / maxTurn;
	int distance = SQUARE(z) + SQUARE(x);

	if (angle > FRONT_ARC || angle < -FRONT_ARC && distance < SQUARE(range))
		maxTurn /= 2;

	if (angle > maxTurn)
		angle = maxTurn;
	else if (angle < -maxTurn)
		angle = -maxTurn;

	item->Pose.Orientation.y += angle;

	return angle;
}

static void SpawnCreatureGunEffect(const ItemInfo& item, const CreatureMuzzleFlashInfo& muzzleFlash)
{
	if (muzzleFlash.Delay == 0)
		return;

	auto muzzlePos = muzzleFlash.Bite;
	auto pos = GetJointPosition(item, muzzlePos);
	SpawnDynamicLight(pos.x, pos.y, pos.z, 15, 128, 64, 16);

	if (muzzleFlash.UseSmoke)
	{
		muzzlePos.Position.y -= CREATURE_GUN_EFFECT_VERTICAL_OFFSET;
		auto smokePos = GetJointPosition(item, muzzlePos);
		SpawnGunSmokeParticles(smokePos.ToVector3(), Vector3::Zero, item.RoomNumber, 1, LaraWeaponType::Pistol, 12);
	}
}

bool CreatureAnimation(short itemNumber, short headingAngle, short tiltAngle)
{
	auto& item = g_Level.Items[itemNumber];
	if (!item.IsCreature())
		return false;

	auto& creature = *GetCreatureInfo(&item);

	SpawnCreatureGunEffect(item, creature.MuzzleFlash[0]);
	SpawnCreatureGunEffect(item, creature.MuzzleFlash[1]);

	auto prevPos = item.Pose.Position;

	AnimateItem(item);
	ProcessSectorFlags(&item);
	CreatureHealth(&item);

	if (item.Status == ITEM_DEACTIVATED)
	{
		CreatureDie(itemNumber, false);
		return false;
	}

	return CreaturePathfind(&item, prevPos, headingAngle, tiltAngle);
}

void CreatureHealth(ItemInfo* item)
{
	auto* creature = GetCreatureInfo(item);

	if (creature->Poisoned && item->HitPoints > 1 && (GlobalCounter & 0x1F) == 0x1F)
		item->HitPoints--;

	if (!Objects[item->ObjectNumber].waterCreature &&
		TestEnvironment(RoomEnvFlags::ENV_FLAG_WATER, &g_Level.Rooms[item->RoomNumber]))
	{
		auto bounds = GameBoundingBox(item);
		auto height = item->Pose.Position.y - GetPointCollision(*item).GetWaterTopHeight();

		if (abs(bounds.Y1 + bounds.Y2) < height)
			DoDamage(item, INT_MAX);
	}
}

void CreatureDie(int itemNumber, bool doExplosion, bool forceExplosion)
{
	int flags = 0;
	if (doExplosion)
	{
		const auto& item = g_Level.Items[itemNumber];
		const auto& object = Objects[item.ObjectNumber];

		switch (object.hitEffect)
		{
		case HitEffect::Blood:
			flags |= BODY_DO_EXPLOSION | BODY_GIBS;
			break;

		case HitEffect::Smoke:
			flags |= BODY_DO_EXPLOSION | BODY_NO_BOUNCE;
			break;

		case HitEffect::NonExplosive:
			if (forceExplosion)
			{
				flags |= BODY_DO_EXPLOSION;
				break;
			}
			return;

		default:
			flags |= BODY_DO_EXPLOSION;
			break;
		}
	}

	CreatureDie(itemNumber, doExplosion, flags);
}

void CreatureDie(int itemNumber, bool doExplosion, int flags)
{
	auto& item = g_Level.Items[itemNumber];

	item.HitPoints = NOT_TARGETABLE;
	item.Collidable = false;

	if (doExplosion)
	{
		ExplodingDeath(itemNumber, flags);
		KillItem(itemNumber);
	}
	else
	{
		RemoveActiveItem(itemNumber);
	}

	DisableEntityAI(itemNumber);
	item.Flags |= IFLAG_KILLED | IFLAG_INVISIBLE;
	DropPickups(&item);
}

bool BadFloor(int x, int y, int z, int boxHeight, int nextHeight, short roomNumber, LOTInfo* LOT)
{
	auto* floor = GetFloor(x, y, z, &roomNumber);
	if (floor->PathfindingBoxID == NO_VALUE)
		return true;

	if (LOT->IsJumping)
		return false;

	auto* box = &g_Level.PathfindingBoxes[floor->PathfindingBoxID];
	if (box->flags & LOT->BlockMask)
		return true;

	int height = box->height;
	if ((boxHeight - height) > LOT->Step || (boxHeight - height) < LOT->Drop)
		return true;

	if ((boxHeight - height) < -LOT->Step && height > nextHeight)
		return true;

	if (LOT->Fly != NO_FLYING && y > (height + LOT->Fly))
		return true;

	return false;
}

int CreatureCreature(short itemNumber)  
{
	auto* item = &g_Level.Items[itemNumber];
	auto* object = &Objects[item->ObjectNumber];

	int x = item->Pose.Position.x;
	int z = item->Pose.Position.z;
	short radius = object->radius;

	auto* room = &g_Level.Rooms[item->RoomNumber];

	short link = room->itemNumber;
	int distance = 0;
	do
	{
		auto* linked = &g_Level.Items[link];
		
		if (link != itemNumber && linked != LaraItem && linked->IsCreature() && linked->Status == ITEM_ACTIVE && linked->HitPoints > 0) // TODO: deal with LaraItem global.
		{
			int xDistance = abs(linked->Pose.Position.x - x);
			int zDistance = abs(linked->Pose.Position.z - z);
			
			if (xDistance > zDistance)
				distance = xDistance + (zDistance >> 1);
			else
				distance = xDistance + (zDistance >> 1);

			if (distance < radius + Objects[linked->ObjectNumber].radius)
				return phd_atan(linked->Pose.Position.z - z, linked->Pose.Position.x - x) - item->Pose.Orientation.y;
		}

		link = linked->NextItem;
	} while (link != NO_VALUE);

	return 0;
}

bool ValidBox(ItemInfo* item, short zoneNumber, short boxNumber) 
{
	if (boxNumber == NO_VALUE)
		return false;

	const auto& creature = *GetCreatureInfo(item);
	const auto& zone = g_Level.Zones[(int)creature.LOT.Zone][(int)FlipStatus].data();

	if (creature.LOT.Fly == NO_FLYING && zone[boxNumber] != zoneNumber)
		return false;

	const auto& box = g_Level.PathfindingBoxes[boxNumber];
	if (creature.LOT.BlockMask & box.flags)
		return false;

	if (item->Pose.Position.z > (box.left * BLOCK(1)) &&
		item->Pose.Position.z < (box.right * BLOCK(1)) &&
		item->Pose.Position.x > (box.top * BLOCK(1)) &&
		item->Pose.Position.x < (box.bottom * BLOCK(1)))
	{
		return false;
	}

	return true;
}

bool EscapeBox(ItemInfo* item, ItemInfo* enemy, int boxNumber) 
{
	if (boxNumber == NO_VALUE)
		return false;

	const auto& box = g_Level.PathfindingBoxes[boxNumber];
	int x = ((box.top + box.bottom) * BLOCK(0.5f)) - enemy->Pose.Position.x;
	int z = ((box.left + box.right) * BLOCK(0.5f)) - enemy->Pose.Position.z;

	if (x > -ESCAPE_DIST && x < ESCAPE_DIST &&
		z > -ESCAPE_DIST && z < ESCAPE_DIST)
	{
		return false;
	}

	return ((z > 0) == (item->Pose.Position.z > enemy->Pose.Position.z)) ||
		   ((x > 0) == (item->Pose.Position.x > enemy->Pose.Position.x));
}

void TargetBox(LOTInfo* LOT, int boxNumber)
{
	if (boxNumber == NO_VALUE)
		return;
	auto* box = &g_Level.PathfindingBoxes[boxNumber];

	// Maximize target precision. DO NOT change bracket precedence!
	LOT->Target.x = (int)((box->top  * BLOCK(1)) + (float)GetRandomControl() * (((float)(box->bottom - box->top) - 1.0f) / 32.0f) + CLICK(2.0f));
	LOT->Target.z = (int)((box->left * BLOCK(1)) + (float)GetRandomControl() * (((float)(box->right - box->left) - 1.0f) / 32.0f) + CLICK(2.0f));
	LOT->RequiredBox = boxNumber;

	if (LOT->Fly == NO_FLYING)
		LOT->Target.y = box->height;
	else
		LOT->Target.y = box->height - STEPUP_HEIGHT;
}

bool UpdateLOT(LOTInfo* LOT, int depth)
{
	if (LOT->RequiredBox != NO_VALUE && LOT->RequiredBox != LOT->TargetBox)
	{
		LOT->TargetBox = LOT->RequiredBox;

		auto* node = &LOT->Node[LOT->RequiredBox];
		if (node->nextExpansion == NO_VALUE && LOT->Tail != LOT->RequiredBox)
		{
			node->nextExpansion = LOT->Head;

			if (LOT->Head == NO_VALUE)
				LOT->Tail = LOT->TargetBox;

			LOT->Head = LOT->TargetBox;
		}

		node->searchNumber = ++LOT->SearchNumber;
		node->exitBox = NO_VALUE;
	}

	return SearchLOT(LOT, depth);
}

bool SearchLOT(LOTInfo* LOT, int depth)
{
	auto* zone = g_Level.Zones[(int)LOT->Zone][(int)FlipStatus].data();
	int searchZone = zone[LOT->Head];

	for (int i = 0; i < depth; i++)
	{
		if (LOT->Head == NO_VALUE)
		{
			LOT->Tail = NO_VALUE; 
			return false;
		}

		auto* box = &g_Level.PathfindingBoxes[LOT->Head];
		auto* node = &LOT->Node[LOT->Head];

		int index = box->overlapIndex;
		bool done = false;
		if (index >= 0)
		{
			do
			{
				int boxNumber = g_Level.Overlaps[index].box;
				int flags = g_Level.Overlaps[index++].flags;

				if (flags & BOX_END_BIT)
					done = true;
				
				if (LOT->Fly == NO_FLYING && searchZone != zone[boxNumber])
					continue;
				
				int delta = g_Level.PathfindingBoxes[boxNumber].height - box->height;
				if ((delta > LOT->Step || delta < LOT->Drop) && (!(flags & BOX_MONKEY) || !LOT->CanMonkey))
					continue;

				if ((flags & BOX_JUMP) && !LOT->CanJump)
					continue;

				auto* expand = &LOT->Node[boxNumber];
				if ((node->searchNumber & SEARCH_NUMBER) < (expand->searchNumber & SEARCH_NUMBER))
					continue;

				if (node->searchNumber & SEARCH_BLOCKED)
				{
					if ((node->searchNumber & SEARCH_NUMBER) == (expand->searchNumber & SEARCH_NUMBER))
						continue;

					expand->searchNumber = node->searchNumber;
				}
				else
				{
					if ((node->searchNumber & SEARCH_NUMBER) == (expand->searchNumber & SEARCH_NUMBER) && !(expand->searchNumber & SEARCH_BLOCKED))
						continue;

					if (g_Level.PathfindingBoxes[boxNumber].flags & LOT->BlockMask)
					{
						expand->searchNumber = node->searchNumber | SEARCH_BLOCKED;
					}
					else
					{
						expand->searchNumber = node->searchNumber;
						expand->exitBox = LOT->Head;
					}
				}

				if (expand->nextExpansion == NO_VALUE && boxNumber != LOT->Tail)
				{
					LOT->Node[LOT->Tail].nextExpansion = boxNumber;
					LOT->Tail = boxNumber;
				}
			} while (!done);
		}

		LOT->Head = node->nextExpansion;
		node->nextExpansion = NO_VALUE;
	}

	return true;
}

#if CREATURE_AI_PRIORITY_OPTIMIZATION
CreatureAIPriority GetCreatureLOTPriority(ItemInfo* item)
{
	auto itemPos = item->Pose.Position.ToVector3();
	auto cameraPos = Camera.pos.ToVector3();

	float distance = Vector3::Distance(itemPos, cameraPos) / BLOCK(1);
	if (distance <= HIGH_PRIO_RANGE)
		return CreatureAIPriority::High;

	if (distance <= MEDIUM_PRIO_RANGE)
		return CreatureAIPriority::Medium;

	if (distance <= LOW_PRIO_RANGE)
		return CreatureAIPriority::Low;

	return CreatureAIPriority::None;
}
#endif

bool CreatureActive(short itemNumber)
{
	auto* item = &g_Level.Items[itemNumber];

	// Object is not a creature.
	if (!Objects[item->ObjectNumber].intelligent)
		return false;

	// Object is already dead.
	if (item->Flags & IFLAG_KILLED)
		return false;

	if (item->Status == ITEM_INVISIBLE || !item->IsCreature())
	{
		// AI couldn't be activated.
		if (!EnableEntityAI(itemNumber, false))
			return false;

		item->Status = ITEM_ACTIVE;
	}

#ifdef CREATURE_AI_PRIORITY_OPTIMIZATION
	auto* creature = GetCreatureInfo(item);
	creature->Priority = GetCreatureLOTPriority(item);
#endif // CREATURE_AI_PRIORITY_OPTIMIZATION

	return true;
}

void InitializeCreature(short itemNumber) 
{
	auto* item = &g_Level.Items[itemNumber];

	item->Collidable = true;
	item->Data = nullptr;
	item->StartPose = item->Pose;
}

bool StalkBox(ItemInfo* item, ItemInfo* enemy, int boxNumber)
{
	if (enemy == nullptr || boxNumber == NO_VALUE)
		return false;
	auto* box = &g_Level.PathfindingBoxes[boxNumber];

	int xRange = STALK_DIST + ((box->bottom - box->top) * BLOCK(1));
	int zRange = STALK_DIST + ((box->right - box->left) * BLOCK(1));
	int x = (box->top + box->bottom) * BLOCK(1) / 2 - enemy->Pose.Position.x;
	int z = (box->left + box->right) * BLOCK(1) / 2 - enemy->Pose.Position.z;

	if (x > xRange || x < -xRange || z > zRange || z < -zRange)
		return false;

	int enemyQuad = (enemy->Pose.Orientation.y / ANGLE(90.0f)) + 2;
	int boxQuad;
	if (z > 0)
		boxQuad = (x > 0) ? 2 : 1;
	else
		boxQuad = (x > 0) ? 3 : 0;

	if (enemyQuad == boxQuad)
		return false;

	int baddyQuad = 0;
	if (item->Pose.Position.z > enemy->Pose.Position.z)
		baddyQuad = (item->Pose.Position.x > enemy->Pose.Position.x) ? 2 : 1;
	else
		baddyQuad = (item->Pose.Position.x > enemy->Pose.Position.x) ? 3 : 0;

	if (enemyQuad == baddyQuad && abs(enemyQuad - boxQuad) == 2)
		return false;

	return true;
}

// TODO: Do it via Lua instead. -- TokyoSU 22.12.21
bool IsCreatureVaultAvailable(ItemInfo* item, int stepCount)
{
	switch (stepCount)
	{
	case -4:
		return (item->ObjectNumber != ID_SMALL_SPIDER);

	case -3:
		return (item->ObjectNumber != ID_CIVVY &&
				item->ObjectNumber != ID_MP_WITH_STICK &&
				item->ObjectNumber != ID_YETI &&
				item->ObjectNumber != ID_LIZARD &&
				item->ObjectNumber != ID_APE &&
				item->ObjectNumber != ID_SMALL_SPIDER &&
			    item->ObjectNumber != ID_SOPHIA_LEIGH_BOSS);

	case -2:
		return (item->ObjectNumber != ID_BADDY1 &&
				item->ObjectNumber != ID_BADDY2 &&
				item->ObjectNumber != ID_CIVVY &&
				item->ObjectNumber != ID_MP_WITH_STICK &&
				item->ObjectNumber != ID_YETI &&
				item->ObjectNumber != ID_LIZARD &&
				item->ObjectNumber != ID_APE &&
				item->ObjectNumber != ID_SMALL_SPIDER &&
				item->ObjectNumber != ID_SOPHIA_LEIGH_BOSS);
	}

	return true;
}

int CreatureVault(short itemNumber, short angle, int vault, int shift)
{
	auto* item = &g_Level.Items[itemNumber];
	auto* creature = GetCreatureInfo(item);

	int xBlock = item->Pose.Position.x / BLOCK(1);
	int zBlock = item->Pose.Position.z / BLOCK(1);
	int y = item->Pose.Position.y;
	short roomNumber = item->RoomNumber;

	CreatureAnimation(itemNumber, angle, 0);

	if (item->Floor > (y + CLICK(4.5f)))
	{
		vault = 0;
	}
	else if (item->Floor > (y + CLICK(3.5f)) && IsCreatureVaultAvailable(item, -4))
	{
		vault = -4;
	}
	else if (item->Floor > (y + CLICK(2.5f)) && IsCreatureVaultAvailable(item, -3))
	{
		vault = -3;
	}
	else if (item->Floor > (y + CLICK(1.5f)) && IsCreatureVaultAvailable(item, -2))
	{
		vault = -2;
	}
	else if (item->Pose.Position.y > (y - CLICK(1.5f)))
	{
		return 0;
	}
	else if (item->Pose.Position.y > (y - CLICK(2.5f)))
	{
		vault = 2;
	}
	else if (item->Pose.Position.y > (y - CLICK(3.5f)))
	{
		vault = 3;
	}
	else if (item->Pose.Position.y > (y - CLICK(4.5f)))
	{
		vault = 4;
	}

	// Jump
	int newXblock = item->Pose.Position.x / BLOCK(1);
	int newZblock = item->Pose.Position.z / BLOCK(1);

	if (zBlock == newZblock)
	{
		if (xBlock == newXblock)
			return 0;

		if (xBlock < newXblock)
		{
			item->Pose.Position.x = (newXblock * BLOCK(1)) - shift;
			item->Pose.Orientation.y = ANGLE(90.0f);
		}
		else
		{
			item->Pose.Position.x = (xBlock * BLOCK(1)) + shift;
			item->Pose.Orientation.y = -ANGLE(90.0f);
		}
	}
	else if (xBlock == newXblock)
	{
		if (zBlock < newZblock)
		{
			item->Pose.Position.z = (newZblock * BLOCK(1)) - shift;
			item->Pose.Orientation.y = 0;
		}
		else
		{
			item->Pose.Position.z = (zBlock * BLOCK(1)) + shift;
			item->Pose.Orientation.y = -ANGLE(180.0f);
		}
	}

	item->Pose.Position.y = y;
	item->Floor = y;

	if (roomNumber != item->RoomNumber)
		ItemNewRoom(itemNumber, roomNumber);

	return vault;
}

void GetAITarget(CreatureInfo* creature)
{
	auto* enemy = creature->Enemy;

	short enemyObjectNumber;
	if (enemy)
		enemyObjectNumber = enemy->ObjectNumber;
	else
		enemyObjectNumber = NO_VALUE;

	auto* item = &g_Level.Items[creature->ItemNumber];

	if (item->AIBits & GUARD)
	{
		creature->Enemy = LaraItem;
		if (creature->Alerted)
		{
			item->AIBits = ~GUARD;
			if (item->AIBits & AMBUSH)
				item->AIBits |= MODIFY;
		}
	}
	else if (item->AIBits & PATROL1)
	{
		if (creature->Alerted || creature->HurtByLara)
		{
			item->AIBits &= ~PATROL1;
			if (item->AIBits & AMBUSH)
			{
				item->AIBits |= MODIFY;
				// NOTE: added in TR5
				//item->itemFlags[3] = (creature->Tosspad & 0xFF);
			}
		}
		else if (!creature->Patrol)
		{
			if (enemyObjectNumber != ID_AI_PATROL1)
				FindAITargetObject(creature, ID_AI_PATROL1);
		}
		else if (enemyObjectNumber != ID_AI_PATROL2)
		{
			FindAITargetObject(creature, ID_AI_PATROL2);
		}
		else if (abs(enemy->Pose.Position.x - item->Pose.Position.x) < REACHED_GOAL_RADIUS &&
			abs(enemy->Pose.Position.y - item->Pose.Position.y) < REACHED_GOAL_RADIUS &&
			abs(enemy->Pose.Position.z - item->Pose.Position.z) < REACHED_GOAL_RADIUS ||
			Objects[item->ObjectNumber].waterCreature)
		{
			TestTriggers(enemy, true);
			creature->Patrol = !creature->Patrol;
		}
	}
	else if (item->AIBits & AMBUSH)
	{
		// First if was removed probably after TR3 and was it used by monkeys?
		/*if (!(item->aiBits & MODIFY) && !creature->hurtByLara)
			creature->enemy = LaraItem;
		else*/ if (enemyObjectNumber != ID_AI_AMBUSH)
			FindAITargetObject(creature, ID_AI_AMBUSH);
		/*else if (item->objectNumber == ID_MONKEY)
			return;*/
		else if (abs(enemy->Pose.Position.x - item->Pose.Position.x) < REACHED_GOAL_RADIUS &&
			abs(enemy->Pose.Position.y - item->Pose.Position.y) < REACHED_GOAL_RADIUS &&
			abs(enemy->Pose.Position.z - item->Pose.Position.z) < REACHED_GOAL_RADIUS)
		{
			TestTriggers(enemy, true);		
			creature->ReachedGoal = true;
			creature->Enemy = LaraItem;
			item->AIBits &= ~(AMBUSH /* | MODIFY*/);
			if (item->AIBits != MODIFY)
			{
				item->AIBits |= GUARD;
				creature->Alerted = false;
			}
		}
	}
	else if (item->AIBits & FOLLOW)
	{
		if (creature->HurtByLara)
		{
			creature->Enemy = LaraItem;
			creature->Alerted = true;
			//item->aiBits &= ~FOLLOW;
		}
		else if (item->HitStatus)
		{
			item->AIBits &= ~FOLLOW;
		}
		else if (enemyObjectNumber != ID_AI_FOLLOW)
		{
			FindAITargetObject(creature, ID_AI_FOLLOW);
		}
		else if (abs(enemy->Pose.Position.x - item->Pose.Position.x) < REACHED_GOAL_RADIUS &&
			abs(enemy->Pose.Position.y - item->Pose.Position.y) < REACHED_GOAL_RADIUS &&
			abs(enemy->Pose.Position.z - item->Pose.Position.z) < REACHED_GOAL_RADIUS)
		{
			creature->ReachedGoal = true;
			item->AIBits &= ~FOLLOW;
		}
	}
	/*else if (item->objectNumber == ID_MONKEY && item->carriedItem == NO_VALUE)
	{
		if (item->aiBits != MODIFY)
		{
			if (enemyObjectNumber != ID_SMALLMEDI_ITEM)
				FindAITargetObject(creature, ID_SMALLMEDI_ITEM);
		}
		else
		{
			if (enemyObjectNumber != ID_KEY_ITEM4)
				FindAITargetObject(creature, ID_KEY_ITEM4);
		}
	}*/
}

// Old TR3 way.
void FindAITarget(CreatureInfo* creature, short objectNumber)
{
	const auto& item = g_Level.Items[creature->ItemNumber];

	int i;
	ItemInfo* targetItem;
	for (i = 0, targetItem = &g_Level.Items[0]; i < g_Level.NumItems; i++, targetItem++)
	{
		if (targetItem->ObjectNumber != objectNumber)
			continue;

		if (targetItem->RoomNumber == NO_VALUE)
			continue;

		if (SameZone(creature, targetItem) &&
			targetItem->Pose.Orientation.y == item.ItemFlags[3])
		{
			creature->Enemy = targetItem;
			break;
		}
	}
}

void FindAITargetObject(CreatureInfo* creature, int objectNumber)
{
	const auto& item = g_Level.Items[creature->ItemNumber];

	FindAITargetObject(creature, objectNumber, item.ItemFlags[3], true);
}

void FindAITargetObject(CreatureInfo* creature, int objectNumber, int ocb, bool checkSameZone)
{
	auto& item = g_Level.Items[creature->ItemNumber];

	if (g_Level.AIObjects.empty())
		return;

	AI_OBJECT* foundObject = nullptr;

	for (auto& aiObject : g_Level.AIObjects)
	{
		if (aiObject.objectNumber == objectNumber &&
			aiObject.triggerFlags == ocb &&
			aiObject.roomNumber != NO_VALUE)
		{
			int* zone = g_Level.Zones[(int)creature->LOT.Zone][(int)FlipStatus].data();
			auto* room = &g_Level.Rooms[item.RoomNumber];

			item.BoxNumber = GetSector(room, item.Pose.Position.x - room->Position.x, item.Pose.Position.z - room->Position.z)->PathfindingBoxID;
			room = &g_Level.Rooms[aiObject.roomNumber];
			aiObject.boxNumber = GetSector(room, aiObject.pos.Position.x - room->Position.x, aiObject.pos.Position.z - room->Position.z)->PathfindingBoxID;

			if (item.BoxNumber == NO_VALUE || aiObject.boxNumber == NO_VALUE)
				return;

			if (checkSameZone && (zone[item.BoxNumber] != zone[aiObject.boxNumber]))
				return;

			// Don't check for same zone. Needed for Sophia Leigh.
			foundObject = &aiObject;
		}
	}

	if (foundObject == nullptr)
		return;

	auto& aiItem = *creature->AITarget;

	creature->Enemy = &aiItem;

	aiItem.ObjectNumber = foundObject->objectNumber;
	aiItem.RoomNumber = foundObject->roomNumber;
	aiItem.Pose.Position = foundObject->pos.Position;
	aiItem.Pose.Orientation.y = foundObject->pos.Orientation.y;
	aiItem.Flags = foundObject->flags;
	aiItem.TriggerFlags = foundObject->triggerFlags;
	aiItem.BoxNumber = foundObject->boxNumber;

	if (!(creature->AITarget->Flags & ItemFlags::IFLAG_TRIGGERED))
	{
		float sinY = phd_sin(creature->AITarget->Pose.Orientation.y);
		float cosY = phd_cos(creature->AITarget->Pose.Orientation.y);

		creature->AITarget->Pose.Position.x += CLICK(1) * sinY;
		creature->AITarget->Pose.Position.z += CLICK(1) * cosY;
	}
}

int TargetReachable(ItemInfo* item, ItemInfo* enemy)
{
	const auto& creature = *GetCreatureInfo(item);
	auto& room = g_Level.Rooms[enemy->RoomNumber];
	auto* floor = GetSector(&room, enemy->Pose.Position.x - room.Position.x, enemy->Pose.Position.z - room.Position.z);

	// NEW: Only update enemy box number if it is actually reachable by the enemy.
	// This prevents enemies from running to the player and attacking nothing when they are hanging or shimmying. -- Lwmte, 27.06.22

	bool isReachable = false;
	if (creature.LOT.Zone == ZoneType::Flyer ||
	   (creature.LOT.Zone == ZoneType::Water && TestEnvironment(RoomEnvFlags::ENV_FLAG_WATER, item->RoomNumber)))
	{
		// If NPC is flying or swimming in water, player is always reachable.
		isReachable = true;
	}
	else
	{
		auto pointColl = GetPointCollision(enemy->Pose.Position, floor->RoomNumber);
		auto bounds = GameBoundingBox(item);
		isReachable = abs(enemy->Pose.Position.y - pointColl.GetFloorHeight()) < bounds.GetHeight();
	}

	return (isReachable ? floor->PathfindingBoxID : NO_VALUE);
}

void CreatureAIInfo(ItemInfo* item, AI_INFO* AI)
{
	if (!item->IsCreature())
		return;

	auto* object = &Objects[item->ObjectNumber];
	auto* creature = GetCreatureInfo(item);
	auto* enemy = creature->Enemy;

	// TODO: Deal with LaraItem global.
	if (enemy == nullptr)
	{
		enemy = LaraItem;
		creature->Enemy = LaraItem;
	}

	auto* zone = g_Level.Zones[(int)creature->LOT.Zone][(int)FlipStatus].data();
	auto* room = &g_Level.Rooms[item->RoomNumber];

	item->BoxNumber = GetSector(room, item->Pose.Position.x - room->Position.x, item->Pose.Position.z - room->Position.z)->PathfindingBoxID;
	AI->zoneNumber = zone[item->BoxNumber];

	enemy->BoxNumber = TargetReachable(item, enemy);
	AI->enemyZone = enemy->BoxNumber == NO_VALUE ? NO_VALUE : zone[enemy->BoxNumber];

	if (!object->nonLot)
	{
		if (enemy->BoxNumber != NO_VALUE && g_Level.PathfindingBoxes[enemy->BoxNumber].flags & creature->LOT.BlockMask)
		{
			AI->enemyZone |= BLOCKED;
		}
		else if (item->BoxNumber != NO_VALUE && 
			creature->LOT.Node[item->BoxNumber].searchNumber == (creature->LOT.SearchNumber | SEARCH_BLOCKED))
		{
			AI->enemyZone |= BLOCKED;
		}
	}

	auto vector = Vector3i::Zero;
	if (enemy->IsLara())
	{
		auto* lara = GetLaraInfo(enemy);
		vector.x = enemy->Pose.Position.x + (PREDICTIVE_SCALE_FACTOR * enemy->Animation.Velocity.z * phd_sin(lara->Control.MoveAngle)) - (object->pivotLength * phd_sin(item->Pose.Orientation.y)) - item->Pose.Position.x;
		vector.z = enemy->Pose.Position.z + (PREDICTIVE_SCALE_FACTOR * enemy->Animation.Velocity.z * phd_cos(lara->Control.MoveAngle)) - (object->pivotLength * phd_cos(item->Pose.Orientation.y)) - item->Pose.Position.z;
	}
	else
	{
		vector.x = enemy->Pose.Position.x + (PREDICTIVE_SCALE_FACTOR * enemy->Animation.Velocity.z * phd_sin(enemy->Pose.Orientation.y)) - (object->pivotLength * phd_sin(item->Pose.Orientation.y)) - item->Pose.Position.x;
		vector.z = enemy->Pose.Position.z + (PREDICTIVE_SCALE_FACTOR * enemy->Animation.Velocity.z * phd_cos(enemy->Pose.Orientation.y)) - (object->pivotLength * phd_cos(item->Pose.Orientation.y)) - item->Pose.Position.z;
	}

	vector.y = item->Pose.Position.y - enemy->Pose.Position.y;
	short angle = phd_atan(vector.z, vector.x);

	if (vector.x > BLOCK(31.25f) || vector.x < -BLOCK(31.25f) ||
		vector.z > BLOCK(31.25f) || vector.z < -BLOCK(31.25f))
	{
		AI->distance = INT_MAX;
		AI->verticalDistance = INT_MAX;
	}
	else
	{
		if (creature->Enemy != nullptr)
		{
			// TODO: distance is squared, verticalDistance is not. Desquare distance later. -- Lwmte, 27.06.22
			AI->distance = SQUARE(vector.z) + SQUARE(vector.x); // 2D distance.
			AI->verticalDistance = vector.y;
		}
		else
			AI->distance = AI->verticalDistance = INT_MAX;
	}

	AI->angle = angle - item->Pose.Orientation.y;
	AI->enemyFacing = (angle - enemy->Pose.Orientation.y) + ANGLE(180.0f);

	vector.x = abs(vector.x);
	vector.z = abs(vector.z);

	// Makes Lara smaller.
	if (enemy->IsLara())
	{
		if (GetLaraInfo(enemy)->Control.IsLow)
			vector.y -= STEPUP_HEIGHT;
	}

	if (vector.x > vector.z)
		AI->xAngle = phd_atan(vector.x + (vector.z >> 1), vector.y);
	else
		AI->xAngle = phd_atan(vector.z + (vector.x >> 1), vector.y);

	AI->ahead = (AI->angle > -FRONT_ARC && AI->angle < FRONT_ARC);
	AI->bite = (AI->ahead && enemy->HitPoints > 0 && abs(enemy->Pose.Position.y - item->Pose.Position.y) <= CLICK(2));
}

void CreatureMood(ItemInfo* item, AI_INFO* AI, bool isViolent)
{
	if (!item->IsCreature())
		return;

	auto* creature = GetCreatureInfo(item);
	auto* LOT = &creature->LOT;

	auto* enemy = creature->Enemy;
	if (enemy == nullptr)
		return;

	int boxNumber;
	switch (creature->Mood)
	{
	case MoodType::Bored:
		boxNumber = LOT->Node[GetRandomControl() * LOT->ZoneCount >> 15].boxNumber;
		if (ValidBox(item, AI->zoneNumber, boxNumber))
		{
			if (StalkBox(item, enemy, boxNumber) && enemy->HitPoints > 0 && creature->Enemy)
			{
				TargetBox(LOT, boxNumber);
				creature->Mood = MoodType::Bored;
			}
			else if (LOT->RequiredBox == NO_VALUE)
			{
				TargetBox(LOT, boxNumber);
			}
		}

		break;

	case MoodType::Attack:
		LOT->Target = enemy->Pose.Position;
		LOT->RequiredBox = enemy->BoxNumber;

		if (LOT->Fly != NO_FLYING && Lara.Control.WaterStatus == WaterStatus::Dry)
		{
			auto& bounds = GetClosestKeyframe(*enemy).BoundingBox;
			LOT->Target.y += bounds.Y1;
		}

		break;

	case MoodType::Escape:
		boxNumber = LOT->Node[GetRandomControl() * LOT->ZoneCount >> 15].boxNumber;

		if (ValidBox(item, AI->zoneNumber, boxNumber) && LOT->RequiredBox == NO_VALUE)
		{
			if (EscapeBox(item, enemy, boxNumber))
			{
				TargetBox(LOT, boxNumber);
			}
			else if (AI->zoneNumber == AI->enemyZone && StalkBox(item, enemy, boxNumber) && !isViolent)
			{
				TargetBox(LOT, boxNumber);
				creature->Mood = MoodType::Stalk;
			}
		}

		break;

	case MoodType::Stalk:
		if (LOT->RequiredBox == NO_VALUE || !StalkBox(item, enemy, LOT->RequiredBox))
		{
			boxNumber = LOT->Node[GetRandomControl() * LOT->ZoneCount >> 15].boxNumber;
			if (ValidBox(item, AI->zoneNumber, boxNumber))
			{
				if (StalkBox(item, enemy, boxNumber))
				{
					TargetBox(LOT, boxNumber);
				}
				else if (LOT->RequiredBox == NO_VALUE)
				{
					TargetBox(LOT, boxNumber);
					if (AI->zoneNumber != AI->enemyZone)
						creature->Mood = MoodType::Bored;
				}
			}
		}

		break;
	}

	if (LOT->TargetBox == NO_VALUE)
		TargetBox(LOT, item->BoxNumber);

#ifdef CREATURE_AI_PRIORITY_OPTIMIZATION
	bool shouldUpdateTarget = false;

	switch(creature->Priority)
	{
		case CreatureAIPriority::High:
			shouldUpdateTarget = true;
			break;

		case CreatureAIPriority::Medium:
			if (creature->FramesSinceLOTUpdate > std::pow(FRAME_PRIO_BASE, FRAME_PRIO_EXP))
				shouldUpdateTarget = true;

			break;

		case CreatureAIPriority::Low:
			if (creature->FramesSinceLOTUpdate > std::pow(FRAME_PRIO_BASE, FRAME_PRIO_EXP * 2))
				shouldUpdateTarget = true;

			break;

		default:
			break;
	}

	if (shouldUpdateTarget)
	{
		CalculateTarget(&creature->Target, item, &creature->LOT);
		creature->FramesSinceLOTUpdate = 0;
	}
	else
	{
		creature->FramesSinceLOTUpdate++;
	}
#else
	CalculateTarget(&creature->Target, item, &creature->LOT);
#endif // CREATURE_AI_PRIORITY_OPTIMIZATION

	creature->JumpAhead = false;
	creature->MonkeySwingAhead = false;

	if (item->BoxNumber != NO_VALUE)
	{
		int endBox = LOT->Node[item->BoxNumber].exitBox;
		if (endBox != NO_VALUE)
		{
			int overlapIndex = g_Level.PathfindingBoxes[item->BoxNumber].overlapIndex;
			int nextBox = 0;
			int flags = 0;

			if (overlapIndex >= 0)
			{
				do
				{
					nextBox = g_Level.Overlaps[overlapIndex].box;
					flags = g_Level.Overlaps[overlapIndex++].flags;
				} while (nextBox != NO_VALUE && ((flags & BOX_END_BIT) == false) && (nextBox != endBox));
			}

			if (nextBox == endBox)
			{
				if (flags & BOX_JUMP)
					creature->JumpAhead = true;

				if (flags & BOX_MONKEY)
					creature->MonkeySwingAhead = true;
			}
		}
	}
}

void GetCreatureMood(ItemInfo* item, AI_INFO* AI, bool isViolent)
{
	if (!item->IsCreature())
		return;

	auto* creature = GetCreatureInfo(item);
	auto* enemy = creature->Enemy;
	auto* LOT = &creature->LOT;

	if (item->BoxNumber == NO_VALUE || creature->LOT.Node[item->BoxNumber].searchNumber == (creature->LOT.SearchNumber | SEARCH_BLOCKED))
		creature->LOT.RequiredBox = NO_VALUE;

	if (creature->Mood != MoodType::Attack && creature->LOT.RequiredBox != NO_VALUE && !ValidBox(item, AI->zoneNumber, creature->LOT.TargetBox))
	{
		if (AI->zoneNumber == AI->enemyZone)
			creature->Mood = MoodType::Bored;

		creature->LOT.RequiredBox = NO_VALUE;
	}

	auto mood = creature->Mood;
	if (enemy)
	{
		if (enemy->HitPoints <= 0 && enemy == LaraItem) // TODO: deal with LaraItem global !
		{
			creature->Mood = MoodType::Bored;
		}
		else if (isViolent)
		{
			switch (creature->Mood)
			{
			case MoodType::Bored:
			case MoodType::Stalk:
				if (AI->zoneNumber == AI->enemyZone)
					creature->Mood = MoodType::Attack;
				else if (item->HitStatus)
					creature->Mood = MoodType::Escape;

				break;

			case MoodType::Attack:
				if (AI->zoneNumber != AI->enemyZone)
					creature->Mood = MoodType::Bored;

				break;

			case MoodType::Escape:
				if (AI->zoneNumber == AI->enemyZone)
					creature->Mood = MoodType::Attack;

				break;
			}
		}
		else
		{
			switch (creature->Mood)
			{
			case MoodType::Bored:
			case MoodType::Stalk:
				if (creature->Alerted && AI->zoneNumber != AI->enemyZone)
				{
					if (AI->distance > BLOCK(3))
						creature->Mood = MoodType::Stalk;
					else
						creature->Mood = MoodType::Bored;
				}
				else if (AI->zoneNumber == AI->enemyZone)
				{
					if (AI->distance < ATTACK_RANGE ||
						(creature->Mood == MoodType::Stalk && LOT->RequiredBox == NO_VALUE))
						creature->Mood = MoodType::Attack;
					else
						creature->Mood = MoodType::Stalk;
				}

				break;

			case MoodType::Attack:
				if (item->HitStatus &&
					(GetRandomControl() < ESCAPE_CHANCE ||
						AI->zoneNumber != AI->enemyZone))
					creature->Mood = MoodType::Stalk;
				else if (AI->zoneNumber != AI->enemyZone && AI->distance > BLOCK(6))
					creature->Mood = MoodType::Bored;

				break;

			case MoodType::Escape:
				if (AI->zoneNumber == AI->enemyZone && GetRandomControl() < RECOVER_CHANCE)
					creature->Mood = MoodType::Stalk;

				break;
			}
		}
	}
	else
	{
		creature->Mood = MoodType::Bored;
	}

	if (mood != creature->Mood)
	{
		if (mood == MoodType::Attack)
		{
			TargetBox(LOT, LOT->TargetBox);
			LOT = &creature->LOT;
		}

		LOT->RequiredBox = NO_VALUE;
	}
}

TARGET_TYPE CalculateTarget(Vector3i* target, ItemInfo* item, LOTInfo* LOT)
{
	UpdateLOT(LOT, 5);

	*target = item->Pose.Position;

	int boxNumber = item->BoxNumber;
	if (boxNumber == NO_VALUE)
		return TARGET_TYPE::NO_TARGET;

	auto* box = &g_Level.PathfindingBoxes[boxNumber];

	int boxLeft = ((int)box->left * BLOCK(1));
	int boxRight = ((int)box->right * BLOCK(1)) - 1;
	int boxTop = ((int)box->top * BLOCK(1));
	int boxBottom = ((int)box->bottom * BLOCK(1)) - 1;
	int left = boxLeft;
	int right = boxRight;
	int top = boxTop;
	int bottom = boxBottom;
	int direction = CLIP_ALL;

	do
	{
		box = &g_Level.PathfindingBoxes[boxNumber];

		if (LOT->Fly != NO_FLYING)
		{
			if (target->y > box->height - BLOCK(1))
				target->y = box->height - BLOCK(1);
		}
		else if(target->y > box->height)
			target->y = box->height;

		boxLeft = ((int)box->left * BLOCK(1));
		boxRight = ((int)box->right * BLOCK(1)) - 1;
		boxTop = ((int)box->top * BLOCK(1));
		boxBottom = ((int)box->bottom * BLOCK(1)) - 1;

		if (item->Pose.Position.z >= boxLeft &&
			item->Pose.Position.z <= boxRight &&
			item->Pose.Position.x >= boxTop &&
			item->Pose.Position.x <= boxBottom)
		{
			left = ((int)box->left * BLOCK(1));
			right = ((int)box->right * BLOCK(1)) - 1;
			top = ((int)box->top * BLOCK(1));
			bottom = ((int)box->bottom * BLOCK(1)) - 1;
		}
		else
		{
			if (item->Pose.Position.z < boxLeft && direction != CLIP_RIGHT)
			{
				if ((direction & CLIP_LEFT) &&
					item->Pose.Position.x >= boxTop &&
					item->Pose.Position.x <= boxBottom)
				{
					if (target->z < (boxLeft + CLICK(2)))
						target->z = boxLeft + CLICK(2);

					if (direction & CLIP_SECONDARY)
						return TARGET_TYPE::SECONDARY_TARGET;

					if (boxTop > top)
						top = boxTop;

					if (boxBottom < bottom)
						bottom = boxBottom;

					direction = CLIP_LEFT;
				}
				else if (direction != CLIP_LEFT)
				{
					target->z = (right - CLICK(2));

					if (direction != CLIP_ALL)
						return TARGET_TYPE::SECONDARY_TARGET;

					direction |= (CLIP_ALL | CLIP_SECONDARY);
				}
			}
			else if (item->Pose.Position.z > boxRight && direction != CLIP_LEFT)
			{
				if ((direction & CLIP_RIGHT) &&
					item->Pose.Position.x >= boxTop &&
					item->Pose.Position.x <= boxBottom)
				{
					if (target->z > boxRight - CLICK(2))
						target->z = boxRight - CLICK(2);

					if (direction & CLIP_SECONDARY)
						return TARGET_TYPE::SECONDARY_TARGET;

					if (boxTop > top)
						top = boxTop;

					if (boxBottom < bottom)
						bottom = boxBottom;

					direction = CLIP_RIGHT;
				}
				else if (direction != CLIP_RIGHT)
				{
					target->z = left + CLICK(2);

					if (direction != CLIP_ALL)
						return TARGET_TYPE::SECONDARY_TARGET;

					direction |= (CLIP_ALL | CLIP_SECONDARY);
				}
			}

			if (item->Pose.Position.x < boxTop && direction != CLIP_BOTTOM)
			{
				if ((direction & CLIP_TOP) &&
					item->Pose.Position.z >= boxLeft &&
					item->Pose.Position.z <= boxRight)
				{
					if (target->x < boxTop + CLICK(2))
						target->x = boxTop + CLICK(2);

					if (direction & CLIP_SECONDARY)
						return TARGET_TYPE::SECONDARY_TARGET;

					if (boxLeft > left)
						left = boxLeft;

					if (boxRight < right)
						right = boxRight;

					direction = CLIP_TOP;
				}
				else if (direction != CLIP_TOP)
				{
					target->x = bottom - CLICK(2);

					if (direction != CLIP_ALL)
						return TARGET_TYPE::SECONDARY_TARGET;

					direction |= (CLIP_ALL | CLIP_SECONDARY);
				}
			}
			else if (item->Pose.Position.x > boxBottom && direction != CLIP_TOP)
			{
				if ((direction & CLIP_BOTTOM) &&
					item->Pose.Position.z >= boxLeft &&
					item->Pose.Position.z <= boxRight)
				{
					if (target->x > (boxBottom - CLICK(2)))
						target->x = (boxBottom - CLICK(2));

					if (direction & CLIP_SECONDARY)
						return TARGET_TYPE::SECONDARY_TARGET;

					if (boxLeft > left)
						left = boxLeft;

					if (boxRight < right)
						right = boxRight;

					direction = CLIP_BOTTOM;
				}
				else if (direction != CLIP_BOTTOM)
				{
					target->x = top + CLICK(2);

					if (direction != CLIP_ALL)
						return TARGET_TYPE::SECONDARY_TARGET;

					direction |= (CLIP_ALL | CLIP_SECONDARY);
				}
			}
		}

		if (boxNumber == LOT->TargetBox)
		{
			if (direction & (CLIP_LEFT | CLIP_RIGHT))
			{
				target->z = LOT->Target.z;
			}
			else if (!(direction & CLIP_SECONDARY))
			{
				if (target->z < (boxLeft + CLICK(2)))
					target->z = boxLeft + CLICK(2);
				else if (target->z > (boxRight - CLICK(2)))
					target->z = boxRight - CLICK(2);
			}

			if (direction & (CLIP_TOP | CLIP_BOTTOM))
			{
				target->x = LOT->Target.x;
			}
			else if (!(direction & CLIP_SECONDARY))
			{
				if (target->x < (boxTop + CLICK(2)))
					target->x = boxTop + CLICK(2);
				else if (target->x > (boxBottom - CLICK(2)))
					target->x = boxBottom - CLICK(2);
			}

			target->y = LOT->Target.y;
			return TARGET_TYPE::PRIME_TARGET;
		}

		boxNumber = LOT->Node[boxNumber].exitBox;
		if (boxNumber != NO_VALUE && (g_Level.PathfindingBoxes[boxNumber].flags & LOT->BlockMask))
			break;
	} while (boxNumber != NO_VALUE);

	if (!(direction & CLIP_SECONDARY))
	{
		if (target->z < (boxLeft + CLICK(2)))
			target->z = boxLeft + CLICK(2);
		else if (target->z > (boxRight - CLICK(2)))
			target->z = boxRight - CLICK(2);
	}

	if (!(direction & CLIP_SECONDARY))
	{
		if (target->x < (boxTop + CLICK(2)))
			target->x = boxTop + CLICK(2);
		else if (target->x > (boxBottom - CLICK(2)))
			target->x = boxBottom - CLICK(2);
	}

	if (LOT->Fly == NO_FLYING)
		target->y = box->height;
	else
		target->y = box->height - STEPUP_HEIGHT;

	return TARGET_TYPE::NO_TARGET;
}

void AdjustStopperFlag(ItemInfo* item, int direction)
{
	int x = item->Pose.Position.x;
	int z = item->Pose.Position.z;

	auto* room = &g_Level.Rooms[item->RoomNumber];
	auto* floor = GetSector(room, x - room->Position.x, z - room->Position.z);
	floor->Stopper = !floor->Stopper;

	x = item->Pose.Position.x + BLOCK(1) * phd_sin(direction);
	z = item->Pose.Position.z + BLOCK(1) * phd_cos(direction);
	room = &g_Level.Rooms[GetPointCollision(Vector3i(x, item->Pose.Position.y, z), item->RoomNumber).GetRoomNumber()];

	floor = GetSector(room, x - room->Position.x, z - room->Position.z);
	floor->Stopper = !floor->Stopper;
}

void InitializeItemBoxData()
{
	for (int i = 0; i < g_Level.Items.size(); i++)
	{
		auto* currentItem = &g_Level.Items[i];
	}

	for (auto& room : g_Level.Rooms)
	{
		for (const auto& mesh : room.mesh)
		{
			long index = ((mesh.pos.Position.z - room.Position.z) / BLOCK(1)) + room.ZSize * ((mesh.pos.Position.x - room.Position.x) / BLOCK(1));
			if (index >= room.Sectors.size())
				continue;

			auto* floor = &room.Sectors[index];
			if (floor->PathfindingBoxID == NO_VALUE)
				continue;

			if (!(g_Level.PathfindingBoxes[floor->PathfindingBoxID].flags & BLOCKED))
			{
				int floorHeight = floor->GetSurfaceHeight(mesh.pos.Position.x, mesh.pos.Position.z, true);
				const auto& bBox = GetBoundsAccurate(mesh, false);

				if (floorHeight <= mesh.pos.Position.y - bBox.Y2 + CLICK(2) &&
					floorHeight < mesh.pos.Position.y - bBox.Y1)
				{
					if (bBox.X1 == 0 || bBox.X2 == 0 || bBox.Z1 == 0 || bBox.Z2 == 0 ||
					  ((bBox.X1 < 0) ^ (bBox.X2 < 0)) && ((bBox.Z1 < 0) ^ (bBox.Z2 < 0)))
					{
						floor->Stopper = true;
					}
				}
			}
		}
	}
}

bool CanCreatureJump(ItemInfo& item, JumpDistance jumpDistType)
{
	const auto& creature = *GetCreatureInfo(&item);
	if (creature.Enemy == nullptr)
		return false;

	float stepDist = BLOCK(0.92f);

	int vPos = item.Pose.Position.y;
	auto pointCollA = GetPointCollision(item, item.Pose.Orientation.y, stepDist);
	auto pointCollB = GetPointCollision(item, item.Pose.Orientation.y, stepDist * 2);
	auto pointCollC = GetPointCollision(item, item.Pose.Orientation.y, stepDist * 3);

	switch (jumpDistType)
	{
	default:
	case JumpDistance::Block1:
		if (item.BoxNumber == creature.Enemy->BoxNumber ||
			vPos >= (pointCollA.GetFloorHeight() - STEPUP_HEIGHT) ||
			vPos >= (pointCollB.GetFloorHeight() + CLICK(1)) ||
			vPos <= (pointCollB.GetFloorHeight() - CLICK(1)) ||
			pointCollA.GetSector().PathfindingBoxID == NO_VALUE ||
			pointCollB.GetSector().PathfindingBoxID == NO_VALUE)
		{
			return false;
		}

		break;

	case JumpDistance::Block2:
		if (item.BoxNumber == creature.Enemy->BoxNumber ||
			vPos >= (pointCollA.GetFloorHeight() - STEPUP_HEIGHT) ||
			vPos >= (pointCollB.GetFloorHeight() - STEPUP_HEIGHT) ||
			vPos >= (pointCollC.GetFloorHeight() + CLICK(1)) ||
			vPos <= (pointCollC.GetFloorHeight() - CLICK(1)) ||
			pointCollA.GetSector().PathfindingBoxID == NO_VALUE ||
			pointCollB.GetSector().PathfindingBoxID == NO_VALUE ||
			pointCollC.GetSector().PathfindingBoxID == NO_VALUE)
		{
			return false;
		}

		break;
	}

	return true;
}
