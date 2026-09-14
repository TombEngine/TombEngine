# Progress

## Current Task
Gunship (TR5): (1) Heli steigt beim Evaden nicht auf – Root Cause gefunden & behoben. (2) Heli soll immer etwas über dem Ziel fliegen, damit er beim Schießen nach unten zielen kann.

## Completed Work
- **Root Cause EVADE-Rise:** `ItemFlags[6]` (persistente Y-Geschwindigkeit) wurde im EVADE-Zweig (`ItemFlags[7]==1`) **ohne** `* FLOATING_POINT_SCALE` geschrieben, aber in Zeile ~391 mit `/ FLOATING_POINT_SCALE` gelesen → Lerp startete jeden Frame fast bei 0 → Aufstieg ~2.5 Einheiten/Frame (unsichtbar). Behoben: `ItemFlags[6]` wird nun immer mit `* FLOATING_POINT_SCALE` geschrieben (Skalen-Bug entfernt).
- **Hover über Ziel:** Neue Konstante `HOVER_HEIGHT_OFFSET` (1.5 Sektoren). `IDLE`-Zweig peilt jetzt `targetPosIdle.y - HOVER_HEIGHT_OFFSET` an → Heli schwebt immer ~1.5 Sektoren über dem Ziel (mindestens ~0.5 Sektor, wegen `minYDiff`-Deadband).
- **C2360-Fix:** `idleTargetY` vor dem `switch` deklariert (Initialisierung in `case` würde an der folgenden `case`-Bezeichnung vorbeigehen); im IDLE-Fall nur noch zugewiesen.

## Modified Files
- `Objects/TR5/Entity/tr5_gunship.cpp`

## Next Step
- In Szene testen: (a) Heli steigt beim Evaden jetzt sichtbar/fließend auf, (b) Heli schwebt über Lara auch wenn sie hinaufklettert und trifft beim Schießen.
- Ggf. `HOVER_HEIGHT_OFFSET` (1.5 Sektoren) bei Bedarf tunen.

## Blockers
- Keine.
