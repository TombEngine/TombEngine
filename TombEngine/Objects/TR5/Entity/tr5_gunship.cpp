#include "framework.h"
#include "Objects/TR5/Entity/tr5_gunship.h"

#include <unordered_map>
#include <algorithm>

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

	void ControlGunShip(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		if (!TriggerActive(item))
			return;

		if (!CreatureActive(itemNumber))
			return;

		SoundEffect(SFX_TR4_HELICOPTER_LOOP, &item->Pose);

		auto& creature = *GetCreatureInfo(item);
		//item->Animation.IsAirborne = true;

		//AI_INFO ai{};
		//CreatureAIInfo(item, &ai);
		//GetAITarget(&creature);
		//GetCreatureMood(item, &ai, true);
		//CreatureMood(item, &ai, true);

		//creature.LOT.Zone = ZoneType::Flyer;
		//creature.LOT.Fly = DEFAULT_FLY_UPDOWN_SPEED;
		//creature.LOT.BlockMask = BLOCKED;

		// Mindestabstand aus TriggerFlags berechnen
		const int minDistance = (item->TriggerFlags > 0) ? item->TriggerFlags * SECTOR_SIZE : SECTOR_SIZE * 3;
		const int maxShotsRange = minDistance + SECTOR_SIZE;

		// Horizontale Distanz zu Lara
		const float hdx = LaraItem->Pose.Position.x - item->Pose.Position.x;
		const float hdz = LaraItem->Pose.Position.z - item->Pose.Position.z;
		const float hLen = sqrtf(hdx * hdx + hdz * hdz);

		// Zustand bestimmen
		int currentState = -1;

		// State-Übergang: EVADE_NEAR, IDLE oder FOLLOW
		if (hLen < minDistance)
			currentState = GunShipState::EVADE_NEAR;
		else if (hLen < maxShotsRange && item->ItemFlags[7] == 0)
			currentState = GunShipState::IDLE;
		else
			currentState = GunShipState::FOLLOW;

		// Trägheit: Wenn sich der Zustand geändert hat, Timer starten
		int prevStates = item->ItemFlags[4];
		int inertiaTimer = item->ItemFlags[5];
		if (prevStates != currentState)
		{
			inertiaTimer = INERTIA_FRAMES;
			item->ItemFlags[4] = currentState;
			item->ItemFlags[5] = INERTIA_FRAMES;
		}

		// Vertikales Ausweich-Flag: ItemFlags[7] = isDodgingUp
		// Setzen beim Wechsel zu EVADE_NEAR, löschen wenn sicher entfernt
		if (currentState == GunShipState::EVADE_NEAR && item->ItemFlags[7] == 0 && hLen < SECTOR_SIZE * 3 && item->Pose.Position.y >= LaraItem->Pose.Position.y - SECTOR_SIZE * 6)

			item->ItemFlags[7] = 1;



		if (item->ItemFlags[7] == 1 && hLen > maxShotsRange && currentState != GunShipState::EVADE_NEAR)
		{
			item->ItemFlags[7] = 0;
			inertiaTimer = 0;
			item->ItemFlags[5] = 0;
		}



		// Wenn isDodgingUp, erzwunge EVADE_NEAR um FOLLOW/EVADE oszillation zu verhindern
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
		// Vertikaler Y-Speed-Target
		float ySpeedTargetGlobal = 0.0f;

		if (!inertiaTimer)
		{
			switch (currentState)
			{
			case GunShipState::FOLLOW:
				if (hLen > maxShotsRange)
					targetSpeed = maxSpeed;
				else if (hLen <= maxShotsRange && hLen >= minDistance)
					targetSpeed = maxSpeed * 0.25f; // In Schussreichweite, langsamer

				currentYSpeed += (0.0f - currentYSpeed) * yLerpAlpha;
				if (fabsf(LaraItem->Pose.Position.y - item->Pose.Position.y) > minYDiff)
					ySpeedTargetGlobal = (LaraItem->Pose.Position.y > item->Pose.Position.y)
					? FLY_DOWN_SPEED : -FLY_UP_SPEED;

				break;
			case GunShipState::IDLE:
				targetSpeed = 0.0f;
				if (fabsf(LaraItem->Pose.Position.y - item->Pose.Position.y) > minYDiff)
					ySpeedTargetGlobal = (LaraItem->Pose.Position.y > item->Pose.Position.y)
					? FLY_DOWN_SPEED * 0.5f : -FLY_UP_SPEED * 0.5f;

				break;
			case GunShipState::EVADE_NEAR:
				targetSpeed = maxSpeed * 2.5f;
				//currentYSpeed -= (0.0f - currentYSpeed) * yLerpAlpha;
				ySpeedTargetGlobal = -FLY_UP_SPEED * 2.0f;

				//if (item->ItemFlags[7] == 1)
					//



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

		const bool isMoving = currentSpeed > 1.0f;

		// Y-Geschwindigkeit über ItemFlags[6] speichern (negativ = hoch)

		float pitchTarget = 0.0f;
		float bankTarget = 0.0f;

		// Berechne Ziel-Orientierung VOR der Bewegung für korrekte Vorwärtsrichtung
		Vector3 vecOrigin = item->Pose.Position.ToVector3();
		Vector3 vecTarget = LaraItem->Pose.Position.ToVector3();
		EulerAngles targetOrient = Geometry::GetOrientToPoint(vecOrigin, vecTarget);




		// FOLLOW/IDLE: Nur wenn Lara genug über/unter uns ist

		/*if (fabsf(LaraItem->Pose.Position.y - item->Pose.Position.y) > minYDiff)
		{
			switch (currentState)
			{
			case GunShipState::FOLLOW:
				ySpeedTargetGlobal = (LaraItem->Pose.Position.y > item->Pose.Position.y)
					? FLY_DOWN_SPEED : -FLY_UP_SPEED;
				break;
			case GunShipState::IDLE:
				ySpeedTargetGlobal = (LaraItem->Pose.Position.y > item->Pose.Position.y)
					? FLY_DOWN_SPEED * 0.5f : -FLY_UP_SPEED * 0.5f;
				break;
			default:
				break;
			}
		}*/








		if (isMoving && hLen > 100.0f)
		{
			const float moveDist = currentSpeed;

			switch (currentState)
			{
		case GunShipState::FOLLOW:
		{
			// Geradeaus in Richtung Lara fliegen — keine seitliche Bewegung
			if (hLen > 1.0f)
			{
				item->Pose.Position.x += (int)((hdx / hLen) * moveDist);
				item->Pose.Position.z += (int)((hdz / hLen) * moveDist);
			}

			pitchTarget = (float)DEG_TO_RAD(MAX_PITCH_DEG);

				// Wenn isDodgingUp, sanft auf LaraY zurückkehren (ySpeed auf 0 decelerieren)
				if (item->ItemFlags[7])
				{


					if (item->Pose.Position.y >= LaraItem->Pose.Position.y - SECTOR_SIZE - 50.0f)
						item->ItemFlags[7] = 0;
				}

				auto fwdVec = Vector3(
					cosf(item->Pose.Orientation.x) * sinf(item->Pose.Orientation.y),
					sinf(item->Pose.Orientation.x),
					cosf(item->Pose.Orientation.x) * cosf(item->Pose.Orientation.y));
				fwdVec.Normalize();

				Vector3 toLaraNorm = LaraItem->Pose.Position.ToVector3() - item->Pose.Position.ToVector3();
				toLaraNorm.y = 0.0f;
				toLaraNorm.Normalize();

				float crossY = fwdVec.z * toLaraNorm.x - fwdVec.x * toLaraNorm.z;
				bankTarget = DEG_TO_RAD(MAX_BANK_DEG) * crossY;
			}
			break;

			case GunShipState::EVADE_NEAR:
			{
				pitchTarget = -(float)DEG_TO_RAD(MAX_PITCH_DEG);

				// Weg von Lara fliegen — geradeaus ohne seitliche Bewegung
				if (hLen > 1.0f)
				{
					item->Pose.Position.x += (int)(-(hdx / hLen) * moveDist);
					item->Pose.Position.z += (int)(-(hdz / hLen) * moveDist);
				}

				// Direkt nach oben steigen — unabhängig vom globalen ySpeedTargetGlobal
				// Wenn isDodgingUp, sanft auf LaraY zurückkehren (ySpeed auf 0 decelerieren)
				if (item->ItemFlags[7])
				{


					if (item->Pose.Position.y <= LaraItem->Pose.Position.y - SECTOR_SIZE * 6 || hLen >= maxShotsRange)
						item->ItemFlags[7] = 0;
				}



				auto fwdVec = Vector3(
					cosf(item->Pose.Orientation.x) * sinf(item->Pose.Orientation.y),
					sinf(item->Pose.Orientation.x),
					cosf(item->Pose.Orientation.x) * cosf(item->Pose.Orientation.y));
				fwdVec.Normalize();

				Vector3 toLara = LaraItem->Pose.Position.ToVector3() - item->Pose.Position.ToVector3();
				toLara.y = 0.0f;
				toLara.Normalize();

				float bankCrossY = fwdVec.z * toLara.x - fwdVec.x * toLara.z;
				bankTarget = DEG_TO_RAD(MAX_BANK_DEG) * bankCrossY;
			}
			break;

			default:
				break;
			}
		}

		// Lerne den globalen ySpeedTarget (aktiv für IDLE/FOLLOW — EVADE_NEAR hat eigenen Boost)

		//if (currentState != GunShipState::EVADE_NEAR)
			currentYSpeed += (ySpeedTargetGlobal - currentYSpeed) * yLerpAlpha;

		//if (fabsf(ySpeedTargetGlobal) > 0.1f)
		//	currentYSpeed += (ySpeedTargetGlobal - currentYSpeed) * 0.1f;

		// Direkte Y-Bewegung über currentYSpeed (konstante vertikale Geschwindigkeit, immer möglich)
		item->Pose.Position.y += (int)currentYSpeed;

		// IDLE: Orientierung beibehalten wenn nicht bewegung (targetSpeed ~ 0, aber kein FOLLOW/EVADE_NEAR)
		if (!isMoving && currentState == GunShipState::IDLE)
		{
			pitchTarget = (float)DEG_TO_RAD(MAX_PITCH_DEG);

			auto fwdVec = Vector3(
				cosf(item->Pose.Orientation.x) * sinf(item->Pose.Orientation.y),
				sinf(item->Pose.Orientation.x),
				cosf(item->Pose.Orientation.x) * cosf(item->Pose.Orientation.y));
			fwdVec.Normalize();

			Vector3 toLaraNorm = LaraItem->Pose.Position.ToVector3() - item->Pose.Position.ToVector3();
			toLaraNorm.y = 0.0f;
			toLaraNorm.Normalize();

			float crossY = fwdVec.z * toLaraNorm.x - fwdVec.x * toLaraNorm.z;
			bankTarget = DEG_TO_RAD(MAX_BANK_DEG) * crossY;
		}

		// Post-position collision: Wall sliding
		CollisionInfo coll{};
		auto collObjects = GetCollidedObjects(*item, true, true);

		if (!collObjects.Statics.empty())
		{
			for (const StaticMesh* staticMesh : collObjects.Statics)
				ItemPushStatic(item, *staticMesh, &coll);
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

			/*/ YSpeed auch im Idle sanft auf ySpeedTargetGlobal lerpren
			if (fabsf(ySpeedTargetGlobal) > 0.1f)
				currentYSpeed += (ySpeedTargetGlobal - currentYSpeed) * 0.1f;
			else
				currentYSpeed *= 0.95f;*/
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

		auto& heliFrame = GetFrame(*item);
		float heliTopMeshY = item->Pose.Position.y + heliFrame.BoundingBox.Y1;

		/*if (ceilingHeight != NO_VALUE && heliTopMeshY < ceilingHeight)
		{
			item->Pose.Position.y = ceilingHeight - heliFrame.BoundingBox.Y1;

			const int distToCeiling = (int)(item->Pose.Position.y + heliFrame.BoundingBox.Y1 - ceilingHeight);
			if (distToCeiling > SECTOR_SIZE / 4)
			{
				// Decke erreicht: YSpeed auf 0 setzen um Oszillation zu vermeiden
				currentYSpeed = 0.0f;

				const float maxYPos = LaraItem->Pose.Position.y - SECTOR_SIZE;
				if (maxYPos < item->Pose.Position.y)
				{
					const float sinkSpeed = 0.5f;
					item->Pose.Position.y += (int)((maxYPos - item->Pose.Position.y) * sinkSpeed);
				}

				// YSpeed persistent auf 0 setzen statt nur lokal
				item->ItemFlags[6] = 0;
			}
		}*/

		FloorInfo* floorCheck = GetFloor(item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z, &item->RoomNumber);
		if (floorCheck != nullptr)
		{
			const int floorHeight = GetFloorHeight(floorCheck, item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z);
			if (floorHeight != NO_VALUE)
			{
				auto& heliFrame = GetFrame(*item);
				const float bottomMeshY = item->Pose.Position.y + heliFrame.BoundingBox.Y1;
				const int distToFloor = floorHeight - (int)bottomMeshY;

				if (distToFloor < SECTOR_SIZE / 8)
					item->Pose.Position.y += (int)((float)(SECTOR_SIZE / 8 - distToFloor));
			}
		}

		item->ItemFlags[0]++;

		const bool isInRange = hLen <= maxShotsRange;
		if (isInRange)
		{
			const int frameSinceActivation = item->ItemFlags[0];
			if (!(GlobalCounter & (FIRE_RATE - 1)) && frameSinceActivation > FIRE_RATE)
				SoundEffect(SFX_TR4_HK_FIRE, &item->Pose, SoundEnvironment::Land, 0.8f);

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
		if (currentState != GunShipState::EVADE_NEAR || item->ItemFlags[7] == 0)
			item->ItemFlags[6] = (int)(currentYSpeed * FLOATING_POINT_SCALE);
		else
			item->ItemFlags[6] = (int)currentYSpeed;

		AnimateItem(item);
	}
}