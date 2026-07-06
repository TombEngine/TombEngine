#include "framework.h"
#include "Objects/TR5/Entity/tr5_gunship.h"

#include <unordered_map>
#include <algorithm>

#include "Scripting/Internal/TEN/Objects/Creature/Creature.h"
#include "Scripting/Internal/TEN/Properties/PropertyHandler.h"
#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/effects/debris.h"
#include "Game/effects/effects.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Game/Lara/lara.h"
#include "Math/Geometry.h"
#include "Objects/Generic/Object/objects.h"
#include "Sound/sound.h"
#include "Specific/level.h"

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

	constexpr int ROTOR_ACTIVE_THRESHOLD = 15;
	constexpr int FIRE_RATE = 30;

	constexpr float MOVEMENT_LERP_SPEED = 4.0f;
	constexpr int INERTIA_FRAMES = 25;
	constexpr float YAW_LERP_SPEED = 3.0f;
	constexpr float PITCH_LERP_SPEED = 5.0f;
	constexpr float BANK_LERP_SPEED = 3.0f;

	constexpr int MAX_PITCH_DEG = 20;
	constexpr int MAX_BANK_DEG = 15;
	constexpr float MAX_MOVE_SPEED = 214.0f;
	constexpr float FLY_UP_SPEED = 40.0f;
	constexpr float FLY_DOWN_SPEED = 40.0f;
	constexpr float VERTICAL_DODGE_SPEED = 50.0f;
	constexpr int SECTOR_SIZE = 1024;
	constexpr int FLOATING_POINT_SCALE = 1000;

	enum GunShipState
	{
		FOLLOW = 0,
		IDLE = 1,
		EVADE_NEAR = 2
	};

	struct GunShipTargets
	{
		int shootTargetItemNum = -1;
		bool hasShootTarget = false;
		Vector3 movementTargetPos = Vector3::Zero;
		bool hasMovementTargetPos = false;
	};

	GunShipTargets ResolveGunShipTargets(const ItemInfo& item)
	{
		GunShipTargets targets{};

		const PropertyValue* shootProp = PropertyHandler::Get(item, "GunshipShootTarget");

		if (shootProp != nullptr)
		{
			auto val = ExtractValue<int>(*shootProp);
			
			if (val.has_value() && val.value() >= 0)
			{

			}
		}

		auto val = LaraItem->Index;
		targets.shootTargetItemNum = val;
		targets.hasShootTarget = true;

		auto moveProp = PropertyHandler::Get(item, "GunshipMovementTarget");
		if (moveProp != nullptr)
		{
			auto val = ExtractValue<Vec3>(*moveProp);
			if (val.has_value())
			{
				targets.movementTargetPos = val->ToVector3();
				targets.hasMovementTargetPos = true;
			}
		}

		return targets;
	}


	void ControlGunShip(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		if (!TriggerActive(item))
			return;

		if (!CreatureActive(itemNumber))
			return;

		SoundEffect(SFX_TR4_HELICOPTER_LOOP, &item->Pose);

		auto& creature = *GetCreatureInfo(item);

		// Property-basierte Targets auflösen
		const GunShipTargets targets = ResolveGunShipTargets(*item);

		AnimateItem(item);

		// Kein ShootTarget und kein MoveTarget: Heli untätig
		if (!targets.hasShootTarget && !targets.hasMovementTargetPos)
			return;

		const int* shootTargetNum = (targets.hasShootTarget && targets.shootTargetItemNum >= 0) ? &targets.shootTargetItemNum : nullptr;

		// Schuss-Target und Move-Target Item bestimmen
		ItemInfo* moveTargetItem = LaraItem;
		Vector3 moveTargetPosCopy = Vector3::Zero;
		bool hasMoveTargetPos = targets.hasMovementTargetPos;
		if (hasMoveTargetPos)
			moveTargetPosCopy = targets.movementTargetPos;

		ItemInfo* shootTargetItem = shootTargetNum ? &g_Level.Items[*shootTargetNum] : nullptr;

		// Wenn kein MoveTarget gesetzt, aber ein ShootTarget: Bewege uns zum ShootTarget
		if (!hasMoveTargetPos && targets.hasShootTarget)
			moveTargetItem = &g_Level.Items[targets.shootTargetItemNum];

		// Mindestabstand aus TriggerFlags berechnen (nur ohne MovementTarget relevant)
		const int minDistance = (item->TriggerFlags > 0) ? item->TriggerFlags * SECTOR_SIZE : SECTOR_SIZE * 3;
		const int maxShotsRange = minDistance + SECTOR_SIZE;

		// Horizontale Distanz zum Move-Target (für Bewegung/State)
		float moveHdx, moveHdz;
		float moveHLen;
		if (hasMoveTargetPos)
		{
			moveHdx = moveTargetPosCopy.x - item->Pose.Position.x;
			moveHdz = moveTargetPosCopy.z - item->Pose.Position.z;
		}
		else
		{
			moveHdx = moveTargetItem->Pose.Position.x - item->Pose.Position.x;
			moveHdz = moveTargetItem->Pose.Position.z - item->Pose.Position.z;
		}
		moveHLen = sqrtf(moveHdx * moveHdx + moveHdz * moveHdz);

		// Horizontale Distanz zum Shoot-Target (für Schussreichweite)
		float shootHdx = 0.0f, shootHdz = 0.0f, shootHLen = 0.0f;
		if (targets.hasShootTarget)
		{
			shootHdx = shootTargetItem->Pose.Position.x - item->Pose.Position.x;
			shootHdz = shootTargetItem->Pose.Position.z - item->Pose.Position.z;
			shootHLen = sqrtf(shootHdx * shootHdx + shootHdz * shootHdz);
		}

		// Vertikaler Abstand zum Move-Target
		float moveTargetY = (hasMoveTargetPos) ? moveTargetPosCopy.y : moveTargetItem->Pose.Position.y;
		const float yDiff = item->Pose.Position.y - moveTargetY;

		// Kollisionsprüfung für State-Bestimmung (VOR State-Bestimmung!)
		bool blockedEarly = false;
		if (!hasMoveTargetPos)
		{
			float currentSpeed = fabsf((float)item->ItemFlags[3] / FLOATING_POINT_SCALE);
			currentSpeed += ((moveHLen > maxShotsRange) ? MAX_MOVE_SPEED : MAX_MOVE_SPEED * 0.25f - currentSpeed) * (1.0f / powf(2.0f, MOVEMENT_LERP_SPEED));
			bool isMoving = currentSpeed > 1.0f;

			if (isMoving && moveHLen > 100.0f)
			{
				Vector3 forwardVec(
					-sinf(item->Pose.Orientation.y),
					0.0f,
					-cosf(item->Pose.Orientation.y));
				forwardVec.Normalize();

				auto& heliFrame = GetFrame(*item);
				float bottomMeshY = item->Pose.Position.y + heliFrame.BoundingBox.Y1;
				float topMeshY = item->Pose.Position.y + heliFrame.BoundingBox.Y2;

				const float safetyDistance = SECTOR_SIZE;
				Vector3 checkPoint = item->Pose.Position.ToVector3() + forwardVec * safetyDistance;

				auto pointCollCenter = GetPointCollision(Vector3(checkPoint.x, (bottomMeshY + topMeshY) / 2, checkPoint.z), item->RoomNumber);

				if (pointCollCenter.GetSector().IsWall(checkPoint.x, checkPoint.z))
					blockedEarly = true;

				int relCeilHeight = abs(pointCollCenter.GetCeilingHeight() - pointCollCenter.GetFloorHeight());
				if (!blockedEarly && relCeilHeight <= (int)(topMeshY - bottomMeshY))
					blockedEarly = true;

				const float sideOffset = SECTOR_SIZE / 2;
				Vector3 leftPoint = checkPoint + Vector3(forwardVec.z, 0.0f, -forwardVec.x) * sideOffset;
				Vector3 rightPoint = checkPoint + Vector3(-forwardVec.z, 0.0f, forwardVec.x) * sideOffset;

				auto pointCollLeft = GetPointCollision(Vector3(leftPoint.x, (bottomMeshY + topMeshY) / 2, leftPoint.z), item->RoomNumber);
				auto pointCollRight = GetPointCollision(Vector3(rightPoint.x, (bottomMeshY + topMeshY) / 2, rightPoint.z), item->RoomNumber);

				if (pointCollLeft.GetSector().IsWall(leftPoint.x, leftPoint.z))
					blockedEarly = true;

				if (!blockedEarly && pointCollRight.GetSector().IsWall(rightPoint.x, rightPoint.z))
					blockedEarly = true;
			}
		}

		// Zustand bestimmen: Bei MovementTarget keine EVADE_NEAR basierend auf Distanz
		int currentState = -1;

		if (hasMoveTargetPos)
		{
			// Mit MovementTarget: Immer FOLLOW bis Punkt erreicht, dann IDLE
			currentState = (moveHLen < 100.0f) ? GunShipState::IDLE : GunShipState::FOLLOW;
		}
		else
		{
			if (moveHLen < minDistance)
				currentState = GunShipState::EVADE_NEAR;
			else if (moveHLen < maxShotsRange && item->ItemFlags[7] == 0)
				currentState = GunShipState::IDLE;
			else if (!blockedEarly)
				currentState = GunShipState::FOLLOW;
		}

		// Trägheit: Wenn sich der Zustand geändert hat, Timer starten
		int prevStates = item->ItemFlags[4];
		int inertiaTimer = item->ItemFlags[5];
		if (prevStates != currentState && item->ItemFlags[7] == 0)
		{
			inertiaTimer = INERTIA_FRAMES;
			item->ItemFlags[4] = currentState;
			item->ItemFlags[5] = INERTIA_FRAMES;
		}

		// Vertikales Ausweich-Flag: ItemFlags[7] = isDodgingUp (1=hochweichen, 2=herunterkommen)
		if (currentState == GunShipState::EVADE_NEAR && item->ItemFlags[7] == 0 && moveHLen < SECTOR_SIZE * 3 && yDiff >= -SECTOR_SIZE * 6)
			item->ItemFlags[7] = 1;

		if (item->ItemFlags[7] == 1 && moveHLen > maxShotsRange * 1.5f && currentState != GunShipState::EVADE_NEAR)
		{
			item->ItemFlags[7] = 2; // Runterkommen starten
			inertiaTimer = 0;
			item->ItemFlags[5] = 0;
		}

		if (item->ItemFlags[7] == 2 && fabsf(yDiff) < SECTOR_SIZE * 0.5f)
		{
			item->ItemFlags[7] = 0; // Zielhöhe erreicht
			inertiaTimer = 0;
			item->ItemFlags[5] = 0;
		}

		if (item->ItemFlags[7] == 1)
			currentState = GunShipState::EVADE_NEAR;

		// Beschleunigung/Verzögerung über ItemFlags speichern (ItemFlags[3] = currentSpeed * 1000)
		float currentSpeed = fabsf((float)item->ItemFlags[3] / FLOATING_POINT_SCALE);
		const float maxSpeed = MAX_MOVE_SPEED;

		// Zielgeschwindigkeit berechnen — mit Distanzberücksichtigung
		float targetSpeed = 0.0f;
		float currentYSpeed = (float)item->ItemFlags[6] / FLOATING_POINT_SCALE;
		const float yLerpAlpha = 1.0f / powf(2.0f, MOVEMENT_LERP_SPEED);
		const int minYDiff = SECTOR_SIZE;
		float ySpeedTargetGlobal = 0.0f;

		if (!inertiaTimer)
		{
			switch (currentState)
			{
			case GunShipState::FOLLOW:
				// Mit MovementTarget: Immer maxSpeed, unabhängig von Distanz
				if (hasMoveTargetPos)
					targetSpeed = maxSpeed;
				else
					targetSpeed = (moveHLen > maxShotsRange) ? maxSpeed : maxSpeed * 0.25f;

				currentYSpeed += (0.0f - currentYSpeed) * yLerpAlpha;
				if (fabsf(yDiff) > minYDiff)
					ySpeedTargetGlobal = (moveTargetY > item->Pose.Position.y)
						? FLY_DOWN_SPEED : -FLY_UP_SPEED;

				break;
			case GunShipState::IDLE:
				targetSpeed = 0.0f;
				if (fabsf(yDiff) > minYDiff)
					ySpeedTargetGlobal = (moveTargetY > item->Pose.Position.y)
						? FLY_DOWN_SPEED * 0.5f : -FLY_UP_SPEED * 0.5f;

				break;
			case GunShipState::EVADE_NEAR:
				targetSpeed = maxSpeed * 2.5f;

				if (item->ItemFlags[7] == 1)
					ySpeedTargetGlobal = -FLY_UP_SPEED * 2.0f;
				else if (item->ItemFlags[7] == 2)
					ySpeedTargetGlobal = (moveTargetY > item->Pose.Position.y) ? FLY_DOWN_SPEED : -FLY_DOWN_SPEED;

				break;
			default:
				break;
			}
		}

		// Trägheit: während der Inertia-Phase stark verzögern
		if (inertiaTimer > 0)
		{
			targetSpeed *= 0.15f;
			inertiaTimer--;
			item->ItemFlags[5] = inertiaTimer;
		}

		// Geschwindigkeit sanft anpassen
		const float speedAlpha = 1.0f / powf(2.0f, MOVEMENT_LERP_SPEED);
		currentSpeed += (targetSpeed - currentSpeed) * speedAlpha;
		if (fabsf(currentSpeed) < 0.5f && targetSpeed == 0.0f)
			currentSpeed = 0.0f;
		item->ItemFlags[3] = (int)(currentSpeed * FLOATING_POINT_SCALE);

		bool isMoving = currentSpeed > 1.0f;

		float pitchTarget = 0.0f;
		float bankTarget = 0.0f;

		// Berechne Ziel-Orientierung zum Shoot-Target (wenn gesetzt), sonst zu Move-Target
		Vector3 vecOrigin = item->Pose.Position.ToVector3();
		EulerAngles targetOrient;
		if (targets.hasShootTarget)
		{
			Vector3 vecShootTarget = shootTargetItem->Pose.Position.ToVector3();
			targetOrient = Geometry::GetOrientToPoint(vecOrigin, vecShootTarget);
		}
		else
		{
			Vector3 vecMoveTarget = (hasMoveTargetPos) ? moveTargetPosCopy : moveTargetItem->Pose.Position.ToVector3();
			targetOrient = Geometry::GetOrientToPoint(vecOrigin, vecMoveTarget);
		}

		// Kollisionsprüfung: Heli darf nicht weiter nach vorne fliegen wenn:
		// 1. Wand/solid geometry im Weg
		// 2. raised floor oder lowered ceiling mit Boundingbox im Weg

		bool blocked = false;
		short earlyBlockRoomNum = item->RoomNumber;

		if (isMoving && moveHLen > 100.0f)
		{
			// Vorwärtsrichtung berechnen (aus der Orientierung des Helis)
			// TR: y=0 zeigt nach -Z (Vorwärts)
			Vector3 forwardVec(
				-sinf(item->Pose.Orientation.y),
				0.0f,
				-cosf(item->Pose.Orientation.y));
			forwardVec.Normalize();

			// Boundingbox Werte
			auto& heliFrame = GetFrame(*item);
			float bottomMeshY = item->Pose.Position.y + heliFrame.BoundingBox.Y1;
			float topMeshY = item->Pose.Position.y + heliFrame.BoundingBox.Y2;

			// Prüfe drei Checkpoints im Vorfeld (frühes Abbremsen ab ~3-4 Squares)
			const float safetyDistance = SECTOR_SIZE;         // 1 Square für sofortiges Stoppen
			const float earlyCheckDistance = SECTOR_SIZE * 3; // 3 Squares für rechtzeitiges Abbremsen

			// Early check: Wenn Wand/Decke in 2 Squars erkannt, bereits targetSpeed reduzieren
			Vector3 earlyCheckPoint = item->Pose.Position.ToVector3() + forwardVec * earlyCheckDistance;
			auto pointCollEarly = GetPointCollision(Vector3(earlyCheckPoint.x, (bottomMeshY + topMeshY) / 2, earlyCheckPoint.z), earlyBlockRoomNum);

			if (pointCollEarly.GetSector().IsWall(earlyCheckPoint.x, earlyCheckPoint.z))
				targetSpeed *= 0.5f; // Schon early abbremsen
			else
			{
				int relCeilHeight = abs(pointCollEarly.GetCeilingHeight() - pointCollEarly.GetFloorHeight());
				if (relCeilHeight <= (int)(topMeshY - bottomMeshY))
					targetSpeed *= 0.5f;
			}

			// Prüfe einen Punkt VOR dem Heli (halbes Square vor Pivot)
			Vector3 checkPoint = item->Pose.Position.ToVector3() + forwardVec * safetyDistance;

			// Mitte prüfen
			short roomNum = item->RoomNumber;
			auto pointCollCenter = GetPointCollision(Vector3(checkPoint.x, (bottomMeshY + topMeshY) / 2, checkPoint.z), roomNum);

			if (pointCollCenter.GetSector().IsWall(checkPoint.x, checkPoint.z))
				blocked = true;

			// Prüfe ob Deckenhöhe zu niedrig ist (Heli passt nicht durch)
			int relCeilHeight = abs(pointCollCenter.GetCeilingHeight() - pointCollCenter.GetFloorHeight());
			if (!blocked && relCeilHeight <= (int)(topMeshY - bottomMeshY))
				blocked = true;

			// Prüfe noch 45° links und rechts (halbes Square seitlich)
			if (!blocked)
			{
				const float sideOffset = SECTOR_SIZE / 2;
				Vector3 leftPoint = checkPoint + Vector3(forwardVec.z, 0.0f, -forwardVec.x) * sideOffset;
				Vector3 rightPoint = checkPoint + Vector3(-forwardVec.z, 0.0f, forwardVec.x) * sideOffset;

				auto pointCollLeft = GetPointCollision(Vector3(leftPoint.x, (bottomMeshY + topMeshY) / 2, leftPoint.z), roomNum);
				auto pointCollRight = GetPointCollision(Vector3(rightPoint.x, (bottomMeshY + topMeshY) / 2, rightPoint.z), roomNum);

				if (pointCollLeft.GetSector().IsWall(leftPoint.x, leftPoint.z))
					blocked = true;

				if (!blocked && pointCollRight.GetSector().IsWall(rightPoint.x, rightPoint.z))
					blocked = true;
			}
		}

		if (blocked)
		{
			targetSpeed = 0.0f;
			isMoving = false;
			ySpeedTargetGlobal = 0.0f; // Y-Bewegung ebenfalls stoppen
			currentYSpeed = 0.0f;      // Aktuelle Y-Geschwindigkeit zurücksetzen
			
			// In IDLE-Zustand wechseln damit Heli hovert und ausrichtet
			currentState = GunShipState::IDLE;
			
			// Pitch/Bank zurücksetzen damit Heli nicht kippt
			pitchTarget = 0.0f;
			bankTarget = 0.0f;
			
			// Position so justieren dass Heli gerade noch vor dem Wall steht (nicht in ihm hinein)
			const float moveDist = currentSpeed;
			if (moveHLen > 1.0f)
			{
				item->Pose.Position.x -= (int)((moveHdx / moveHLen) * moveDist);
				item->Pose.Position.z -= (int)((moveHdz / moveHLen) * moveDist);
			}
			
		}
		if (!blocked && isMoving && moveHLen > 100.0f)
		{
			const float moveDist = currentSpeed;

			switch (currentState)
			{
			case GunShipState::FOLLOW:
			{
				if (moveHLen > 1.0f)
				{
					item->Pose.Position.x += (int)((moveHdx / moveHLen) * moveDist);
					item->Pose.Position.z += (int)((moveHdz / moveHLen) * moveDist);
				}

				pitchTarget = (float)DEG_TO_RAD(MAX_PITCH_DEG);

				if (item->ItemFlags[7])
				{
					float targetReturnY = (hasMoveTargetPos) ? moveTargetPosCopy.y : moveTargetItem->Pose.Position.y;
					if (fabsf(item->Pose.Position.y - targetReturnY) < SECTOR_SIZE)
						item->ItemFlags[7] = 0;
				}

				auto fwdVec = Vector3(
					cosf(item->Pose.Orientation.x) * sinf(item->Pose.Orientation.y),
					sinf(item->Pose.Orientation.x),
					cosf(item->Pose.Orientation.x) * cosf(item->Pose.Orientation.y));
				fwdVec.Normalize();

				Vector3 toMoveTarget;
				if (hasMoveTargetPos)
					toMoveTarget = moveTargetPosCopy - item->Pose.Position.ToVector3();
				else
					toMoveTarget = moveTargetItem->Pose.Position.ToVector3() - item->Pose.Position.ToVector3();
				toMoveTarget.y = 0.0f;
				toMoveTarget.Normalize();

				float crossY = fwdVec.z * toMoveTarget.x - fwdVec.x * toMoveTarget.z;
				bankTarget = DEG_TO_RAD(MAX_BANK_DEG) * crossY;
			}
			break;

			case GunShipState::EVADE_NEAR:
			{
				pitchTarget = -(float)DEG_TO_RAD(MAX_PITCH_DEG);

				if (moveHLen > 1.0f)
				{
					item->Pose.Position.x += (int)(-(moveHdx / moveHLen) * moveDist);
					item->Pose.Position.z += (int)(-(moveHdz / moveHLen) * moveDist);
				}

				auto fwdVec = Vector3(
					cosf(item->Pose.Orientation.x) * sinf(item->Pose.Orientation.y),
					sinf(item->Pose.Orientation.x),
					cosf(item->Pose.Orientation.x) * cosf(item->Pose.Orientation.y));
				fwdVec.Normalize();

				Vector3 toMoveTarget;
				if (hasMoveTargetPos)
					toMoveTarget = moveTargetPosCopy - item->Pose.Position.ToVector3();
				else
					toMoveTarget = moveTargetItem->Pose.Position.ToVector3() - item->Pose.Position.ToVector3();
				toMoveTarget.y = 0.0f;
				toMoveTarget.Normalize();

				float bankCrossY = fwdVec.z * toMoveTarget.x - fwdVec.x * toMoveTarget.z;
				bankTarget = DEG_TO_RAD(MAX_BANK_DEG) * bankCrossY;
			}
			break;

			default:
				break;
			}
		}

		// YSpeed lerp (nur wenn nicht blockiert)
		if (!blocked)
		{
			currentYSpeed += (ySpeedTargetGlobal - currentYSpeed) * yLerpAlpha;

			// Direkte Y-Bewegung über currentYSpeed
			item->Pose.Position.y += (int)currentYSpeed;

			// Wenn Heli im Boden/Decke steckt, auf korrekte Höhe setzen
			auto& frameData = GetFrame(*item);
			float bottomMeshY = item->Pose.Position.y + frameData.BoundingBox.Y1;
			FloorInfo* floorCheck2 = GetFloor(item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z, &item->RoomNumber);
			if (floorCheck2 != nullptr)
			{
				const int floorHeight = GetFloorHeight(floorCheck2, item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z);
				const int ceilingHeight = GetCeiling(floorCheck2, item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z);

				if (floorHeight != NO_VALUE && bottomMeshY > floorHeight)
					item->Pose.Position.y = floorHeight - frameData.BoundingBox.Y1;
				else if (ceilingHeight != NO_VALUE && (item->Pose.Position.y + frameData.BoundingBox.Y2) < ceilingHeight + SECTOR_SIZE / 8)
					item->Pose.Position.y = ceilingHeight + SECTOR_SIZE / 8 - frameData.BoundingBox.Y2;
			}

			// IDLE: Orientierung beibehalten wenn nicht bewegung
			if (!isMoving && currentState == GunShipState::IDLE)
			{
				pitchTarget = (float)DEG_TO_RAD(MAX_PITCH_DEG);

				auto fwdVec = Vector3(
					cosf(item->Pose.Orientation.x) * sinf(item->Pose.Orientation.y),
					sinf(item->Pose.Orientation.x),
					cosf(item->Pose.Orientation.x) * cosf(item->Pose.Orientation.y));
				fwdVec.Normalize();

				Vector3 toMoveTarget;
				if (hasMoveTargetPos)
					toMoveTarget = moveTargetPosCopy - item->Pose.Position.ToVector3();
				else
					toMoveTarget = moveTargetItem->Pose.Position.ToVector3() - item->Pose.Position.ToVector3();
				toMoveTarget.y = 0.0f;
				toMoveTarget.Normalize();

				float crossY = fwdVec.z * toMoveTarget.x - fwdVec.x * toMoveTarget.z;
				bankTarget = DEG_TO_RAD(MAX_BANK_DEG) * crossY;
			}

			// Pitch und Bank über ItemFlags interpolieren
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

				if (fabsf(ySpeedTargetGlobal) > 0.1f)
					currentYSpeed += (ySpeedTargetGlobal - currentYSpeed) * 0.1f;
				else
					currentYSpeed *= 0.95f;
			}

			constexpr int TRACK_SPEED = 3;
			float lerpAlpha = 1.0f / powf(2.0f, TRACK_SPEED);
			if (item->ItemFlags[0] == 1)
				lerpAlpha = 1.0f;

			EulerAngles lerpResult = targetOrient;
			lerpResult.y += ANGLE(180.0f);

			constexpr float RAD_TO_SHORTS = (float)(65536.0 / (2.0 * PI));
			lerpResult.x = (short)(currentPitch * RAD_TO_SHORTS);
			lerpResult.z = (short)(currentBankAngle * RAD_TO_SHORTS);

			EulerAngles lerpResult2 = EulerAngles::Lerp(item->Pose.Orientation, lerpResult, lerpAlpha);
			item->Pose.Orientation = lerpResult2;

			FloorInfo* floorInfo = GetFloor(item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z, &item->RoomNumber);
			int ceilingHeight = NO_VALUE;
			if (floorInfo != nullptr)
				ceilingHeight = GetCeiling(floorInfo, item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z);

			item->ItemFlags[0]++;

			// Post-position collision: Wall sliding
			CollisionInfo coll{};
			auto collObjects = GetCollidedObjects(*item, true, true);

			if (!collObjects.Statics.empty())
			{
				for (const StaticMesh* staticMesh : collObjects.Statics)
					ItemPushStatic(item, *staticMesh, &coll);
			}

			// Schussreichweite basierend auf Shoot-Target Distanz prüfen
			const bool hasShootTargetInRange = targets.hasShootTarget && shootHLen <= maxShotsRange;
			if (hasShootTargetInRange)
			{
				const int frameSinceActivation = item->ItemFlags[0];
				if (!(GlobalCounter & (FIRE_RATE - 1)) && frameSinceActivation > FIRE_RATE)
				{
					SoundEffect(SFX_TR4_HK_FIRE, &item->Pose, SoundEnvironment::Land, 0.8f);
				}

				if (frameSinceActivation <= ROTOR_ACTIVE_THRESHOLD)
					item->MeshBits |= 0x100;
				else
					item->MeshBits &= 0xFEFF;
			}
			else
			{
				item->MeshBits &= 0xFEFF;
			}

			// YSpeed persistent speichern für den nächsten Frame
			if (item->ItemFlags[7] != 1)
				item->ItemFlags[6] = (int)(currentYSpeed * FLOATING_POINT_SCALE);
			else
				item->ItemFlags[6] = (int)currentYSpeed;
		}
	}
}
