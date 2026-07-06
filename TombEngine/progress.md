# Progress

## Current Task
Heli-Kollisionsprüfung: Heli stoppt wenn halbes Square vor Pivot + 3 Squares links/rechts Wand/solid geometry ist oder Deckenhöhe zu niedrig ist.

## Completed Work
- `CheckPointCollision` Funktion implementiert mit:
  - Wandprüfung mit `pointColl.GetSector().IsWall(x, z)`
  - Deckenhöhenprüfung mit `relCeilHeight <= boxHeight`
  - Keine Slopes/Stufen Prüfung
- Checkpoints berechnen sich aus Heli-Orientierung

## Next Step
Testen ob der Code korrekt funktioniert.
