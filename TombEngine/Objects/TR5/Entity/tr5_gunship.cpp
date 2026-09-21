#include "framework.h"
#include "Objects/TR5/Entity/tr5_gunship.h"

#include "Scripting/Internal/TEN/Objects/Creature/Creature.h"
#include "Scripting/Internal/TEN/Properties/PropertyHandler.h"
#include "Scripting/Internal/TEN/Properties/PropertyNames.h"
#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/room.h"
#include "Game/Lara/lara.h"
#include "Game/control/los.h"
#include "Math/Geometry.h"
#include "Sound/sound.h"
#include "Game/Setup.h"
#include "Game/effects/debris.h"
#include "Specific/level.h"

#include "Game/misc.h"
#include "Game/effects/tomb4fx.h"

using namespace TEN::Animation;
using namespace TEN::Math;
using namespace TEN::Scripting::Properties;

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
	constexpr float EVADE_RAISE_HEIGHT = SECTOR_SIZE * 1.5f; // ~1.5 BLOCK, Aufstieg beim Evaden, damit das Heck den Boden nicht berührt
	constexpr float HOVER_HEIGHT_OFFSET = SECTOR_SIZE * 1.5f; // Heli schwebt ~1.5 Sektoren über dem Ziel, damit er beim Schießen nach unten zielen kann
	constexpr float EVADE_OVERFLY_HEIGHT = SECTOR_SIZE * 3.0f; // Overfly: Heli steigt so weit ueber Lara, dass Rumpf-Heck frei bleibt
	constexpr float MOVE_TARGET_REACH_RADIUS = SECTOR_SIZE * 0.5f; // moveTargetPos: One-Shot-Radius (horizontal), danach wird der Escape-/MoveTarget-Zustand geleert
	constexpr float ESCAPE_EXCLUDE_RADIUS = SECTOR_SIZE * 2.0f; // Ausschlussradius: das zuletzt fehlgeschlagene Escapetarget wird bei der Neu-Suche nicht erneut gewaehlt
	constexpr int MAX_ESCAPE_FRAMES = 280; // Auto-Escape: max. Frames, bevor der Escape-Flug aufgegeben wird (Kampf wird fortgesetzt)

	// Enum für Helikopter-Status
	enum class GunShipState : short
	{
		FOLLOW = 0,
		IDLE = 1,
		EVADE_NEAR = 2,
		ESCAPE = 3
	};

	// Statische Orbit-Daten zum Ausweichen. Nicht im Savegame persistiert;
	// der Heli richtet sich nach einem Neuladen automatisch neu aus.
	struct GunShipOrbitData
	{
		int Direction = 1;      // +1 / -1: Umlaufrichtung beim Evaden.
		bool Initialized = false;
	};

	static GunShipOrbitData GunShipOrbit;

	// Nicht-persistenter Auto-Escape-Zustand: wenn aktiv, dient das Escapetarget als MoveTarget.
	// Wird gesetzt, wenn der Heli in EVADE blockiert ist, und geleert, wenn er das Ziel erreicht hat.
	struct GunShipEscapeData
	{
		bool Active = false;
		Vector3 TargetPos = Vector3::Zero;
		int Frames = 0;

		void Reset()
		{
			Active = false;
			TargetPos = Vector3::Zero;
			Frames = 0;
		}
	};

	static GunShipEscapeData GunShipEscape;

	// Helper: Bestimmt den aktuellen Status basierend auf Distanz und Kollisionen
	GunShipState DetermineGunShipState(const ItemInfo& item, float horizontalDistance, bool hasMoveTargetPos)
	{
		int minDistance = (item.TriggerFlags > 0) ? item.TriggerFlags * SECTOR_SIZE : SECTOR_SIZE * 3;
		int maxShotsRange = minDistance + SECTOR_SIZE;

		if (hasMoveTargetPos)
		{
			// Escape-Flug: eigener State (freie Bewegung, Y frei innerhalb Raum-Bounds).
			return GunShipState::ESCAPE;
		}

		if (horizontalDistance < minDistance)
			return GunShipState::EVADE_NEAR;

		// Innerhalb der Schussreichweite: immer stabil schweben, unabhaengig von Evade-Flag oder Blockade.
		if (horizontalDistance < maxShotsRange)
			return GunShipState::IDLE;

		// TEST (FOLLOW): Wand-Check vorlaeufig deaktiviert – der Heli fliegt im FOLLOW-Zweig
		// auch durch Waende (bewusst, fuer den Test). Neuen Wall-Check spaeter neu bauen.
		return GunShipState::FOLLOW;
	}

	// Helper: Liefert den Raum, in dem eine Probe-Position liegt: aktueller Raum oder ein direkt
	// verbundener Nachbarraum (per Portal, aus NeighborRoomNumbers). NO_VALUE = ausserhalb der
	// gueltigen, verbundenen Room-Geometrie (z.B. Horizon / Open Area ohne verbundenen Room).
	int ResolveProbeRoom(const Vector3& probePos, int startRoomNumber)
	{
		Vector3i probe(probePos.x, probePos.y, probePos.z);
		if (IsPointInRoom(probe, startRoomNumber))
			return startRoomNumber;

		const auto& room = g_Level.Rooms[startRoomNumber];
		for (int neighborRoomNumber : room.NeighborRoomNumbers)
		{
			if (IsPointInRoom(probe, neighborRoomNumber))
				return neighborRoomNumber;
		}

		return NO_VALUE;
	}

	// Helper: Richtungs-unabhaengiger Kollisions-Fussabdruck-Check.
	// Prueft, ob der um `displacement` verschobene Bodenebenen-Fussabdruck des Helikopters
	// gegen Wand-Sektoren, erhohte Squares / abgesenkte Decken (Clearance) stoesst.
	// Ein Gitter von Stichproben ueber die gesamte Fussabdruck-Flaeche (Ecken, Kanten- und Innenpunkte)
	// wird geprueft, damit auch innere Waende erkannt werden - nicht nur die 4 Ecken.
	// Jeder Punkt muss zudem in einem gueltigen, verbundenen Raum liegen (kein Horizon, kein unverbundenes Aussenland).
	bool CheckFootprintCollision(const ItemInfo& item, const Vector3& displacement)
	{
		auto& heliFrame = GetFrame(item);
		float bottomMeshY = item.Pose.Position.y + heliFrame.BoundingBox.Y1 + displacement.y;
		float topMeshY = item.Pose.Position.y + heliFrame.BoundingBox.Y2 + displacement.y;
		int bandHeight = (int)(topMeshY - bottomMeshY);
		float midY = (bottomMeshY + topMeshY) / 2.0f;

		float yawCos = cosf(item.Pose.Orientation.y);
		float yawSin = sinf(item.Pose.Orientation.y);
		float posX = item.Pose.Position.x;
		float posZ = item.Pose.Position.z;

		// Gitter ueber die Fussabdruck-Flaeche (lokal, relativ zum Pose-Ort): 4x4 = 16 Stichproben.
		constexpr int FOOTPRINT_GRID = 4;
		float xSpan = (float)heliFrame.BoundingBox.X2 - (float)heliFrame.BoundingBox.X1;
		float zSpan = (float)heliFrame.BoundingBox.Z2 - (float)heliFrame.BoundingBox.Z1;

		for (int gx = 0; gx < FOOTPRINT_GRID; gx++)
		{
			for (int gz = 0; gz < FOOTPRINT_GRID; gz++)
			{
				float fx = (float)gx / (float)(FOOTPRINT_GRID - 1);
				float fz = (float)gz / (float)(FOOTPRINT_GRID - 1);
				float localX = (float)heliFrame.BoundingBox.X1 + fx * xSpan;
				float localZ = (float)heliFrame.BoundingBox.Z1 + fz * zSpan;

				// Yaw-Rotation (um Y) + World-Translation + vorgeschlagene Verschiebung.
				float worldX = posX + (localX * yawCos + localZ * yawSin) + displacement.x;
				float worldZ = posZ + (-localX * yawSin + localZ * yawCos) + displacement.z;

				Vector3 probePos(worldX, midY, worldZ);

				// Punkt muss in einem gueltigen, verbundenen Raum liegen (kein Horizon, kein unverbundenes Aussenland).
				int probeRoom = ResolveProbeRoom(probePos, item.RoomNumber);
				if (probeRoom == NO_VALUE)
					return true;

				auto pointColl = GetPointCollision(probePos, probeRoom);

				if (pointColl.IsWall())
					return true;

				// Exakte Durchdringung: Fussabdruck-Unterkante unter dem Boden oder Oberkante ueber der Decke.
				int floorHeight = pointColl.GetFloorHeight();
				int ceilingHeight = pointColl.GetCeilingHeight();
				if (floorHeight != NO_HEIGHT && floorHeight < bottomMeshY)
					return true;
				if (ceilingHeight != NO_HEIGHT && ceilingHeight > topMeshY)
					return true;

				if (abs(ceilingHeight - floorHeight) <= bandHeight)
					return true;
			}
		}

		return false;
	}

	// Unterstrukturierter Kollisions-Check: prueft den Fussabdruck entlang des
	// kompletten Verschiebungspfades (x+y+z) in kleinen Sub-Schritten. true = frei.
	bool SweptFootprintClear(const ItemInfo& item, const Vector3& displacement)
	{
		float stepLength = displacement.Length();
		if (stepLength < 1.0f)
			return !CheckFootprintCollision(item, displacement);

		// Sub-Schritte, max. SECTOR/4, damit keine Wand bei schneller Bewegung durchflogen wird.
		constexpr float MAX_SUBSTEP = (float)SECTOR_SIZE * 0.25f;
		int steps = (int)(stepLength / MAX_SUBSTEP + 1.0f);

		Vector3 dir = displacement * (1.0f / stepLength);
		for (int i = 1; i <= steps; i++)
		{
			Vector3 offset = dir * (stepLength * ((float)i / (float)steps));
			if (CheckFootprintCollision(item, offset))
				return false;
		}

		return true;
	}

	// Helper: Sucht eine freie Flug-Richtung nahe der bevorzugten Richtung (XZ-Ebene).
	// Testet fuenf symmetrische Kandidaten (0, +/-45, +/-90 Grad) um preferredDir und waehlt die
	// kollisionsfreie Richtung mit dem kleinsten Abstand zur bevorzugten Richtung.
	// Ein Continuity-Bias bevorzugt die zuletzt gewaehlte Ausweich-Seite (verhindert Hin-/Her-Kippen).
	// true = freie Richtung gefunden (in outDir), false = keine Richtung frei.
	bool FindBestAvoidanceDirection(const ItemInfo& item, const Vector3& preferredDir, float probeDistance, Vector3& outDir)
	{
		// Fuenf Kandidaten in Grad, symmetrisch um die bevorzugte Richtung.
		const float candidateAngles[5] = { 0.0f, 45.0f, -45.0f, 90.0f, -90.0f };
		const float RAD_PER_DEG = 0.0174532925f; // PI / 180

		float bestScore = 1000.0f;
		Vector3 bestDir = preferredDir;
		bool found = false;

		int orbitSide = (GunShipOrbit.Direction > 0) ? 1 : -1;

		for (int i = 0; i < 5; i++)
		{
			float theta = candidateAngles[i] * RAD_PER_DEG;
			float cosT = cosf(theta);
			float sinT = sinf(theta);

			// 2D-Rotation um Y (nur XZ-Komponenten).
			Vector3 cand(preferredDir.x * cosT - preferredDir.z * sinT, 0.0f, preferredDir.x * sinT + preferredDir.z * cosT);
			cand.Normalize();

			if (!SweptFootprintClear(item, cand * probeDistance))
				continue;

			// Scoring: Abstand zur bevorzugten Richtung, mit Continuity-Bias auf die letzte Ausweich-Seite.
			float score = fabsf(candidateAngles[i]);
			int candidateSide = (candidateAngles[i] > 0.0f) ? 1 : ((candidateAngles[i] < 0.0f) ? -1 : 0);
			if (GunShipOrbit.Initialized && candidateSide != 0 && candidateSide != orbitSide)
				score += 0.5f;

			if (score < bestScore)
			{
				bestScore = score;
				bestDir = cand;
				found = true;
				if (candidateSide != 0)
					GunShipOrbit.Direction = candidateSide;
			}
		}

		if (!found)
			return false;

		GunShipOrbit.Initialized = true;
		outDir = bestDir;
		return true;
	}

	// Helper: Sucht ein Escapetarget in gueltiger, verbundener Raum-Geometrie, das mindestens
	// minEscapeDist vom Shoot-Target entfernt ist und ausreichende Flug-Clearance bietet
	// (inkl. Decken-Hoehe). Wird bei Blockade in EVADE_NEAR/ESCAPE verwendet, um aus einer Sackgasse zu fliegen.
	// Kandidaten in excludeRadius um excludePos werden ausgespart, damit ein fehlgeschlagenes
	// Escapetarget nicht erneut gewaehlt wird (verhindert den Stuck-Loop).
	// true = Escapetarget gefunden (in outPos), false = kein gueltiger Punkt.
	bool FindEscapeTarget(const ItemInfo& item, const Vector3& shootTargetPos, float minEscapeDist, const Vector3& excludePos, float excludeRadius, Vector3& outPos)
	{
		Vector3 heliPos = item.Pose.Position.ToVector3();
		float bestScore = 10000000.0f;
		bool found = false;

		// Ring von Kandidaten im Umkreis des Shoot-Targets (horizontal), auf der aktuellen Heli-Hoehe.
		constexpr int DIRS = 12;
		constexpr float TWO_PI = 6.28318530718f;

		for (int k = 0; k < DIRS; k++)
		{
			float angle = ((float)k / (float)DIRS) * TWO_PI;
			Vector3 cand(
				shootTargetPos.x + cosf(angle) * minEscapeDist,
				heliPos.y,
				shootTargetPos.z + sinf(angle) * minEscapeDist);

			Vector3 disp = cand - heliPos;
			if (disp.Length() < (float)SECTOR_SIZE * 2.0f)
				continue; // zu nah am Heli, kein sinnvoller Fluchtpunkt

			// Destination muss Clearance + verbundene Geometrie bieten (ResolveProbeRoom + Waende/Decke).
			if (CheckFootprintCollision(item, disp))
				continue;

			// Pfad muss an mehreren Zwischenpunkten frei sein, damit das Ziel realistisch erreichbar ist.
			// (Nur die Pfad-Mitte zu pruefen verpasst Blockaden im letzten Stueck vor dem Ziel.)
			if (CheckFootprintCollision(item, disp * 0.33f) ||
				CheckFootprintCollision(item, disp * 0.66f) ||
				CheckFootprintCollision(item, disp * 0.9f))
				continue;

			// Zuletzt fehlgeschlagenes Escapetarget nicht erneut waehlen (sonst gleicher Stuck-Loop).
			// Nur horizontal (XZ) vergleichen, da der Ring auf der Heli-Hoehe liegt.
			if (excludePos != Vector3::Zero)
			{
				float exx = cand.x - excludePos.x;
				float exz = cand.z - excludePos.z;
				if (sqrtf(exx * exx + exz * exz) < excludeRadius)
					continue;
			}

			// Naechster gueltige Punkt (wenigste Fluggestrecke) bevorzugt.
			float score = disp.Length();
			if (score < bestScore)
			{
				bestScore = score;
				outPos = cand;
				found = true;

				

			}
		}

		return found;
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
				const int minCeilingClearance = SECTOR_SIZE / 16; // 64 units = 0.0625 BLOCK, Heli soll so nah wie möglich an die Decke dürfen
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
		case GunShipState::ESCAPE:
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

	// Helper: IDLE-Pitch, mit dem die Heckwaffe auf Unterschenkel-Hohe des Ziels zielt.
	// Positives Pitch = Nase nach oben = Heckwaffe (Modellachse -Z) zeigt nach unten (abgueringelt: Pitch + 8 Grad).
	float CalculateIdlePitch(const ItemInfo& item, const Vector3& targetPos)
	{
		float dx = targetPos.x - item.Pose.Position.x;
		float dz = targetPos.z - item.Pose.Position.z;
		float hDist = sqrtf(dx * dx + dz * dz);
		if (hDist < 1.0f)
			hDist = 1.0f;

		// Unterschenkel liegen leicht ueber dem Pivot des Ziels (Y positiv = nach unten).
		constexpr float LOWER_LEG_OFFSET = SECTOR_SIZE * 0.25f;
		float dy = (targetPos.y - LOWER_LEG_OFFSET) - item.Pose.Position.y;

		float pitch = atan2f(dy, hDist) - (float)DEG_TO_RAD(8.0f);

		constexpr float maxPitch = (float)DEG_TO_RAD(MAX_PITCH_DEG);
		if (pitch > maxPitch)
			pitch = maxPitch;
		if (pitch < 0.0f)
			pitch = 0.0f;
		return pitch;
	}

	void ControlGunShip(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		if (!TriggerActive(item))
			return;

		if (!CreatureActive(itemNumber))
			return;

		SoundEffect(SFX_TR4_HELICOPTER_LOOP, &item->Pose);

		int shootTargetNum = PropertyHandler::Get(*item, PropName_ShootTarget, -1);
		shootTargetNum = LaraItem->Index;//-1; Zum Testen auf Lara gestellt, zeile wird dann wieder gelöscht

		bool hasShootTarget = (shootTargetNum >= 0);

		// moveTargetPos (hoechste Prioritaet): Zielposition als Vec3. One-Shot: wird beim Erreichen geleert.
		//Vector3 moveTargetPos = (Vector3)PropertyHandler::Get(*item, "GunshipMoveTarget", Vec3());
		//bool hasMoveTargetPos = (moveTargetPos != Vector3::Zero);

		Vector3 moveTargetPos = Vector3::Zero;
		bool hasMoveTargetPos = (moveTargetPos != Vector3::Zero);

		// Auto-Escape (nicht-persistent): wenn aktiv, dient das Escapetarget als MoveTarget (Vorrang vor Shoot-Target).
		if (GunShipEscape.Active)
		{
			moveTargetPos = GunShipEscape.TargetPos;
			hasMoveTargetPos = true;

			// Timeout: Escape darf nicht ewig laufen (z.B. Pfad blockiert / Ziel nicht erreichbar) -> dann Kampf fortsetzen.
			if (++GunShipEscape.Frames > MAX_ESCAPE_FRAMES)
			{
				GunShipEscape.Reset();
				hasMoveTargetPos = false;
				moveTargetPos = Vector3::Zero;
				GunShipEscape.Active = false;
				GunShipEscape.TargetPos = Vector3::Zero;
				GunShipEscape.Frames = 0;
			}
		}

		// One-Shot: moveTargetPos (HORIZONTAL) erreicht -> Property leeren, Escape-Zustand zuruecksetzen, normaler Kampf-Modus.
		// Horizontal statt 3D: im FOLLOW schwebt der Heli ~HOVER_HEIGHT_OFFSET unter dem Ziel und wuerde die 3D-Distanz
		// nie < Radius senken -> der Escape-Zustand wuerde sonst nie geleert (Heli steckt in IDLE fest, feuert nicht).
		if (hasMoveTargetPos)
		{
			float dmx = moveTargetPos.x - item->Pose.Position.x;
			float dmz = moveTargetPos.z - item->Pose.Position.z;
			if (sqrtf(dmx * dmx + dmz * dmz) < MOVE_TARGET_REACH_RADIUS)
			{
				item->Properties.Set("GunshipMoveTarget", Vec3());
				GunShipEscape.Reset();
				hasMoveTargetPos = false;
				moveTargetPos = Vector3::Zero;
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

		GunShipState currentState = DetermineGunShipState(*item, horizontalDist, hasMoveTargetPos);

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

		// Evade abgeschlossen (Heli wieder ausserhalb der Schussreichweite) -> Evade-Flag zuruecksetzen.
		// Sonst schaltet der Heli in Reichweite nie sauber in IDLE (FOLLOW/EVADE-Loop).
		if (item->ItemFlags[7] == 1 && horizontalDist > maxShotsRange && currentState != GunShipState::EVADE_NEAR)
		{
			item->ItemFlags[7] = 0;
			inertiaTimer = 0;
			item->ItemFlags[5] = 0;
		}

		if (item->ItemFlags[7] == 1 && currentState != GunShipState::ESCAPE)
			currentState = GunShipState::EVADE_NEAR;

		float currentYSpeed = (float)item->ItemFlags[6] / FLOATING_POINT_SCALE;
		const float yLerpAlpha = 1.0f / powf(2.0f, MOVEMENT_LERP_SPEED);
		const int minYDiff = SECTOR_SIZE;

		float ySpeedTargetIdle = 0.0f;
		Vector3 targetPosIdle = targetInfo.targetPos;

		// TargetSpeed basierend auf State berechnen (immer, nicht nur wenn kein Inertie!)
		float targetSpeed = 0.0f;
		float idleTargetY = 0.0f;
		switch (currentState)
		{
			case GunShipState::FOLLOW:
			targetSpeed = hasMoveTargetPos ? MAX_MOVE_SPEED : (horizontalDist > maxShotsRange) ? MAX_MOVE_SPEED : MAX_MOVE_SPEED * 0.25f;
			
			// FOLLOW: Heli folgt der Höhe des Ziels, um es zu überragen (wie in IDLE/EVADE).
			// Escape/MoveTarget: auf Ziel-Höhe fliegen (keine Hover-Offset) -> waagerechter, natuerlicher Anflug
			// (sonst steigt/fällt der Heli langsam -> sieht aus wie am Seil hochgezogen).
			if (hasMoveTargetPos)
				idleTargetY = moveTargetPos.y;
			else
				idleTargetY = targetPosIdle.y - HOVER_HEIGHT_OFFSET;
			if (fabsf(item->Pose.Position.y - idleTargetY) > minYDiff)
				ySpeedTargetIdle = (idleTargetY > item->Pose.Position.y) ? FLY_DOWN_SPEED : -FLY_UP_SPEED;
			currentYSpeed += (ySpeedTargetIdle - currentYSpeed) * yLerpAlpha;
			
			break;

			case GunShipState::IDLE:
			targetSpeed = 0.0f;
	
				// Heli schwebt etwas über dem Ziel, damit er beim Schießen nach unten zielen kann.
				idleTargetY = targetPosIdle.y - HOVER_HEIGHT_OFFSET;
				if (fabsf(item->Pose.Position.y - idleTargetY) > minYDiff)
					ySpeedTargetIdle = (idleTargetY > item->Pose.Position.y) ? FLY_DOWN_SPEED : -FLY_UP_SPEED;

			currentYSpeed += (ySpeedTargetIdle - currentYSpeed) * yLerpAlpha;
			
			break;

			case GunShipState::EVADE_NEAR:
			{
			targetSpeed = MAX_MOVE_SPEED * 2.5f;
			
			// Beim Evaden (Rückwärtsfliegen) leicht aufsteigen, damit das Heck beim Nachhintenkippen nicht in den Boden stößt.
				float evadeTargetY = targetPosIdle.y - EVADE_RAISE_HEIGHT;
				if (fabsf(item->Pose.Position.y - evadeTargetY) > minYDiff)
					ySpeedTargetIdle = (evadeTargetY > item->Pose.Position.y) ? FLY_DOWN_SPEED : -FLY_UP_SPEED;
				else
					ySpeedTargetIdle = 0.0f;
			
			currentYSpeed += (ySpeedTargetIdle - currentYSpeed) * yLerpAlpha;
			break;
			}

			case GunShipState::ESCAPE:
			targetSpeed = MAX_MOVE_SPEED;
			
			// Escape: Y frei an das Ziel anfliegen (nur FixYPosition clampt auf Raumdecke/-boden).
			idleTargetY = moveTargetPos.y;
			if (fabsf(item->Pose.Position.y - idleTargetY) > minYDiff)
				ySpeedTargetIdle = (idleTargetY > item->Pose.Position.y) ? FLY_DOWN_SPEED : -FLY_UP_SPEED;
			currentYSpeed += (ySpeedTargetIdle - currentYSpeed) * yLerpAlpha;
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

		// currentSpeed aus targetSpeed und pitchRatio.
		// ESCAPE ist ein entschlossener Notfall-Ausflug: Geschwindigkeit ist vom (kosmetischen)
		// Pitch-Ramp entkoppelt. Sonst crawlt der Heli (Pitch-Ramp + Inertie + !isMoving-Daempfung)
		// und bleibt vor der Wand stehen, statt zum Escapetarget zu fliegen.
		float currentSpeed = (currentState == GunShipState::ESCAPE) ? targetSpeed : targetSpeed * pitchRatio;

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

			case GunShipState::ESCAPE:
			// Escape: Pitch wie FOLLOW (Nase leicht hoch), damit currentSpeed > 0 bleibt und der Heli sich bewegt.
			pitchTarget = (float)DEG_TO_RAD(MAX_PITCH_DEG);
			CalculatePitchAndBank(*item, currentState, targetInfo.targetPos, 0.0f, pitchTarget, bankTarget);
			break;

			case GunShipState::EVADE_NEAR:
			pitchTarget = -(float)DEG_TO_RAD(MAX_PITCH_DEG);
			CalculatePitchAndBank(*item, currentState, targetInfo.targetPos, 0.0f, pitchTarget, bankTarget);
			break;

			case GunShipState::IDLE:
			// Heli neigt sich so, dass die Heckwaffe auf die Unterschenkel des Ziels zeigt.
			pitchTarget = CalculateIdlePitch(*item, targetInfo.targetPos);
			bankTarget = 0.0f;
			break;

			default:
			pitchTarget = 0.0f;
			bankTarget = 0.0f;
			break;
		}

		// Bewegungsrichtung (FOLLOW: zum Ziel, EVADE: vom Ziel weg), normalisiert.
		Vector3 moveDir(0.0f, 0.0f, 0.0f);
		if (horizontalDist > 1.0f)
		{
			moveDir = Vector3(targetInfo.targetPos.x - item->Pose.Position.x, 0.0f, targetInfo.targetPos.z - item->Pose.Position.z);
			moveDir = moveDir * (1.0f / horizontalDist);
			if (currentState == GunShipState::EVADE_NEAR)
				moveDir = -moveDir;
		}

		bool blocked = false;
		bool overfly = false;
		// Ausweich-Kaskade (SweptFootprintClear + FindBestAvoidanceDirection + Overfly) NUR fuer EVADE_NEAR.
		// FOLLOW (Vorwaertsflug) hat aktuell KEINEN Wand-Check (testweise deaktiviert); bei Wand fliegt er durch.
		// ESCAPE: kein Wand-/Ausweich-Check -> verpflichtender, gerader Notflug zum (vorher validierten) Escapetarget,
		// notfalls durch Wand/Geometrie. Sonst blockiert die Kaskade in engen Spaenen (Waende links/rechts/hinten) die
		// Flucht und der Heli steht still, statt in den freien Raum zu gelangen (z.B. ueber Lara hinwegfliegen).
		if (isMoving && horizontalDist > 1.0f && currentState != GunShipState::FOLLOW && currentState != GunShipState::ESCAPE)
		{
			// Probedistanz: bewegungsgeschwindigkeit-basiert, mindestens 1 Sektor (kein Wand-Tunneling).
			float probeDistance = (currentSpeed > (float)SECTOR_SIZE) ? currentSpeed : (float)SECTOR_SIZE;

			// Preferred Richtung (Combat-Distanz / MoveTo) auf Flight-Clearance testen.
			if (!SweptFootprintClear(*item, moveDir * probeDistance))
			{
				// Preferred blockiert -> nahe Ausweich-Richtung waehlen (EVADE_NEAR).
				// Collision-Vermeidung hat Vorrang vor Combat-Distanz.
				Vector3 avoidDir = moveDir;
				if (!FindBestAvoidanceDirection(*item, moveDir, probeDistance, avoidDir))
				{
					// Keine freie horizontale Richtung -> vertikal ueberfliegen (nur mit Decken-Clearance).
					if (SweptFootprintClear(*item, moveDir * probeDistance + Vector3(0.0f, -EVADE_OVERFLY_HEIGHT, 0.0f)))
						overfly = true;
					else
						blocked = true;
				}
				else
				{
					moveDir = avoidDir;
				}
			}
			else if (!SweptFootprintClear(*item, moveDir * probeDistance * 2.0f))
			{
				// Weiche Blockade weiter vorne -> Tempo reduzieren.
				currentSpeed *= 0.5f;
			}

			// Nose-Orientierung: Overfly -> in Bewegungsrichtung anvisen (sonst Ziel bleibt anvisiert).
			if (overfly)
			{
				Vector3 overPoint = item->Pose.Position.ToVector3() + moveDir * probeDistance;
				overPoint.y = targetInfo.targetPos.y - EVADE_OVERFLY_HEIGHT;
				targetOrient = Geometry::GetOrientToPoint(item->Pose.Position.ToVector3(), overPoint);
			}
		}

		// Finale Validierung der tatsaechlichen Bewegung (fixt Wand-Durchflug) NUR fuer EVADE_NEAR.
		// Overfly: Bewegung ist horizontal + vertikal, daher den erhohten Endpunkt validieren.
		// FOLLOW/ESCAPE: uebersprungen (kein Wand-Check; bei Wand -> fliegt er durch, kein Vorwaertsflug-Stop).
		if (isMoving && !blocked && currentState != GunShipState::FOLLOW && currentState != GunShipState::ESCAPE)
		{
			Vector3 finalDisplacement = moveDir * currentSpeed;
			if (overfly)
				finalDisplacement += Vector3(0.0f, -EVADE_OVERFLY_HEIGHT, 0.0f);

			if (!SweptFootprintClear(*item, finalDisplacement))
				blocked = true;
		}

		// Debug: Escapetarget (weisse Kugel) immer anzeigen, solange Escape aktiv ist
		// (auch waehrend des ESCAPE-Flugs, nicht nur bei Blockade) -> Ziel-Beobachtung beim Test.
		if (GunShipEscape.TargetPos != Vector3::Zero)
			DrawDebugSphere(GunShipEscape.TargetPos, 43, Vector4::One, RendererDebugPage::None);

		if (blocked)
		{
			const GunShipState blockedState = currentState;

			currentSpeed = 0.0f;
			item->ItemFlags[3] = 0;

			currentState = GunShipState::IDLE;

			// Auto-Escape: Heli kann nicht weiter (Wand / Roombound) -> Escapetarget (>= Shoot-Reichweite,
			// in erreichbaren Raumen, mit Clearance) suchen und dorthin fliegen (via MoveTarget). Dort
			// angekommen wird der Escape-Zustand geleert und der Kampf automatisch fortgesetzt.
			// NUR aus EVADE_NEAR neu loesen: Im ESCAPE-Flug bleibt der Heli am aktuellen Ziel verpflichtet
			// (bis Erreichen/Timeout) und tauscht es nicht mehr permanent aus -> verhindert den
			// EVADE_NEAR<->ESCAPE-Stuck-Loop (Heli springt sonst ohne Fortschritt zwischen beiden hin und her).
			if (blockedState == GunShipState::EVADE_NEAR && hasShootTarget && shootTargetNum >= 0)
			{
				Vector3 escapeTarget = Vector3::Zero;
				if (FindEscapeTarget(*item, g_Level.Items[shootTargetNum].Pose.Position.ToVector3(), (float)(maxShotsRange + SECTOR_SIZE), GunShipEscape.TargetPos, ESCAPE_EXCLUDE_RADIUS, escapeTarget))
				{
					GunShipEscape.Active = true;
					GunShipEscape.TargetPos = escapeTarget;
					GunShipEscape.Frames = 0; // Neues Ziel -> frisches Timeout-Fenster (sonst laeuft Frames aus dem Reset mit).
					item->ItemFlags[7] = 0; // Evade-Flag zuruecksetzen, damit der ESCAPE-State nicht auf EVADE_NEAR gezwungen wird.
				}
			}

			FixYPosition(item);

			pitchTarget = 0.0f;
			bankTarget = 0.0f;

			// Animation trotz blocked status fortsetzen
			AnimateItem(item);
			
			// Kollisionsprüfung nach Ausweichbewegung um zu prüfen ob freie Sicht vorliegt
			blocked = CheckFootprintCollision(*item, moveDir * SECTOR_SIZE);
			
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
			case GunShipState::ESCAPE:
				CalculatePitchAndBank(*item, currentState, targetInfo.targetPos, 0.0f, pitchTarget, bankTarget);
				break;
			case GunShipState::EVADE_NEAR:
				pitchTarget = -(float)DEG_TO_RAD(MAX_PITCH_DEG);
				CalculatePitchAndBank(*item, currentState, targetInfo.targetPos, 0.0f, pitchTarget, bankTarget);
				break;
			case GunShipState::IDLE:
				// Heli neigt sich so, dass die Heckwaffe auf die Unterschenkel des Ziels zeigt.
				pitchTarget = CalculateIdlePitch(*item, targetInfo.targetPos);
				bankTarget = 0.0f;
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
						// moveDir enthaelt die aktive Flug-Richtung (Preferred Richtung oder Ausweich-Richtung).
						item->Pose.Position.x += (int)(moveDir.x * moveDist);
						item->Pose.Position.z += (int)(moveDir.z * moveDist);
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
						// moveDir enthaelt die aktive Ausweich-Richtung (Rueckwaerts, lateral oder Overfly).
						item->Pose.Position.x += (int)(moveDir.x * moveDist);
						item->Pose.Position.z += (int)(moveDir.z * moveDist);
					}

					break;

					case GunShipState::ESCAPE:
					if (horizontalDist > 1.0f)
					{
						// Escape: direkt zum Escapetarget fliegen (XZ; Y wird separat behandelt).
						item->Pose.Position.x += (int)(moveDir.x * moveDist);
						item->Pose.Position.z += (int)(moveDir.z * moveDist);
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
			// Y-Schritt nur anwenden, wenn der verschobene Fussabdruck frei ist (seitliche Waende/Erhohungen).
			int yDelta = (int)currentYSpeed;
			if (overfly)
			{
				// Overfly: auf die erhohte Hohe ueber Lara steigen.
				float overflyTargetY = targetInfo.targetPos.y - EVADE_OVERFLY_HEIGHT;
				float dy = overflyTargetY - (float)item->Pose.Position.y;
				yDelta = (fabsf(dy) > 1.0f) ? ((dy < 0.0f) ? -FLY_UP_SPEED : FLY_DOWN_SPEED) : 0;
			}
			if (yDelta != 0 && CheckFootprintCollision(*item, Vector3(0.0f, (float)yDelta, 0.0f)))
				yDelta = 0;

			item->Pose.Position.y += yDelta;

			FixYPosition(item);

			// Nur fuer FOLLOW/EVADE: setzt Bewegungs-Pitch (+/- MAX_PITCH) und wuerde den IDLE-Zielpitch ueberschreiben.
			if (currentState != GunShipState::IDLE)
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
				// Pitch/Bank-Rueckstellung auf waagerecht nur außerhalb von IDLE: IDLE haelt den Zielpitch auf die Unterschenkel.
				if (currentState != GunShipState::IDLE)
				{
					currentPitch *= 0.95f;
					currentBankAngle *= 0.95f;
					item->ItemFlags[1] = (int)(currentPitch * FLOATING_POINT_SCALE);
					item->ItemFlags[2] = (int)(currentBankAngle * FLOATING_POINT_SCALE);
				}

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

			// Waehrend eines MoveTarget/Escape-Flugs NICHT auf das Shoot-Target feuern (Heli fliegt dorthin, statt zu angreifen).
		const bool hasShootTargetInRange = hasShootTarget && !hasMoveTargetPos && shootHLen <= maxShotsRange;
		

			if (hasShootTargetInRange)
			{
				// Feuersperre: nur abfeuern, wenn ein Ziel (Lara/Static) in der Schusslinie ist und keine Wand dazwischen.
				auto gateMuzzle = GetJointPosition(item, 8, Vector3i::Zero);
				auto gateOrigin = GameVector(gateMuzzle.ToVector3(), item->RoomNumber);
				auto gateRot = EulerAngles(item->Pose.Orientation.x + ANGLE(8.0f), item->Pose.Orientation.y, item->Pose.Orientation.z).ToRotationMatrix();
				Vector3 gateAimed = gateMuzzle.ToVector3() + Vector3::Transform(Vector3(0.0f, 0.0f, -maxShotsRange * 2), gateRot);
				auto gateTarget = GameVector(gateAimed, g_Level.Items[shootTargetNum].RoomNumber);
				auto gateClamped = gateTarget;
				LOS(&gateOrigin, &gateClamped);

				StaticMesh* gateMesh = nullptr;
				Vector3i gateHitPos = Vector3i::Zero;
				int gateResult = ObjectOnLOS2(&gateOrigin, &gateClamped, &gateHitPos, &gateMesh, ID_LARA, item->Index);
				const bool canFire = (gateResult != NO_LOS_ITEM);

				// Sound for gunfire + gun flash visual (nur beim Abfeuern).
				if (canFire)
				{
					if (!(GlobalCounter & (FIRE_RATE - 1)) && item->ItemFlags[0] > FIRE_RATE)
						SoundEffect(SFX_TR4_HK_FIRE, &item->Pose, SoundEnvironment::Land, 0.8f);

					if (item->ItemFlags[0] > FIRE_RATE)
						item->MeshBits |= 0x100;
					else
						item->MeshBits &= 0xFEFF;
				}

				// Use mesh 8 (gun neck) joint as muzzle point.
				auto muzzleJoint = GetJointPosition(item, 8, Vector3i::Zero);
				auto flashPos = muzzleJoint.ToVector3();

				if (canFire)
				{
					auto lightColor = Vector3(Random::GenerateFloat(0.75f, 0.85f), Random::GenerateFloat(0.5f, 0.6f), 0.0f) * 255;
					SpawnDynamicLight(flashPos.x, flashPos.y, flashPos.z, 10, lightColor.x, lightColor.y, lightColor.z);

					auto weaponType = LaraWeaponType::HK;
					// Spawn gun shell effect at the muzzle using generic function.
					TriggerGunShellAt(Vector3i(flashPos.x, flashPos.y, flashPos.z), item->RoomNumber, ID_GUNSHELL, weaponType);
					TriggerGunSmoke(flashPos.x, flashPos.y, flashPos.z, 0, 0, 0, 0, weaponType, 16);
				}

				// Determine line of sight from the muzzle.
				// Apply a small forward offset so that the gunships own hitbox does not block LOS.
				const float aimSpread = BLOCK(0.2f); // 512 world units ≈ 0.5 BLOCK

				auto rotMatrix = EulerAngles(item->Pose.Orientation.x + ANGLE(8.0f), item->Pose.Orientation.y, item->Pose.Orientation.z).ToRotationMatrix();

				// Use the offset position as LOS origin.
				auto origin = GameVector(flashPos, item->RoomNumber);

				// Apply aim spread (horizontal) around that forward point.
				float spreadX = Random::GenerateFloat(-aimSpread, aimSpread);
				float spreadY = Random::GenerateFloat(-aimSpread, aimSpread);
				float spreadZ = Random::GenerateFloat(-aimSpread, aimSpread);
				Vector3 aimedPos = flashPos + Vector3::Transform(Vector3(spreadX, spreadY, spreadZ) + Vector3(0.0f, 0.0f, -maxShotsRange * 2), rotMatrix);

				auto targetVec = GameVector(aimedPos, g_Level.Items[shootTargetNum].RoomNumber);

				// Geometrie-Check zuerst: LOS klappt den Strahl an der Wand ab und fuellt LosRoomNumbers (wird von ObjectOnLOS2 genutzt).
				auto target2 = targetVec;
				LOS(&origin, &target2);

				GetFloor(target2.x, target2.y, target2.z, &target2.RoomNumber);

				// Objekt-Treffer (Lara/Statics) nur innerhalb des wandbegrenzten Segments -> kein Durchschuss durch Waende.
				StaticMesh* mesh = nullptr;
				Vector3i hitPos = Vector3i::Zero;
				int losResult = ObjectOnLOS2(&origin, &target2, &hitPos, &mesh, ID_LARA, item->Index);

				bool hasHit = (losResult != NO_LOS_ITEM);

				DrawDebugLine(origin.ToVector3(), targetVec.ToVector3(), Vector4::One, RendererDebugPage::None);

				// Nur bei Treffer auf ein Ziel in der Schusslinie weiterverarbeiten (Wand davor -> kein Schuss, kein Decal).
				if (hasHit)
				{
					// Something is in the way.
					if (losResult < 0)
					{
						// Hit static mesh.
						if (mesh && Statics[mesh->Slot].shatterType != ShatterType::None)
						{
							mesh->HitPoints -= GUNSHIP_DAMAGE;
							ShatterImpactData.impactDirection = Vector3(0, 0, 0);
							ShatterImpactData.impactLocation = Vector3(hitPos.x, hitPos.y, hitPos.z);
							int shatterRoomNumber = FindRoomNumber(Vector3i(hitPos), mesh->RoomNumber, true);
							ShatterObject(nullptr, mesh, 128, shatterRoomNumber, 0);
							SoundEffect(GetShatterSound(mesh->Slot), &mesh->Pose);
						}
						// Ricochet spark at hit position.
						GameVector impactPos(hitPos.x, hitPos.y, hitPos.z, origin.RoomNumber);
						TriggerRicochetSpark(impactPos, Random::GenerateAngle());
					}
					else
					{
						// Hit an item (creature or object).test212
						auto* item1 = &g_Level.Items[losResult];

						if (item1->Index == LaraItem->Index || item1->IsCreature())
						{
							DoDamage(item1, GUNSHIP_DAMAGE);
						}

						GameVector impactPos(hitPos.x, hitPos.y, hitPos.z, origin.RoomNumber);
						TriggerRicochetSpark(impactPos, Random::GenerateAngle());

					}

				}
			}

			// Not in range – ensure flash is cleared.
			item->MeshBits &= 0xFEFF;


			// Y-Geschwindigkeit immer mit FLOATING_POINT_SCALE persistieren (ItemFlags[6] wird mit /FLOATING_POINT_SCALE gelesen).
			item->ItemFlags[6] = (int)(currentYSpeed * FLOATING_POINT_SCALE);

			AnimateItem(item);
			
		}
	}
}
