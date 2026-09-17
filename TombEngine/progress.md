# Progress

## Current Task
Gunship (TR5): Escape-Stuck-Loop behoben. Wenn der Heli WÄHREND des ESCAPE-Flugs blockiert ist, wurde `TargetPos` nie neu gewählt (Trigger feuerte nur bei `blockedState == EVADE_NEAR`) → deterministische `FindEscapeTarget` wählte nach Timeout denselben unerreichbaren Punkt → Endlos-Loop. **Fix:** Re-Targeting jetzt auch im ESCAPE-State, Ausschluss des letzten fehlgeschlagenen Targets, robusterer Pfad-Check. Fertig – wartet auf In-Szene-Test.

## Completed Work
- **Gunship (TR5): Escape-Stuck-Loop-Fix (`tr5_gunship.cpp`):**
  - **Trigger erweitert (Zeile ~810):** `blockedState == EVADE_NEAR || blockedState == ESCAPE` → bei Blockade im Escape-Flug wird neu gesucht.
  - **Neue Signature `FindEscapeTarget`:** zusätzlich `const Vector3& excludePos, float excludeRadius` → Kandidaten im Ausschlussradius (XZ) um `excludePos` werden übersprungen. Aufruf gibt `GunShipEscape.TargetPos` als excludePos → letztes fehlgeschlagenes Ziel wird nicht erneut gewählt.
  - **Neue Konstante `ESCAPE_EXCLUDE_RADIUS = SECTOR_SIZE * 2.0f`** (nach `MOVE_TARGET_REACH_RADIUS`).
  - **Pfad-Check verstärkt:** `disp * 0.5f` ersetzt durch `disp * 0.33f` ODER `disp * 0.66f` ODER `disp * 0.9f` → Blockade im letzten Stück vor dem Ziel wird erkannt.
  - **`GunShipEscape.Frames = 0`** beim Escape-Trigger → frisches Timeout-Fenster pro neuem Ziel.
  - **Aufgeräumt:** kaputtes `if (cand == outPos) { Vector3 disp = cand - heliPos/2; }`-Fragment (Re-Deklaration, nicht kompilierbar) durch den Ausschluss-Block ersetzt.
  - **Nicht kompiliert** (Regel: Build nur auf ausdrückliche Anfrage; Build-Errors meldet der Nutzer).
  - **Stand der Konstanten (benutzerseitig):** `MOVE_TARGET_REACH_RADIUS = SECTOR_SIZE * 0.5f` (~0,5 Block horizontale Toleranz), `disp.Length() < SECTOR_SIZE * 2.0f` als Mindest-Fluchtdistanz in `FindEscapeTarget`.

## Modified Files
- `Objects/TR5/Entity/tr5_gunship.cpp`

## Next Step
- In-Szene-Test: Heli EVADE → Blockade → ESCAPE → (optional) Blockade im Escape-Flug → **neues** Escapetarget (altes ausgeschlossen) → Ankunft (Radius ~0,5 Block) → Kampf wird fortgesetzt. Kein Stuck-Loop mehr.

## Blockers
- Keine.

## Previous Completed Work
- **Gunship (TR5): ESCAPE-State für den Escape-Flug (`tr5_gunship.cpp`):**
  - **Neuer `GunShipState::ESCAPE` (=3):** eigener State für den Escape-Flug (statt FOLLOW-Reuse). `DetermineGunShipState`: `hasMoveTargetPos` → `ESCAPE`.
  - **Bug-Fix (Stuck):** Evade-Flag (`ItemFlags[7]`) wurde beim Escape-Trigger NICHT zurückgesetzt → `if (ItemFlags[7]==1) currentState=EVADE_NEAR` zwang den State auf EVADE_NEAR → Heli steckte im EVADE-Loop. Fix: (a) `ItemFlags[7]=0` im Escape-Trigger, (b) Override-Bedingung `&& currentState != ESCAPE`.
  - **ESCAPE-Bewegung:** `targetSpeed=MAX_MOVE_SPEED`, Y auf Ziel-Höhe (frei; `FixYPosition` clampt auf Raumdecke/-boden), Pitch wie FOLLOW (`CalculatePitchAndBank`: ESCAPE→FOLLOW-Fall, damit `currentSpeed>0`), XZ-Bewegung zum Escapetarget.
  - **Build-Fix (C2360):** `float evadeTargetY` im `EVADE_NEAR`-Case wurde durch den `ESCAPE`-Case-Label übersprungen → `EVADE_NEAR`-Case jetzt in `{}` eingeschlossen (scope-local).
  - **Nicht kompiliert** (Regel: Build nur auf ausdrückliche Anfrage; Build-Errors meldet der Nutzer).
- **Gunship (TR5): Escape-Anflug „Seil" – Rückbau + waagerechter Flug (`tr5_gunship.cpp`):**
  - **Rückbau:** `targetOrient`-Bedingung `!hasMoveTargetPos` entfernt → Nase zeigt wieder auf **Lara** (Nase→Flugziel sah im Test SCHLECHTER aus).
  - **Waagerechter Anflug:** `FOLLOW`-Fall: bei `hasMoveTargetPos` → `idleTargetY = moveTargetPos.y` (Ziel-Höhe), sonst `targetPosIdle.y - HOVER_HEIGHT_OFFSET`. So fliegt der Heli waagerecht statt mit Hover-Offset auf-/zufallen → kein „langsam hochgezogen".
  - **Nicht kompiliert** (Regel: Build nur auf ausdrückliche Anfrage; Build-Errors meldet der Nutzer).
- **Gunship (TR5): Auto-Escape „steckt in IDLE fest" Bugfix (`tr5_gunship.cpp`):**
  - **Root Cause:** Im `FOLLOW` schwebt der Heli `idleTargetY = targetPosIdle.y - HOVER_HEIGHT_OFFSET` (1,5 Sektoren) UNTER dem Ziel. Der One-Shot-Clear prüfte aber die **3D-Distanz** (`dmx²+dmy²+dmz²`) < `MOVE_TARGET_REACH_RADIUS`(100) → wegen der ~1536er Vertikal-Offset nie < 100 → Escape-Zustand (`hasMoveTargetPos`/`GunShipEscape.Active`) wurde nie geklärt → `DetermineGunShipState` zwingt IDLE + Feuersperre bleibt aktiv → Heli steckt fest, feuert nicht mehr.
  - **Fix 1 (primär):** One-Shot-Clear jetzt per **horizontaler** Distanz (`dmx²+dmz²` < Radius) – konsistent mit `DetermineGunShipState` (`horizontalDistance < 100`). Heli erreicht das Escapetarget horizontal → State wird geklärt → Kampf wird fortgesetzt.
  - **Fix 2 (Sicherheitsnetz):** `GunShipEscapeData.Frames` (Zähler) + `MAX_ESCAPE_FRAMES`(240). Wenn der Escape-Flug nicht innerhalb der Frames ankommt (z.B. Pfad blockiert / Ziel nicht erreichbar) → `GunShipEscape` wird geleert → Heli gibt auf und setzt Kampf (inkl. Firing) fort.
  - **Nicht kompiliert** (Regel: Build nur auf ausdrückliche Anfrage; Build-Errors meldet der Nutzer).
- **Gunship (TR5): Fußabdruck-Gitter + Auto-Escape (`tr5_gunship.cpp`):**
  - **`CheckFootprintCollision` → 4×4-Gitter (16 Stichproben):** ersetzt die 4 Eck-Punkte durch ein Raster über die gesamte Bounding-Box-Fläche (Ecken, Kantenmitten, Innenpunkte, `FOOTPRINT_GRID=4`). Fixt „Heli fliegt durch Wände": ein großer (6–7 Sektor) Fußabdruck verfehlt mit 4 Ecken Wände, die zwischen Zentrum und Kante liegen; das Gitter deckt Innen-/Kanten-Wände ab. Jeder Punkt: `ResolveProbeRoom` → `GetPointCollision` (Wand/Floor/Ceiling/Clearance). Strenger als 4 Ecken, keine Fehl-Positiven (Punkte liegen im echten Modell-Bereich).
  - **Neue `static GunShipEscapeData GunShipEscape`** (nach `GunShipOrbit`): `Active` + `TargetPos`, `Reset()`. Nicht-persistent (Pattern wie `GunShipOrbit`).
  - **Neuer Helper `FindEscapeTarget(item, shootTargetPos, minEscapeDist, outPos)`:** Ring aus 12 Kandidaten im Umkreis des Shoot-Targets (horizontal, auf Heli-Höhe), jeder mit `CheckFootprintCollision` auf Destination + grober Pfad-Mitte geprüft (verbundene Geometrie + Clearance + Decke). Naechster gültige Punkt (wenigste Strecke) gewinnt. true=gefunden (outPos).
  - **Escape-Trigger im `blocked`-Zweig:** `blockedState == EVADE_NEAR && hasShootTarget` → `FindEscapeTarget(..., maxShotsRange + SECTOR_SIZE, ...)` → `GunShipEscape.Active = true`.
  - **Escape-Flug über MoveTarget-Pipeline:** Top von `ControlGunShip`: `GunShipEscape.Active` → `moveTargetPos`/`hasMoveTargetPos` setzen. One-Shot-Block (Erreichen < `MOVE_TARGET_REACH_RADIUS`) ruft jetzt zusätzlich `GunShipEscape.Reset()` auf → Kampf wird fortgesetzt. `DetermineGunShipState(hasMoveTargetPos=true)` → FOLLOW (fliegt zum Ziel), `targetInfo.targetPos` = Escapetarget.
  - **Feuersperre bei Escape:** `hasShootTargetInRange` bekommt `&& !hasMoveTargetPos` → Heli feuert NICHT auf das Shoot-Target, während er zum Escapetarget fliegt (wird dort auch anvisiert).
  - **Nicht kompiliert** (Regel: Build nur auf ausdrückliche Anfrage; Build-Errors meldet der Nutzer).
- **Gunship (TR5): Lokales Flight-Steering + Clearance-Refactor (`tr5_gunship.cpp`):**
  - **`ResolveProbeRoom(pos, startRoom)`** (neu, vor `CheckFootprintCollision`): aktueller Raum oder verbundener Nachbarraum per `IsPointInRoom` + `RoomData::NeighborRoomNumbers`, sonst `NO_VALUE`. Deckt Horizon/Outdoor-Areas ohne verbundenen Room ab.
  - **`CheckFootprintCollision` erweitert:** jede der 4 Bounding-Box-Ecken wird erst per `ResolveProbeRoom` auf gültige verbundene Geometrie geprüft (`NO_VALUE` → Kollision) und dann per `GetPointCollision(pos, probeRoom)` gegen Wand/Floor/Ceiling/Clearance. Behält echte Modell-Bounds. Fixt Portal-Traversal, zu schmale Portale, Horizon-Flug.
  - **`FindBestAvoidanceDirection(item, preferredDir, probeDist, outDir)`** (neu): 5 Kandidaten (0/±45/±90°) um Preferred-Richtung (2D-Yaw-Rotation), je `SweptFootprintClear`; Scoring = kleinster Winkelabstand + Continuity-Bias auf `GunShipOrbit.Direction` (kein Link/Rechts-Flattern). true=frei (outDir), false=keine frei.
  - **Unified Steering in `ControlGunShip`:** Preferred (FOLLOW/MoveTo = zum Ziel, EVADE = weg) → bei Blockade `FindBestAvoidanceDirection` (ersetzt EVADE-only-Kaskade) → sonst Overfly (vertikal, nur mit Decken-Clearance) → sonst `blocked` (Hover/IDLE). Gilt nun auch für FOLLOW/MoveTo.
  - **FOLLOW-Bewegung nutzt jetzt `moveDir`** (davor eigene Ziel-Zuformel) → wendet die Ausweich-Richtung korrekt an.
  - **Finale Validierung Overfly-aware:** validiert `moveDir*currentSpeed` (+ `-EVADE_OVERFLY_HEIGHT` bei Overfly) → fixt dass Overfly nicht von der waagerechten Final-Prüfung wieder geblockt wird.
  - **`avoidDir`-Temp beim `FindBestAvoidanceDirection`-Aufruf** (vermeidet Self-Aliasing von `moveDir` als Input+Output).
  - **`#include "Game/room.h"`** ergänzt (für `IsPointInRoom`/`NeighborRoomNumbers`).
  - **Nicht kompiliert** (Regel: Build nur auf ausdrückliche Anfrage; Build-Errors meldet der Nutzer).
- **Gunship (TR5): Kontinuierliche Orbit-Ausweichbewegung (`tr5_gunship.cpp`):**
  - **Neue `static GunShipOrbitData GunShipOrbit`** (nach `enum GunShipState`): `Direction` (+1/-1, Umlaufrichtung) + `Initialized`. Persistiert NICHT im Savegame (Pattern wie `Willard.cpp`) – `ItemFlags` sind voll (0–7), daher statisch.
  - **Neuer Helper `SweptFootprintClear(item, displacement)`** (nach `CheckFootprintCollision`): prüft den Fussabdruck entlang des kompletten x+y+z-Verschiebungspfades in Sub-Schritten à max. `SECTOR/4` (nutzt `CheckFootprintCollision` pro Schritt) → kein Wand-Tunneling/Clippen bei schnellen Bewegungen. `true` = frei.
  - **EVADE-Bewegung: vorrangig rückwärts (radial):** `moveDir` = Richtung zum Ziel, in `EVADE_NEAR` negiert (weg vom Ziel) → Heli flieht geradlinig rückwärts. **Nur bei Blockade** greift die Kaskade: Stage 1 = lateral (Orbit – als `lateral` bei sich drehender Radiale eine weiche, kontinuierliche Umlaufrichtung um Lara), Stage 2 = Overfly (drüberfliegen). `GunShipOrbit.Direction` (+1/-1) hält die Umlaufrichtung über Frames stabil (kein Hin-/Her-Kippen); wird in der Kaskade je nach gewählter Laterale gesetzt. (Frühere Variante mit radial+tangential-Spirale als Primär-Bewegung entfernt – Heli orbitierte dann zu vorrangig statt rückwärts zu fliehen.)
  - **Kaskade nutzt jetzt `SweptFootprintClear`** (statt `CheckFootprintCollision`) + setzt `GunShipOrbit.Direction = ±1` je nach gewählter Laterale → nächste Frame setzt die Spirale in derselben Richtung fort (Orbit-Andenken).
  - **Finale Validierung nach Kaskade:** `if (isMoving && !blocked && !SweptFootprintClear(*item, moveDir * currentSpeed)) blocked = true;` – fixt den Overfly-Durchflug (Kaskade wählte eine niedrige Laterale, die in die blockierende Wand flog; Root Cause: Overfly prüfte nur den hohen Endpunkt, bewegte sich aber erst lateral in niedriger Höhe).
  - **Nicht kompiliert** (Regel: Build nur auf ausdrückliche Anfrage; Build-Errors meldet der Nutzer).
- **C2338 static_assert in `std::variant` (Zeile 309, `ControlGunShip`):** `PropertyHandler::Get(*item, "GunshipMoveTarget", Vector3::Zero)` → `..., Vec3())`. Kausalkette: `Vector3::Zero` = `DirectX::SimpleMath::Vector3` → `Get<T>` deduziert `T=Vector3` → `ExtractValue<T>` (PropertyValue.h:30) ruft `std::get_if<Vector3>` → aber `PropertyValue = std::variant<bool, float, std::string, Vec2, Vec3, ScriptColor, Rotation, Time>` enthält nur TEN's `Vec3` (nicht DirectX `Vector3`) → `Vector3` kommt 0× vor → static_assert. Der Fehler zeigt auf die MSVC-`<variant>`-Header, liegt aber in `tr5_gunship.cpp` (MSVC meldet Template-`static_assert` an der Definition, nicht am Aufruf). `Vec3` → `Vector3` läuft über `operator Vector3() const` (Vec3.h:67). Zeile 320 `Properties.Set(..., Vec3())` bestätigt: Property wird als `Vec3` gespeichert. **Regel:** Fallback-Default bei `PropertyHandler::Get` muss ein Mitglied der `PropertyValue`-Variante sein (Vektor → `Vec2`/`Vec3`, nicht `Vector2`/`Vector3`).
- **Build-Warnung con.5 (Zeile 283, `CalculateIdlePitch`):** `float maxPitch = (float)DEG_TO_RAD(MAX_PITCH_DEG);` → `constexpr float maxPitch = ...;`. `DEG_TO_RAD` ist `constexpr` (Math/Legacy.h:41) und `MAX_PITCH_DEG` ist `constexpr int` → Initialisierer ist ein Konstanten-Ausdruck. MSVC meldete: "Function 'DEG_TO_RAD' is marked constexpr. Mark the variable 'maxPitch' as constexpr if compile-time evaluation is desired (con.5)". Weitere `DEG_TO_RAD`-Stellen (226/228/230/239/242/262/264/281) sind keine Warn-Trigger: sie sind entweder Zuweisungen an Referenz-Parameter oder beinhalten Laufzeit-Werte (`crossY`, `dy`, `hDist`).
- **EVADE-Wand-Bug – Fix 1 (Blind-Push-Back entfernt):** Der 32-Unit-Rückdruck bei Blockade greift jetzt NUR in `FOLLOW` (Escape = weg vom Ziel) und nur, wenn `CheckFootprintCollision(escapeDir * 32)` frei ist. In `EVADE_NEAR` bleibt der Heli bei Blockade stehen (IDLE + Recheck) statt in die blockierende Wand gedrückt zu werden.
- **EVADE-Wand-Bug – Fix 2 (Y-Bewegungsschutz):** Der vertikale Schritt `yDelta = (int)currentYSpeed` wird vor Anwendung durch `CheckFootprintCollision(*item, Vector3(0, yDelta, 0))` geprüft; bei Kollision mit seitlichen Wänden/Erhöhungen → `yDelta = 0` (statt Clippen).
- **EVADE-Wand-Bug – Fix 3 (Fussabdruck mit Y-Versatz):** `CheckFootprintCollision` berücksichtigt jetzt `displacement.y` (Band-Ober-/Unterkante + Probe-midY) und prüft exakte Durchdringung pro Ecke: `floorHeight < Band-Unten` (Heli unter dem Boden) bzw. `ceilingHeight > Band-Oben` (Heli über der Decke), jeweils mit `NO_HEIGHT`-Guard.
- **Konventions-Verifikation (wichtig):** `BoundingBox.Y1 = Center − Extents` (OBERE KANTE, kleineres Y), `Y2 = Center + Extents` (UNTERE KANTE, größeres Y) – belegt durch `GameBoundingBox::Rotate` (min/max) + Aabb-Konstruktor. In `CheckFootprintCollision` sind die Namen daher vertauscht: `bottomMeshY (= pos.y + Y1)` = TOP, `topMeshY (= pos.y + Y2)` = BOTTOM. Mathematisch konsistent; `bandHeight = Y2 − Y1 > 0` ✓.
- **API-Verifikation:** `Vector3(float,float,float)`-Konstruktor, `Vector3 * float`, `GetPointCollision(Vector3, short)`, `IsWall()`, `GetFloorHeight()`/`GetCeilingHeight()`, `NO_HEIGHT` (`INT_MIN + UCHAR_MAX`) – alles vorhanden und im File bereits konsistent genutzt.
- **Nicht kompiliert** (Regel: Build nur auf ausdrückliche Anfrage; Build-Errors meldet der Nutzer).
- **Root Cause (vorher):** Kollisions-Proben (`CheckEarlyBlocking`, `CheckForwardCollision`) liefen entlang der MODELL-Vorwärtsrichtung (`forwardVec` aus `Pose.Orientation.y`) statt der tatsächlichen Bewegungsrichtung (`dirToTarget`). Bei FOLLOW/EVADE-Bewegung zum/vom Ziel blieben Wände/Boxen in der Bewegungsrichtung unentdeckt.
- **Neuer Helper `CheckFootprintCollision(item, displacement)`:** Wandert die um `displacement` verschobene Bodenebene des Helikopters (4 Yaw-rotierte Bounding-Box-Ecken) gegen:
  - `pointColl.IsWall()` (Wand-Sektor),
  - Clearance: `abs(Ceiling - Floor) <= bandHeight` (erhohte Squares / abgesenkte Decken).
- **FOLLOW-UP Bugfix 1 (falsch-positiv bei niedrigen Schritten):** `BLOCKABLE`-Pathfinding-Box-Prüfung ENTFERNT. BLOCKABLE ist ein Begehbarkeits-Konzept (Walkability) für Laufkreaturen – falsch für einen FLIEGENDEN Heli. Niedrige Schritte (1 Square) sind oft als BLOCKABLE markiert → Heli stoppte fälschlich, obwohl er locker drüberfliegen könnte. Jetzt fliegt der Heli über niedrige Geometrie.
- **Rotation:** Ecken manuell um Yaw rotiert (`worldX = lx·cosθ + lz·sinθ`, `worldZ = -lx·sinθ + lz·cosθ`) – abgeleitet vom Ground-Truth-Forward-Vektor `(-sinθ, 0, -cosθ)` (lokal −Z = Forward). `GameBoundingBox::operator+` rotiert NICHT.
- **Build-Fix (C4838):** `heliFrame.BoundingBox.X1/X2/Z1/Z2` sind `const int` → Eck-Arrays als `int` deklariert (nicht `float`), um Narrowing in Brace-Initialisierung zu vermeiden. Float-Mathematik läuft im Loop über `yawCos`/`yawSin`.
- **Aufrufstellen refactored:**
  - `blocked` → `moveDir` (FOLLOW = zum Ziel, EVADE = weg, normalisiert); hart `×currentSpeed` → `blocked`, weich `×2·currentSpeed` → `currentSpeed *= 0.5f`.
  - Recheck nach Back-off → `CheckFootprintCollision(*item, moveDir * SECTOR_SIZE)` (fix, da nach Block `currentSpeed = 0`).
- **FOLLOW-UP Bugfix 2 (Tunneling durch Wände, v. a. EVADE):** Probe-Distanz von fixem `SECTOR_SIZE` auf `currentSpeed` gesetzt. `EVADE_NEAR` = `MAX_MOVE_SPEED * 2.5f` → Heli bewegt sich >1 Sektor/Frame → tunnelt durch den 1-Sektor-Probe. Jetzt Probe ≥ Bewegungs-Distanz → kein Tunneling.
- **Aufräumen:** `CheckEarlyBlocking`, `CheckForwardCollision`, `dummySpeed` entfernt.
- **Skalar-Division vermieden:** `moveDir * (1.0f / horizontalDist)` statt `Vector3 / float` (Skalar-Division im Codebase nicht belegt → sicherer).
