# Progress

## Current Task
Gunship (TR5): Heli steigt nicht über die Spielfigur, wenn sie auf eine höhere Plattform klettert (Follow & Idle). Root Cause: FOLLOW-State verfehlte Y-Verfolgung + Decken-Clamp zu restriktiv.

## Completed Work
- **Root Cause (neues Problem):** Im `FOLLOW`-State wurde `currentYSpeed` nicht aktualisiert (nur `targetSpeed`). Der Heli behielt seine alte Y-Geschwindigkeit und stieg nicht nach, wenn Lara hinaufkletterte → blieb tief (unterhalb der Spielfigur). `IDLE`/`EVADE` hatten die Y-Verfolgung bereits korrekt.
- **Fix FOLLOW-Y:** `FOLLOW`-Zweig aktualisiert jetzt `currentYSpeed` identisch wie `IDLE`: konvergiert auf `targetPosIdle.y - HOVER_HEIGHT_OFFSET` (1.5 Sektoren über dem Ziel). Damit folgt der Heli in **allen** drei States der Höhe des Ziels und überragt es.
- **Decken-Clamp (FixYPosition):** `minCeilingClearance` von `SECTOR_SIZE/8` (128) auf `SECTOR_SIZE/16` (64) reduziert → die Oberkante des Helis darf näher an die Decke, die Mitte sinkt dadurch nicht unter das Ziel. (Decken-Check selbst war logisch korrekt, aber bei hohen Modellen zu konservativ.)
- **Erledigt (vorher):** EVADE-Rise Skalen-Bug, `HOVER_HEIGHT_OFFSET` (IDLE), C2360-Fix.

## Modified Files
- `Objects/TR5/Entity/tr5_gunship.cpp`

## Next Step
- In Szene testen: Lara klettert auf 12-Klick-Plattform (Decke 39 Klicks) → Heli sollte in Follow UND Idle flüssig aufsteigen und Lara deutlich überragen.
- Ggf. `HOVER_HEIGHT_OFFSET` (1.5 Sektoren) tunen, falls die Höhe in bestimmter Geometrie unpassend ist.

## Blockers
- Keine.
