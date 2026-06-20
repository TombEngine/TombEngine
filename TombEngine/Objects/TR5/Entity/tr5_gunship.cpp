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
	constexpr float YAW_LERP_SPEED = 3.0f;
	constexpr float PITCH_LERP_SPEED = 5.0f;
	constexpr float BANK_LERP_SPEED = 3.0f;

	constexpr int MAX_PITCH_DEG = 20;
	constexpr int MAX_BANK_DEG = 15;
	constexpr float MAX_MOVE_SPEED = 84.0f;
	constexpr float FLY_UP_SPEED = 40.0f;
	constexpr float FLY_DOWN_SPEED = 40.0f;
	constexpr int SECTOR_SIZE = 1024;
	constexpr int FLOATING_POINT_SCALE = 1000;

	enum GunShipState
	{
		FOLLOW = 0,
		EVADE_NEAR = 1
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

		AI_INFO ai{};
		CreatureAIInfo(item, &ai);
		GetAITarget(&creature);
		GetCreatureMood(item, &ai, true);
		CreatureMood(item, &ai, true);

		creature.LOT.Zone = ZoneType::Flyer;
		creature.LOT.Fly = DEFAULT_FLY_UPDOWN_SPEED;
		creature.LOT.BlockMask = BLOCKED;

		// Mindestabstand aus TriggerFlags berechnen
		const int minDistance = (item->TriggerFlags > 0) ? item->TriggerFlags * SECTOR_SIZE : SECTOR_SIZE * 3;
		const int maxShotsRange = minDistance + SECTOR_SIZE;

		// Horizontale Distanz zu Lara
		const float hdx = LaraItem->Pose.Position.x - item->Pose.Position.x;
		const float hdz = LaraItem->Pose.Position.z - item->Pose.Position.z;
		const float hLen = sqrtf(hdx * hdx + hdz * hdz);

		// Zustand bestimmen
		item->ItemFlags[6] = GunShipState::FOLLOW;
		if (hLen < minDistance)
			item->ItemFlags[6] = GunShipState::EVADE_NEAR;

		// Beschleunigung/Verzögerung über ItemFlags speichern (ItemFlags[3] = currentSpeed * 1000)
		float currentSpeed = fabsf((float)item->ItemFlags[3] / FLOATING_POINT_SCALE);
		const float maxSpeed = MAX_MOVE_SPEED;

		// Zielgeschwindigkeit berechnen — mit Distanzberücksichtigung
		float targetSpeed = 0.0f;
		switch (item->ItemFlags[6])
		{
		case GunShipState::FOLLOW:
			if (hLen > maxShotsRange)
				targetSpeed = maxSpeed;
			else if (hLen <= maxShotsRange && hLen >= minDistance)
				targetSpeed = maxSpeed * 0.25f; // In Schussreichweite, langsamer
			break;
		case GunShipState::EVADE_NEAR:
			targetSpeed = maxSpeed * 0.75f;
			break;
		default:
			break;
		}

		// Geschwindigkeit sanft anpassen
		const float speedAlpha = 1.0f / powf(2.0f, MOVEMENT_LERP_SPEED);
		currentSpeed += (targetSpeed - currentSpeed) * speedAlpha;
		if (fabsf(currentSpeed) < 0.5f && targetSpeed == 0.0f)
			currentSpeed = 0.0f;
		item->ItemFlags[3] = (int)(currentSpeed * FLOATING_POINT_SCALE);

		const bool isMoving = currentSpeed > 1.0f;

		// Zielposition berechnen
		int targetX = item->Pose.Position.x;
		int targetY = item->Pose.Position.y;
		int targetZ = item->Pose.Position.z;
		float pitchTarget = 0.0f;
		float bankTarget = 0.0f;

		if (isMoving && hLen > 100.0f)
		{
			const float moveDist = currentSpeed / powf(2.0f, MOVEMENT_LERP_SPEED);

			switch (item->ItemFlags[6])
			{
			case GunShipState::FOLLOW:
			{
				// Bewegung entlang der Helisichtlinie via Geometry::TranslatePoint
				auto targetPos = Geometry::TranslatePoint(item->Pose.Position.ToVector3(), item->Pose.Orientation, moveDist);
				targetX = (int)targetPos.x;
				targetZ = (int)targetPos.z;

				// Vorwrtpitch (nach unten = positiv)
				pitchTarget = (float)DEG_TO_RAD(MAX_PITCH_DEG);

			// Seitliches Banken: Cross Product zwischen Helis Vorwärtsvektor und zum-Lara-Vektor
			auto fwdVec = Vector3(
				cosf(item->Pose.Orientation.x) * sinf(item->Pose.Orientation.y),
				sinf(item->Pose.Orientation.x),
				cosf(item->Pose.Orientation.x) * cosf(item->Pose.Orientation.y));
			fwdVec.Normalize();

			Vector3 toLara = LaraItem->Pose.Position.ToVector3() - item->Pose.Position.ToVector3();
			toLara.y = 0.0f;
			toLara.Normalize();

			Vector3 crossProduct;
			crossProduct.x = fwdVec.y * toLara.z - fwdVec.z * toLara.y;
			crossProduct.y = fwdVec.z * toLara.x - fwdVec.x * toLara.z;
			crossProduct.z = fwdVec.x * toLara.y - fwdVec.y * toLara.x;

			bankTarget = DEG_TO_RAD(MAX_BANK_DEG) * crossProduct.y;

			// Y-Hhe: in Lara-Nhe (negativ Y = oben)
			targetY = (int)(LaraItem->Pose.Position.y - SECTOR_SIZE / 2);
				break;
			}

			case GunShipState::EVADE_NEAR:
			{
				// Zurckfliegen (entgegen der Helisichtlinie)
				EulerAngles evadeOrient = item->Pose.Orientation;
				evadeOrient.y += ANGLE(180.0f);
				auto targetPos = Geometry::TranslatePoint(item->Pose.Position.ToVector3(), evadeOrient, moveDist);
				targetX = (int)targetPos.x;
				targetZ = (int)targetPos.z;

				// Rckwertspitch (nach oben = negativ)
				pitchTarget = -(float)DEG_TO_RAD(MAX_PITCH_DEG);

				// Nach oben steigen (negativ Y = oben)
				targetY -= (int)(FLY_UP_SPEED / powf(2.0f, MOVEMENT_LERP_SPEED));

				// Seitliches Banken: Cross Product zwischen Helis Vorwärtsvektor und zum-Lara-Vektor
				auto fwdVec = Vector3(
					cosf(item->Pose.Orientation.x) * sinf(item->Pose.Orientation.y),
					sinf(item->Pose.Orientation.x),
					cosf(item->Pose.Orientation.x) * cosf(item->Pose.Orientation.y));
				fwdVec.Normalize();

				Vector3 toLara = LaraItem->Pose.Position.ToVector3() - item->Pose.Position.ToVector3();
				toLara.y = 0.0f;
				toLara.Normalize();

				Vector3 crossProduct;
				crossProduct.x = fwdVec.y * toLara.z - fwdVec.z * toLara.y;
				crossProduct.y = fwdVec.z * toLara.x - fwdVec.x * toLara.z;
				crossProduct.z = fwdVec.x * toLara.y - fwdVec.y * toLara.x;

				bankTarget = DEG_TO_RAD(MAX_BANK_DEG) * crossProduct.y;

				// Mindesthe ber Boden
				FloorInfo* floorCheck = GetFloor(targetX, targetY, targetZ, &item->RoomNumber);
				if (floorCheck != nullptr)
				{
					const int floorH = GetFloorHeight(floorCheck, targetX, targetY, targetZ);
					if (floorH != NO_VALUE)
					{
						auto& heliFrame = GetFrame(*item);
						const float minH = (int)(floorH - SECTOR_SIZE / 2);
						if (targetY < minH)
							targetY = minH;
					}
				}
				break;
			}

			default:
				break;
			}
		}

		// Position lerp-en
		const float posLerpAlpha = 1.0f / powf(2.0f, MOVEMENT_LERP_SPEED);
		item->Pose.Position.x += (int)((targetX - item->Pose.Position.x) * posLerpAlpha);
		item->Pose.Position.y += (int)((targetY - item->Pose.Position.y) * posLerpAlpha);
		item->Pose.Position.z += (int)((targetZ - item->Pose.Position.z) * posLerpAlpha);

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

		// Wenn nicht bewegt, Neigung abbauen
		if (!isMoving)
		{
			currentPitch *= 0.95f;
			currentBankAngle *= 0.95f;
			item->ItemFlags[1] = (int)(currentPitch * FLOATING_POINT_SCALE);
			item->ItemFlags[2] = (int)(currentBankAngle * FLOATING_POINT_SCALE);
		}

		// Orientation — yaw from GetOrientToPoint, controlled pitch/roll
		Vector3 vecOrigin = item->Pose.Position.ToVector3();
		Vector3 vecTarget = LaraItem->Pose.Position.ToVector3();

		EulerAngles targetOrient = Geometry::GetOrientToPoint(vecOrigin, vecTarget);
		targetOrient.y += ANGLE(180.0f);

		constexpr int TRACK_SPEED = 3;
		float lerpAlpha = 1.0f / powf(2.0f, TRACK_SPEED);
		if (item->ItemFlags[0] == 1)
			lerpAlpha = 1.0f;

		EulerAngles lerpResult = EulerAngles::Lerp(item->Pose.Orientation, targetOrient, lerpAlpha);

		constexpr float RAD_TO_SHORTS = (float)(65536.0 / (2.0 * PI));
		lerpResult.x = (short)(currentPitch * RAD_TO_SHORTS);
		lerpResult.z = (short)(currentBankAngle * RAD_TO_SHORTS);

		item->Pose.Orientation = lerpResult;

		// Ceiling correction
		FloorInfo* floorInfo = GetFloor(item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z, &item->RoomNumber);
		int ceilingHeight = NO_VALUE;
		if (floorInfo != nullptr)
			ceilingHeight = GetCeiling(floorInfo, item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z);

		auto& heliFrame = GetFrame(*item);
		float heliTopMeshY = item->Pose.Position.y + heliFrame.BoundingBox.Y1;

		if (ceilingHeight != NO_VALUE && heliTopMeshY < ceilingHeight)
		{
			item->Pose.Position.y = ceilingHeight - heliFrame.BoundingBox.Y1;

			const int distToCeiling = (int)(item->Pose.Position.y + heliFrame.BoundingBox.Y1 - ceilingHeight);
			if (distToCeiling > SECTOR_SIZE / 4)
			{
				const float maxYPos = LaraItem->Pose.Position.y - SECTOR_SIZE;
				if (maxYPos < item->Pose.Position.y)
				{
					const float sinkSpeed = 0.5f;
					item->Pose.Position.y += (int)((maxYPos - item->Pose.Position.y) * sinkSpeed);
				}
			}
		}

		// Mesh-sicherheitsabstand zum Boden
		FloorInfo* floorCheck = GetFloor(item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z, &item->RoomNumber);
		if (floorCheck != nullptr)
		{
			const int floorHeight = GetFloorHeight(floorCheck, item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z);
			if (floorHeight != NO_VALUE)
			{
				auto& heliFrame = GetFrame(*item);
				const float bottomMeshY = item->Pose.Position.y + heliFrame.BoundingBox.Y1;
				const int distToFloor = floorHeight - (int)bottomMeshY;

				if (distToFloor < SECTOR_SIZE / 2)
					item->Pose.Position.y += (int)((float)(SECTOR_SIZE / 2 - distToFloor) * 0.5f);
			}
		}

		// --- Shot counter & mesh control ---
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

		AnimateItem(item);
	}
}
