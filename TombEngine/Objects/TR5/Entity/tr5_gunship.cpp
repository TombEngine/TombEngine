#include "framework.h"
#include "Objects/TR5/Entity/tr5_gunship.h"

#include "Scripting/Internal/TEN/Objects/Creature/Creature.h"
#include "Scripting/Internal/TEN/Properties/PropertyHandler.h"
#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Math/Geometry.h"
#include "Sound/sound.h"
#include "Specific/level.h"

#include "Game/misc.h"
#include "Game/effects/tomb4fx.h"

using namespace TEN::Animation;
using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR5
{
	constexpr short DEFAULT_FLY_UPDOWN_SPEED = BLOCK(4);
	constexpr short NO_FLYING = -1;

	void InitializeGunShip(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];
		InitializeCreature(itemNumber);
	}

	// Konstanten für Verhalten
constexpr int ROTOR_ACTIVE_THRESHOLD = 15;
constexpr int FIRE_RATE = 30;
constexpr int GUNSHIP_DAMAGE = 20; // Damage dealt by gunship to Lara when shooting

	constexpr float MOVEMENT_LERP_SPEED = 4.0f;
	constexpr int INERTIA_FRAMES = 25;
	constexpr float PITCH_LERP_SPEED = 5.0f;
	constexpr float BANK_LERP_SPEED = 3.0f;

	constexpr int MAX_PITCH_DEG = 20;
	constexpr int MAX_BANK_DEG = 15;
	constexpr float MAX_MOVE_SPEED = 64.0f;
	constexpr float FLY_UP_SPEED = 40.0f;
	constexpr float FLY_DOWN_SPEED = 40.0f;
	constexpr float VERTICAL_DODGE_SPEED = 50.0f;

	constexpr int SECTOR_SIZE = 1024;
	constexpr int FLOATING_POINT_SCALE = 1000;

	// Enum für Helikopter-Status
	enum class GunShipState : short
	{
		FOLLOW = 0,
		IDLE = 1,
		EVADE_NEAR = 2
	};

	// Helper: Bestimmt den aktuellen Status basierend auf Distanz und Kollisionen
	GunShipState DetermineGunShipState(const ItemInfo& item, float horizontalDistance, bool hasMoveTargetPos, bool blockedEarly)
	{
		int minDistance = (item.TriggerFlags > 0) ? item.TriggerFlags * SECTOR_SIZE : SECTOR_SIZE * 3;
		int maxShotsRange = minDistance + SECTOR_SIZE;

		if (hasMoveTargetPos)
		{
			return (horizontalDistance < 100.0f) ? GunShipState::IDLE : GunShipState::FOLLOW;
		}

		if (horizontalDistance < minDistance)
			return GunShipState::EVADE_NEAR;
		else if (horizontalDistance < maxShotsRange && item.ItemFlags[7] == 0)
			return GunShipState::IDLE;
		else if (!blockedEarly)
			return GunShipState::FOLLOW;

		// Wenn geblockt -> IDLE statt FOLLOW
		return GunShipState::IDLE;
	}

	// Helper: Kollisionsprüfung für early blocking (VOR State-Bestimmung!)
	bool CheckEarlyBlocking(const ItemInfo& item, float horizontalDistanceToTarget)
	{
		float currentSpeed = fabsf((float)item.ItemFlags[3] / FLOATING_POINT_SCALE);
		currentSpeed += ((horizontalDistanceToTarget > 0) ? MAX_MOVE_SPEED : MAX_MOVE_SPEED * 0.25f - currentSpeed) * (1.0f / powf(2.0f, MOVEMENT_LERP_SPEED));
		bool isMoving = currentSpeed > 1.0f;

		if (!isMoving)
			return false;

		Vector3 forwardVec(
			-sinf(item.Pose.Orientation.y),
			0.0f,
			-cosf(item.Pose.Orientation.y));
		forwardVec.Normalize();

		auto& heliFrame = GetFrame(item);
		float bottomMeshY = item.Pose.Position.y + heliFrame.BoundingBox.Y1;
		float topMeshY = item.Pose.Position.y + heliFrame.BoundingBox.Y2;

		const float safetyDistance = SECTOR_SIZE;
		Vector3 checkPoint = item.Pose.Position.ToVector3() + forwardVec * safetyDistance;

		auto pointCollCenter = GetPointCollision(Vector3(checkPoint.x, (bottomMeshY + topMeshY) / 2, checkPoint.z), item.RoomNumber);

		if (pointCollCenter.GetSector().IsWall(checkPoint.x, checkPoint.z))
			return true;

		int relCeilHeight = abs(pointCollCenter.GetCeilingHeight() - pointCollCenter.GetFloorHeight());
		if (relCeilHeight <= (int)(topMeshY - bottomMeshY))
			return true;

		const float sideOffset = SECTOR_SIZE / 2;
		Vector3 leftPoint = checkPoint + Vector3(forwardVec.z, 0.0f, -forwardVec.x) * sideOffset;
		Vector3 rightPoint = checkPoint + Vector3(-forwardVec.z, 0.0f, forwardVec.x) * sideOffset;

		auto pointCollLeft = GetPointCollision(Vector3(leftPoint.x, (bottomMeshY + topMeshY) / 2, leftPoint.z), item.RoomNumber);
		auto pointCollRight = GetPointCollision(Vector3(rightPoint.x, (bottomMeshY + topMeshY) / 2, rightPoint.z), item.RoomNumber);

		if (pointCollLeft.GetSector().IsWall(leftPoint.x, leftPoint.z))
			return true;

		if (pointCollRight.GetSector().IsWall(rightPoint.x, rightPoint.z))
			return true;

		return false;
	}

	// Helper: Berechnet die Distanz und Richtung zum Ziel
	struct GunShipTargetInfo
	{
		float horizontalDistance = 0.0f;
		float verticalDifference = 0.0f;
		Vector3 targetPos;
		bool hasMoveTarget = false;

		void Calculate(ItemInfo* item, ItemInfo* moveTargetItem, const Vector3& moveTargetPos)
		{
			if (moveTargetPos != Vector3::Zero)
			{
				targetPos = moveTargetPos;
				hasMoveTarget = true;
			}
			else
			{
				targetPos = moveTargetItem->Pose.Position.ToVector3();
				hasMoveTarget = false;
			}

			float dx = targetPos.x - item->Pose.Position.x;
			float dz = targetPos.z - item->Pose.Position.z;
			horizontalDistance = sqrtf(dx * dx + dz * dz);

			verticalDifference = item->Pose.Position.y - targetPos.y;
		}
	};

	// Helper: Prüft Kollisionen vor dem Helikopter
	bool CheckForwardCollision(const ItemInfo& item, float& outTargetSpeed)
	{
		Vector3 forwardVec(
			-sinf(item.Pose.Orientation.y),
			0.0f,
			-cosf(item.Pose.Orientation.y));
		forwardVec.Normalize();

		auto& heliFrame = GetFrame(item);
		float bottomMeshY = item.Pose.Position.y + heliFrame.BoundingBox.Y1;
		float topMeshY = item.Pose.Position.y + heliFrame.BoundingBox.Y2;

		const float earlyCheckDistance = SECTOR_SIZE * 3;
		Vector3 earlyCheckPoint = item.Pose.Position.ToVector3() + forwardVec * earlyCheckDistance;
		auto pointCollEarly = GetPointCollision(Vector3(earlyCheckPoint.x, (bottomMeshY + topMeshY) / 2, earlyCheckPoint.z), item.RoomNumber);

		if (pointCollEarly.GetSector().IsWall(earlyCheckPoint.x, earlyCheckPoint.z))
		{
			outTargetSpeed *= 0.5f;
			return false;
		}

		int relCeilHeight = abs(pointCollEarly.GetCeilingHeight() - pointCollEarly.GetFloorHeight());
		if (relCeilHeight <= (int)(topMeshY - bottomMeshY))
		{
			outTargetSpeed *= 0.5f;
			return false;
		}

		const float safetyDistance = SECTOR_SIZE;
		Vector3 checkPoint = item.Pose.Position.ToVector3() + forwardVec * safetyDistance;

		short roomNum = pointCollEarly.GetRoomNumber();
		auto pointCollCenter = GetPointCollision(Vector3(checkPoint.x, (bottomMeshY + topMeshY) / 2, checkPoint.z), roomNum);

		if (pointCollCenter.GetSector().IsWall(checkPoint.x, checkPoint.z))
			return true;

		relCeilHeight = abs(pointCollCenter.GetCeilingHeight() - pointCollCenter.GetFloorHeight());
		if (relCeilHeight <= (int)(topMeshY - bottomMeshY))
			return true;

		const float sideOffset = SECTOR_SIZE / 2;
		Vector3 leftPoint = checkPoint + Vector3(forwardVec.z, 0.0f, -forwardVec.x) * sideOffset;
		Vector3 rightPoint = checkPoint + Vector3(-forwardVec.z, 0.0f, forwardVec.x) * sideOffset;

		auto pointCollLeft = GetPointCollision(Vector3(leftPoint.x, (bottomMeshY + topMeshY) / 2, leftPoint.z), roomNum);
		auto pointCollRight = GetPointCollision(Vector3(rightPoint.x, (bottomMeshY + topMeshY) / 2, rightPoint.z), roomNum);

		if (pointCollLeft.GetSector().IsWall(leftPoint.x, leftPoint.z))
			return true;

		if (pointCollRight.GetSector().IsWall(rightPoint.x, rightPoint.z))
			return true;

		return false;
	}

	// Helper: Setzt Y-Position basierend auf Decke/Boden
	void FixYPosition(ItemInfo* item, float currentYSpeed = 0.0f)
	{
		auto& frameData = GetFrame(*item);
		float bottomMeshY = item->Pose.Position.y + frameData.BoundingBox.Y1;
		float topMeshY = item->Pose.Position.y + frameData.BoundingBox.Y2;

		FloorInfo* floorCheck = GetFloor(item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z, &item->RoomNumber);
		if (floorCheck != nullptr)
		{
			const int floorHeight = GetFloorHeight(floorCheck, item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z);
			const int ceilingHeight = GetCeiling(floorCheck, item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z);

			// Mindestabstand zum Boden garantieren (NIEMALS im Boden versinken!)
			if (floorHeight != NO_VALUE)
			{
				const int minGroundClearance = SECTOR_SIZE / 4; // 256 units = 0.25 BLOCK
				// Wenn Boden des Helis zu tief liegt -> nach oben korrigieren
				if (bottomMeshY > floorHeight - minGroundClearance)
					item->Pose.Position.y = floorHeight - frameData.BoundingBox.Y1 - minGroundClearance;
			}

			// Mindestabstand zur Decke garantieren
			if (ceilingHeight != NO_VALUE)
			{
				const int minCeilingClearance = SECTOR_SIZE / 8; // 128 units = 0.125 BLOCK
				if (topMeshY < ceilingHeight + minCeilingClearance)
					item->Pose.Position.y = ceilingHeight + minCeilingClearance - frameData.BoundingBox.Y2;
			}
		}
	}

	// Helper: Aktualisiert Y-Bewegung und Orientierung im IDLE-State
	void UpdateIdleOrientation(ItemInfo* item, const Vector3& targetPos, float ySpeedTargetGlobal, float& pitchTarget, float& bankTarget)
	{
		if (ySpeedTargetGlobal == 0.0f)
			return;

		auto fwdVec = Vector3(
			cosf(item->Pose.Orientation.x) * sinf(item->Pose.Orientation.y),
			sinf(item->Pose.Orientation.x),
			cosf(item->Pose.Orientation.x) * cosf(item->Pose.Orientation.y));
		fwdVec.Normalize();

		Vector3 toTarget = targetPos - item->Pose.Position.ToVector3();
		toTarget.y = 0.0f;
		toTarget.Normalize();

		float crossY = fwdVec.z * toTarget.x - fwdVec.x * toTarget.z;

		// Pitch basierend auf Vorwärts/Rückwärtsbewegung
		float dotForward = fwdVec.Dot(toTarget);
		if (dotForward < 0.0f)
			pitchTarget = -(float)DEG_TO_RAD(MAX_PITCH_DEG); // Rückwärtsfliegen -> nach hinten kippen
		else
			pitchTarget = (float)DEG_TO_RAD(MAX_PITCH_DEG); // Vorwärtsfliegen -> nach vorne kippen

		bankTarget = DEG_TO_RAD(MAX_BANK_DEG) * crossY;
	}

	// Helper: Berechnet Pitch/Bank für den aktuellen Status
	void CalculatePitchAndBank(const ItemInfo& item, GunShipState state, const Vector3& targetPos, float yDodgingFactor, float& pitchTarget, float& bankTarget)
	{
		switch (state)
		{
		case GunShipState::FOLLOW:
			pitchTarget = (float)DEG_TO_RAD(MAX_PITCH_DEG);
			break;
		case GunShipState::EVADE_NEAR:
			pitchTarget = -(float)DEG_TO_RAD(MAX_PITCH_DEG);
			break;
		default:
			pitchTarget = 0.0f;
			return;
		}

		auto fwdVec = Vector3(
			cosf(item.Pose.Orientation.x) * sinf(item.Pose.Orientation.y),
			sinf(item.Pose.Orientation.x),
			cosf(item.Pose.Orientation.x) * cosf(item.Pose.Orientation.y));
		fwdVec.Normalize();

		Vector3 toTarget = targetPos - item.Pose.Position.ToVector3();
		toTarget.y = 0.0f;
		toTarget.Normalize();

		float crossY = fwdVec.z * toTarget.x - fwdVec.x * toTarget.z;

		if (state == GunShipState::EVADE_NEAR && yDodgingFactor != 0.0f)
			bankTarget = DEG_TO_RAD(MAX_BANK_DEG) * crossY;
		else
			bankTarget = DEG_TO_RAD(MAX_BANK_DEG) * crossY;
	}

	void ControlGunShip(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		if (!TriggerActive(item))
			return;

		if (!CreatureActive(itemNumber))
			return;

		SoundEffect(SFX_TR4_HELICOPTER_LOOP, &item->Pose);

		const PropertyValue* shootProp = PropertyHandler::Get(*item, "GunshipShootTarget");
		int shootTargetNum = LaraItem->Index;//-1; Zum Testen auf Lara gestellt
		if (shootProp != nullptr)
		{
			auto val = ExtractValue<int>(*shootProp);
			if (val.has_value() && val.value() >= 0)
				shootTargetNum = val.value();
		}

		bool hasShootTarget = (shootTargetNum >= 0);

		const PropertyValue* moveProp = PropertyHandler::Get(*item, "GunshipMovementTarget");
		Vector3 moveTargetPos = Vector3::Zero;
		bool hasMoveTargetPos = false;

		if (moveProp != nullptr)
		{
			auto val = ExtractValue<Vec3>(*moveProp);
			if (val.has_value())
			{
				moveTargetPos = val->ToVector3();
				hasMoveTargetPos = true;
			}
		}

		ItemInfo* moveTargetItem = hasMoveTargetPos ? nullptr : LaraItem.Get();
		if (!hasMoveTargetPos && hasShootTarget)
			moveTargetItem = &g_Level.Items[shootTargetNum];

		int minDistance = (item->TriggerFlags > 0) ? item->TriggerFlags * SECTOR_SIZE : SECTOR_SIZE * 3;
		int maxShotsRange = minDistance + SECTOR_SIZE;

		GunShipTargetInfo targetInfo;
		targetInfo.Calculate(item, moveTargetItem, moveTargetPos);

		float yDiff = targetInfo.verticalDifference;
		float horizontalDist = targetInfo.horizontalDistance;

		bool blockedEarly = false;
		if (!hasMoveTargetPos)
			blockedEarly = CheckEarlyBlocking(*item, horizontalDist);

		GunShipState currentState = DetermineGunShipState(*item, horizontalDist, hasMoveTargetPos, blockedEarly);

		int prevStates = item->ItemFlags[4];
		int inertiaTimer = item->ItemFlags[5];

		if (prevStates != (int)currentState && item->ItemFlags[7] == 0)
		{
			inertiaTimer = INERTIA_FRAMES;
			item->ItemFlags[4] = (int)currentState;
			item->ItemFlags[5] = INERTIA_FRAMES;
		}

		if (currentState == GunShipState::EVADE_NEAR && item->ItemFlags[7] == 0 && horizontalDist < SECTOR_SIZE * 3 && yDiff >= -SECTOR_SIZE * 6)
			item->ItemFlags[7] = 1;

		if (item->ItemFlags[7] == 1 && horizontalDist > maxShotsRange * 1.5f && currentState != GunShipState::EVADE_NEAR)
		{
			item->ItemFlags[7] = 2;
			inertiaTimer = 0;
			item->ItemFlags[5] = 0;
		}

		if (item->ItemFlags[7] == 2 && fabsf(yDiff) < SECTOR_SIZE * 0.5f)
		{
			item->ItemFlags[7] = 0;
			inertiaTimer = 0;
			item->ItemFlags[5] = 0;
		}

		if (item->ItemFlags[7] == 1)
			currentState = GunShipState::EVADE_NEAR;

		float currentYSpeed = (float)item->ItemFlags[6] / FLOATING_POINT_SCALE;
		const float yLerpAlpha = 1.0f / powf(2.0f, MOVEMENT_LERP_SPEED);
		const int minYDiff = SECTOR_SIZE;

	// TargetSpeed basierend auf State berechnen (immer, nicht nur wenn kein Inertie!)
	float targetSpeed = 0.0f;
	switch (currentState)
	{
	case GunShipState::FOLLOW:
		targetSpeed = hasMoveTargetPos ? MAX_MOVE_SPEED : (horizontalDist > maxShotsRange) ? MAX_MOVE_SPEED : MAX_MOVE_SPEED * 0.25f;
		break;
	case GunShipState::IDLE:
		targetSpeed = 0.0f;

		{
			Vector3 targetPosIdle = targetInfo.targetPos;
			
			float yDiffIdle = item->Pose.Position.y - targetPosIdle.y;
			float ySpeedTargetIdle = 0.0f;
				if (fabsf(yDiffIdle) > minYDiff)
					ySpeedTargetIdle = (targetPosIdle.y > item->Pose.Position.y) ? FLY_DOWN_SPEED : -FLY_UP_SPEED;

				currentYSpeed += (ySpeedTargetIdle - currentYSpeed) * yLerpAlpha;
			}
		break;
	case GunShipState::EVADE_NEAR:
		targetSpeed = MAX_MOVE_SPEED * 2.5f;
		break;
	}

	// Inertie anwenden - reduziert targetSpeed bei Stateswitch
	if (inertiaTimer > 0)
	{
		targetSpeed *= 0.15f;
		inertiaTimer--;
		item->ItemFlags[5] = inertiaTimer;
	}

	// Pitch aus ItemFlags[1]
	float currentPitch = (float)item->ItemFlags[1] / FLOATING_POINT_SCALE;

	// pitchRatio für Geschwindigkeit berechnen
	float pitchRatio = fabsf(currentPitch) / ((float)MAX_PITCH_DEG * DEG_TO_RAD(1.0f));

	// currentSpeed aus targetSpeed und pitchRatio
	float currentSpeed = targetSpeed * pitchRatio;

	bool isMoving = currentSpeed > 1.0f;

	EulerAngles targetOrient;
	if (hasShootTarget && shootTargetNum >= 0)
		targetOrient = Geometry::GetOrientToPoint(item->Pose.Position.ToVector3(), g_Level.Items[shootTargetNum].Pose.Position.ToVector3());
	else
		targetOrient = Geometry::GetOrientToPoint(item->Pose.Position.ToVector3(), targetInfo.targetPos);

	// Pitch- und Bank-Zielwerte berechnen (IMMER, auch wenn noch nicht bewegt wird!)
	float pitchTarget = 0.0f;
	float bankTarget = 0.0f;
	switch (currentState)
	{
	case GunShipState::FOLLOW:
		pitchTarget = (float)DEG_TO_RAD(MAX_PITCH_DEG);
		CalculatePitchAndBank(*item, currentState, targetInfo.targetPos, 0.0f, pitchTarget, bankTarget);
		break;
	case GunShipState::EVADE_NEAR:
		pitchTarget = -(float)DEG_TO_RAD(MAX_PITCH_DEG);
		CalculatePitchAndBank(*item, currentState, targetInfo.targetPos, 0.0f, pitchTarget, bankTarget);
		break;
	default:
		pitchTarget = 0.0f;
		bankTarget = 0.0f;
		break;
	}

	bool blocked = false;
	if (isMoving)
		blocked = CheckForwardCollision(*item, currentSpeed);

	if (blocked)
		{
			currentSpeed = 0.0f;
			item->ItemFlags[3] = 0;

			currentState = GunShipState::IDLE;
			
			// Position leicht rückwärts verschieben um aus der Kollision herauszukommen
			if (horizontalDist > 1.0f)
			{
				float dx = targetInfo.targetPos.x - item->Pose.Position.x;
				float dz = targetInfo.targetPos.z - item->Pose.Position.z;
				item->Pose.Position.x -= (int)((dx / horizontalDist) * 32);
				item->Pose.Position.z -= (int)((dz / horizontalDist) * 32);
			}

			FixYPosition(item);

			pitchTarget = 0.0f;
			bankTarget = 0.0f;

			// Animation trotz blocked status fortsetzen
			AnimateItem(item);
			
			// Kollisionsprüfung nach Ausweichbewegung um zu prüfen ob freie Sicht vorliegt
			float dummySpeed = MAX_MOVE_SPEED * 0.25f;
			blocked = CheckForwardCollision(*item, dummySpeed);
			
			if (!blocked)
			{
				// Kollision behoben -> in IDLE warten bis Ziel wieder in Reichweite
				currentState = GunShipState::IDLE;
				item->ItemFlags[3] = 0; // targetSpeed = 0
			}
			else
			{
				// Noch immer geblockt -> keine Bewegung
				currentState = GunShipState::IDLE;
				currentSpeed = 0.0f;
				item->ItemFlags[3] = 0;
			}
		}

		if (!blocked)
		{
			// Pitch und Bank immer berechnen (auch wenn noch nicht bewegt wird!)
			switch (currentState)
			{
			case GunShipState::FOLLOW:
				CalculatePitchAndBank(*item, currentState, targetInfo.targetPos, 0.0f, pitchTarget, bankTarget);
				break;
			case GunShipState::EVADE_NEAR:
				pitchTarget = -(float)DEG_TO_RAD(MAX_PITCH_DEG);
				CalculatePitchAndBank(*item, currentState, targetInfo.targetPos, 0.0f, pitchTarget, bankTarget);
				break;
			default:
				pitchTarget = 0.0f;
				bankTarget = 0.0f;
				break;
			}

			// Bewegung nur ausführen wenn isMoving
			if (isMoving)
			{
				float moveDist = currentSpeed;

				switch (currentState)
				{
				case GunShipState::FOLLOW:
					if (horizontalDist > 1.0f)
					{
						item->Pose.Position.x += (int)((targetInfo.targetPos.x - item->Pose.Position.x) / horizontalDist * moveDist);
						item->Pose.Position.z += (int)((targetInfo.targetPos.z - item->Pose.Position.z) / horizontalDist * moveDist);
					}

					if (item->ItemFlags[7])
					{
						float targetY = hasMoveTargetPos ? moveTargetPos.y : moveTargetItem->Pose.Position.y;
						if (fabsf(item->Pose.Position.y - targetY) < SECTOR_SIZE)
							item->ItemFlags[7] = 0;
					}

					break;

				case GunShipState::EVADE_NEAR:
					if (horizontalDist > 1.0f)
					{
						item->Pose.Position.x -= (int)((targetInfo.targetPos.x - item->Pose.Position.x) / horizontalDist * moveDist);
						item->Pose.Position.z -= (int)((targetInfo.targetPos.z - item->Pose.Position.z) / horizontalDist * moveDist);
					}

					break;
				}

				if (currentState != GunShipState::IDLE)
				{
					short newRoomNum = GetPointCollision(item->Pose.Position, item->RoomNumber).GetRoomNumber();
					if (newRoomNum != item->RoomNumber)
						ItemNewRoom(itemNumber, newRoomNum);
				}
			}
		}

		if (!blocked)
		{
			item->Pose.Position.y += (int)currentYSpeed;

			FixYPosition(item);

			UpdateIdleOrientation(item, targetInfo.targetPos, currentYSpeed, pitchTarget, bankTarget);

			if (!isMoving && currentState == GunShipState::IDLE && currentYSpeed != 0.0f)
				UpdateIdleOrientation(item, targetInfo.targetPos, currentYSpeed, pitchTarget, bankTarget);

			const float pitchLerpAlpha = 1.0f / powf(2.0f, PITCH_LERP_SPEED);
			const float bankLerpAlpha = 1.0f / powf(2.0f, BANK_LERP_SPEED);

			float currentPitch = (float)item->ItemFlags[1] / FLOATING_POINT_SCALE;
			float currentBankAngle = (float)item->ItemFlags[2] / FLOATING_POINT_SCALE;

			currentPitch += (pitchTarget - currentPitch) * pitchLerpAlpha;
			currentBankAngle += (bankTarget - currentBankAngle) * bankLerpAlpha;

			item->ItemFlags[1] = (int)(currentPitch * FLOATING_POINT_SCALE);
			item->ItemFlags[2] = (int)(currentBankAngle * FLOATING_POINT_SCALE);

			if (!isMoving)
			{
				currentPitch *= 0.95f;
				currentBankAngle *= 0.95f;
				item->ItemFlags[1] = (int)(currentPitch * FLOATING_POINT_SCALE);
				item->ItemFlags[2] = (int)(currentBankAngle * FLOATING_POINT_SCALE);

				if (fabsf(currentYSpeed) > 0.1f)
					currentYSpeed += (-currentYSpeed) * 0.1f; // Ziel ist 0
				else
					currentYSpeed *= 0.95f;
			}

			float lerpAlpha = 1.0f / powf(2.0f, 3);
			if (item->ItemFlags[0] == 1)
				lerpAlpha = 1.0f;

			EulerAngles orientResult = targetOrient;
			orientResult.y += ANGLE(180.0f);

			constexpr float RAD_TO_SHORTS = (float)(65536.0 / (2.0 * PI));
			orientResult.x = (short)(currentPitch * RAD_TO_SHORTS);
			orientResult.z = (short)(currentBankAngle * RAD_TO_SHORTS);

			item->Pose.Orientation = EulerAngles::Lerp(item->Pose.Orientation, orientResult, lerpAlpha);

			if (GetFloor(item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z, &item->RoomNumber) != nullptr)
				GetCeiling(GetFloor(item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z, &item->RoomNumber), item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z);

			item->ItemFlags[0]++;

			CollisionInfo coll{};
			auto collObjects = GetCollidedObjects(*item, true, true);

			if (!collObjects.Statics.empty())
			{
				for (const StaticMesh* staticMesh : collObjects.Statics)
					ItemPushStatic(item, *staticMesh, &coll);
			}

			int shootHdx = 0.0f, shootHdz = 0.0f, shootHLen = 0.0f;
			if (hasShootTarget && shootTargetNum >= 0)
			{
				shootHdx = g_Level.Items[shootTargetNum].Pose.Position.x - item->Pose.Position.x;
				shootHdz = g_Level.Items[shootTargetNum].Pose.Position.z - item->Pose.Position.z;
				shootHLen = sqrtf(shootHdx * shootHdx + shootHdz * shootHdz);
			}

			const bool hasShootTargetInRange = hasShootTarget && shootHLen <= maxShotsRange;
if (hasShootTargetInRange)
{
    // Sound for gunfire.
    if (!(GlobalCounter & (FIRE_RATE - 1)) && item->ItemFlags[0] > FIRE_RATE)
        SoundEffect(SFX_TR4_HK_FIRE, &item->Pose, SoundEnvironment::Land, 0.8f);

    // Gun flash visual and light (always shown when firing).
    if (item->ItemFlags[0] <= ROTOR_ACTIVE_THRESHOLD)
        item->MeshBits |= 0x100;
    else
        item->MeshBits &= 0xFEFF;

    // Use mesh‑8 (gun neck) joint as muzzle point.
auto muzzleJoint = GetJointPosition(item, 8, Vector3i::Zero);
    auto flashPos   = muzzleJoint.ToVector3();

    auto lightColor = Vector3(Random::GenerateFloat(0.75f, 0.85f),
                              Random::GenerateFloat(0.5f, 0.6f), 0.0f) * 255;
    SpawnDynamicLight(flashPos.x, flashPos.y, flashPos.z,
                      10, lightColor.x, lightColor.y, lightColor.z);

    // Spawn gun shell effect at the muzzle (same as Lara’s).
    TriggerGunShell(0, ID_GUNSHELL, LaraWeaponType::HK);

    // Determine line of sight from the muzzle.
    auto origin   = GameVector(flashPos, item->RoomNumber);
    auto targetVec = GameVector(g_Level.Items[shootTargetNum].Pose.Position,
                               g_Level.Items[shootTargetNum].RoomNumber);
    bool los = LOS(&origin, &targetVec);

    if (los)
    {
        // Apply damage with a miss chance (50% hit probability).
        if (Random::TestProbability(0.5f))
            DoDamage(&g_Level.Items[shootTargetNum], GUNSHIP_DAMAGE);
    }
    else
    {
        // No line of sight – create ricochet spark at impact point.
        int dx = targetVec.x - origin.x;
        int dz = targetVec.z - origin.z;

        // Clamp distance to a reasonable length for the spark.
        while (abs(dx) > BLOCK(12) || abs(dz) > BLOCK(12))
        {
            dx /= 2;
            dz /= 2;
        }

        GameVector impactPos(
            origin.x + dx + GetRandomControl() - 128,
            origin.y,
            origin.z + dz + GetRandomControl() - 128,
            item->RoomNumber);

        TriggerRicochetSpark(impactPos, Random::GenerateAngle());
    }
}
else
{
    // Not in range – ensure flash is cleared.
    item->MeshBits &= 0xFEFF;
}

			if (item->ItemFlags[7] != 1)
				item->ItemFlags[6] = (int)(currentYSpeed * FLOATING_POINT_SCALE);
			else
				item->ItemFlags[6] = (int)currentYSpeed;

			AnimateItem(item);
		}
}
}
