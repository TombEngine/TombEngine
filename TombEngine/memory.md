# Memory Bank

## Learned Patterns & Decisions

### Code Style
- C++ mit sol2-basierter Lua API, Zielplattformen: Windows, Mac, Linux.
- Domain: Echtzeit-3D-Engine für Tomb Raider-ähnliche Spiele.
-格子: Grid-based, room-based world structure mit Portalen.
- World units: 1 sector = 1024 world units ≈ 2 Meter.
- C-style casts bevorzugen (`(int)` statt `static_cast<int>`).
- Keine `_t` Typen im lokalen Code (z.B. `size_t`, `uint32_t`). Explizite Typen bevorzugen (`int`, `bool`, `float`).
- Floating-point numbers immer mit `f` postfix und Dezimalpunkt (z.B. `2.0f`).
- Namespace: keine anonymous namespaces.
- Includes: eigene `.h` → externe System-Includes → lokale Projekt-Includes (alphabetisch sortiert, gruppiert).
- Indentation: 4 spaces. Windows line endings.
- PascalCase für öffentliche Typen/Methoden. camelCase mit führendem `_` für private fields.
- `auto` bevorzugen wenn der Typ offensichtlich ist.
- One-line lambdas können zusammengefasst werden wenn sie ähnlich sind.

### Engine Architecture
- TombEngine implementiert klassische Lara-Croft-Physik (pre-TL1).
- Level-Dateien werden in einem eigenen Format verwaltet.
- Physik-System mit kollisionserkennung und character controller.
- Scripting über Lua mit sol2 — viele Skripte in `/Scripts/`.

### Coordinate System
- Y positiv ist downwards (im TEN-Koordinatensystem).

## Key Files & Modules

### TR5 Gunship (`Objects/TR5/Entity/tr5_gunship.cpp`)
- State-Machine: `FOLLOW` / `IDLE` / `EVADE_NEAR` (enum `GunShipState`), Persistenz über `ItemFlags`.
- Vertikale Bewegung über persistenten `currentYSpeed` (`ItemFlags[6]`): geschrieben mit `* FLOATING_POINT_SCALE`, gelesen mit `/ FLOATING_POINT_SCALE`. Die Skala muss beidseitig stimmen, sonst bricht die Lerp-Akkumulation zusammen (früherer Bug: im EVADE-Zweig ohne `*1000` → Heli stieg nicht auf).
- **Y-Zielhöhe in ALLEN drei States:** `FOLLOW`, `IDLE` und `EVADE_NEAR` konvergieren alle auf `targetPos.y - OFFSET` (je `HOVER_HEIGHT_OFFSET` bzw. `EVADE_RAISE_HEIGHT`, beide 1.5 Sektoren). Der Heli überragt das Ziel dadurch immer, damit er nach unten zielen kann. WICHTIG: `FOLLOW` hatte ursprünglich KEINE Y-Verfolgung (nur `targetSpeed`) → Heli blieb auf alter Höhe, wenn Lara hinaufkletterte. Jetzt in allen drei Cases identisch.
- Logik-Prinzip Y-Verfolgung: `if (fabsf(posY - targetY) > minYDiff) ySpeed = (targetY > posY) ? FLY_DOWN_SPEED : -FLY_UP_SPEED;` (Y-down: größeres Y = tiefer; Ziel tiefer → runter, Ziel höher → rauf).
- `FixYPosition` clampt Boden-/Deckenabstand (Boden: `SECTOR_SIZE/4`, Decke: `SECTOR_SIZE/16`). Die Decke begrenzt den Aufstieg bei hohen Modellen – bei zu tiefer Mitte (unter dem Ziel) `minCeilingClearance` ggf. weiter senken.
- **Stabilität in Reichweite (wichtig):** `DetermineGunShipState` muss `horizontalDistance < maxShotsRange` → **immer** `IDLE` zurückgeben (unabhängig von `ItemFlags[7]`/`blockedEarly`). War IDLE an das Evade-Flag gekoppelt, lief der Heli in einem FOLLOW/EVADE-Loop an der `minDistance`-Grenze ("vorwärts ~1 s, Stopp, wieder vorwärts" — Rhythmik durch `inertiaTimer`-Reset + `currentSpeed = targetSpeed * pitchRatio`).
- **Evade-Flag (`ItemFlags[7]`):** nur Werte 0/1. `1` wird beim EVADE gesetzt und zurückgesetzt, sobald `dist > maxShotsRange && state != EVADE_NEAR`. Übergangsbedingungen an `yDiff` hängen lassen geht NICHT: die Y-Verfolgung hält den Heli auf `ZielY - 1.5 * SECTOR_SIZE`, daher sind kleine `|yDiff|`-Bedingungen nie erfüllbar (früherer Bug: `2 → 0` bei `|yDiff| < 0.5 * SECTOR_SIZE` → Flag lief nie zurück).
- Weltereinheiten: `BLOCK(x)=1024*x`, `CLICK(x)=256*x` (1 Block = 4 Klicks), `SECTOR_SIZE=1024` lokal definiert.

## Known Issues & TODOs
*(zu vervollständigen)*
