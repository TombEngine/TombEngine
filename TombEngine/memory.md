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
- `IDLE`: Heli schwebt ~`HOVER_HEIGHT_OFFSET` (1.5 Sektoren) über dem Ziel, damit er beim Schießen nach unten zielen kann (statt auf Zielhöhe).
- `EVADE_NEAR`: Heli pitcht nose-up und fliegt vom Ziel weg; Zielhöhe `EVADE_RAISE_HEIGHT` (1.5 Sektoren) über Ziel. Trigger: `horizontalDist < minDistance` UND `yDiff >= -6 Sektoren` (Ziel nicht >6 Sektoren über Heli).
- `FixYPosition` clampt Boden-/Deckenabstand; Decke begrenzt den Aufstieg.

## Known Issues & TODOs
*(zu vervollständigen)*
