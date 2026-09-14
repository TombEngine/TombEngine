# Progress

## Current Task
Gunship (TR5): Kollisions-Refactoring – richtungs-unabhängiger Bounding-Box-Fussabdruck-Check. FOLLOW-UP: (a) `BLOCKABLE`-Check entfernt (falsch-positiv bei 1-Square-Schritten), (b) Probe-Distanz auf `currentSpeed` (Tunneling bei EVADE behoben).

## Completed Work
- **Root Cause:** Kollisions-Proben (`CheckEarlyBlocking`, `CheckForwardCollision`) liefen entlang der MODELL-Vorwärtsrichtung (`forwardVec` aus `Pose.Orientation.y`) statt der tatsächlichen Bewegungsrichtung (`dirToTarget`). Bei FOLLOW/EVADE-Bewegung zum/vom Ziel blieben Wände/Boxen in der Bewegungsrichtung unentdeckt.
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

## Modified Files
- `Objects/TR5/Entity/tr5_gunship.cpp`

## Next Step
- In Szene testen: (a) Heli fliegt auf Ziel zu → fliegt über 1-Square-Schritte (kein falsch-positiver Stopp), stoppt VOR Wänden/hoher Geometrie; (b) EVADE (Rückwärts) → erkennt Wände in Rückwärtsrichtung, kein Tunneling; (c) Hohe Geometrie (erhohte Squares) → Heli stoppt (Clearance-Check).

## Blockers
- Keine.
