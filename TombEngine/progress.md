# Progress

## Current Task
Gunship (TR5): Heli wechselt in Schussreichweite bei stationärer Spielfigur zwischen FOLLOW und IDLE (vorwärts fliegen ~1 s, Stopp, wieder vorwärts) statt stabil im IDLE zu bleiben.

## Completed Work
- **Root Cause 1:** `DetermineGunShipState` erlaubte IDLE in Reichweite nur bei `ItemFlags[7] == 0`. War das Evade-Flag einmal gesetzt, wurde der Heli in Reichweite zu FOLLOW → EVADE an der minDistance-Grenze → FOLLOW → … (Endlos-Loop).
- **Root Cause 2:** Evade-Flag-Maschine war defekt: Übergang `2 → 0` verlangte `|yDiff| < 0.5 * SECTOR_SIZE`, aber die Y-Verfolgung hält den Heli auf `ZielY - 1.5 * SECTOR_SIZE` (Hover-Offset) → Bedingung nie erfüllbar → Flag lief nie zurück.
- **Rhythmik-Ursache:** Jeder State-Wechsel resetet `inertiaTimer` (25 Frames ≈ 0.4–1 s) + `currentSpeed = targetSpeed * pitchRatio` (Pitch lerpt langsam) → sichtbare ~1-s-Vorwärtsimpulse.
- **Fix 1:** `DetermineGunShipState`: `horizontalDistance < maxShotsRange` → immer `IDLE` (unabhängig von Evade-Flag/Blockade) → stabiles Schweben in Reichweite.
- **Fix 2:** Evade-Flag wird direkt auf 0 zurückgesetzt, sobald `horizontalDist > maxShotsRange && state != EVADE_NEAR` (Wert `2` wird nicht mehr verwendet) → sauberes Re-Engagement nach Evade.

## Modified Files
- `Objects/TR5/Entity/tr5_gunship.cpp`

## Next Step
- In Szene testen: Heli auf Schussreichweite anfliegen, Lara stehen lassen → Heli soll sauber in IDLE schweben (kein Vorwärts/Stopp-Puls).
- Evade testen: Lara nähert sich unter minDistance → eine Evade, danach Heli stabil in Reichweite.

## Blockers
- Keine.
