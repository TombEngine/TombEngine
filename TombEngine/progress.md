# Progress

## Current Task
Gunship (TR5): Feuersperre – Heli soll NUR schießen, wenn Lara in der Schusslinie ist und keine Wand dazwischen (keine Effekte mehr sonst).

## Completed Work
- **Analyse Pitch-Konvention:** Engine-Math = `DirectX::SimpleMath::Matrix::CreateFromYawPitchRoll` (via `using namespace DirectX::SimpleMath` in framework.h). Heli-Pose-Yaw = Ziel-Yaw + 180° (fliegt "hinter dem" Ziel her). Heckwaffe schießt entlang Modellachse `-Z` (+ fester 8° im Firing-Code) → **Waffen-Abguerung = Pose-Pitch + 8°**: positives Pitch = Nase oben = Waffe zeigt nach UNTEN; negatives Pitch = Waffe nach oben.
- **Root Cause verfehlter Schüsse:** Im IDLE überschrieb `UpdateIdleOrientation` (dotForward < 0 wegen Yaw+180) den Pitch konstant mit `-MAX_PITCH` (-20°) → Waffe 12° nach OBEN → alle Schüsse über Laras Kopf. Zusätzlich drückte die `*0.95`-Dämpfung (`!isMoving`) jeden bleibenden IDLE-Pitch gegen 0 (Gleichgewicht ≈ 37% des Ziels).
- **Fix 1:** Neuer Helper `CalculateIdlePitch`: `atan2(Unterschenkel-Höhe - HeliY, hDist) - 8°`, geclampt auf `[0, MAX_PITCH]`. Zielpunkt = 0.25 Sektor über Ziel-Pivot (`LOWER_LEG_OFFSET`).
- **Fix 2:** Beide Pitch-Switches in `ControlGunShip` setzen IDLE jetzt auf `CalculateIdlePitch` statt 0.
- **Fix 3:** `UpdateIdleOrientation` wird nur noch bei `currentState != IDLE` aufgerufen (überschreibt sonst Bewegungs-Pitch ±MAX_PITCH).
- **Fix 4:** Pitch/Bank-Rückstellung (`*0.95`) bei `!isMoving` wird bei IDLE übersprungen → IDLE hält den Zielpitch (Y-Speed-Dämpfung bleibt in allen States aktiv).
- **Root Cause Durchschuss durch Wände:** `ObjectOnLOS2` iteriert das GLOBALE `LosRoomNumbers`, das erst durch `LOS()` gefüllt wird (Prüft nur Items/Statics; Wände prüft nur `LOS()` → `GetRoomLosCollision`). Der Firing-Code rief `ObjectOnLOS2` VOR `LOS()` auf → stale Room-Liste → Lara hinter Wand wurde "gefunden" → Schaden.
- **LOS-Fix:** Erst `LOS()` (klammt an Wand + füllt `LosRoomNumbers`), dann `ObjectOnLOS2` mit geclamptem Target → Treffer nur im wandbegrenzten Segment.
- **Feuersperre (neuester Stand):** Firing-Block startet jetzt mit Gate: `gateMuzzle` (Joint 8) → `gateRot` (Pose-Pitch + 8°) → `gateAimed` (8 Sektor entlang -Z) → `LOS()` + `ObjectOnLOS2(..., ID_LARA, ...)` → `canFire = (gateResult != NO_LOS_ITEM)`. DARUNTER: (a) Sound + Flash-MeshBit, (b) Light + Hülse + Rauch stehen beide in `if (canFire)`; (c) Treffer-Logik (Static-Zerbrechen / Lara-Schaden / Funken) nur in `if (hasHit)`. Alter Wand-Decal-Zweig (`!hasHit && !result`) ist ENTFERN → Wand ohne Ziel = kompletter Stillstand (kein Sound, kein Decal). Nebeneffekt-fix: Flash-MeshBit wird nicht mehr gesetzt, wenn nicht geschossen wird.
- **Aufräumen:** Sonderzeichen-Kommentare bereinigt (U+2011, U+2019); `aimSpread`-Kommentar mit `≈`/U+202F + falscher "512 world units"-Anmerkung bleibt (Match-Probleme mit Editor-Tool, harmlos).

## Modified Files
- `Objects/TR5/Entity/tr5_gunship.cpp`

## Next Step
- In Szene testen: (a) Lara außerhalb der Schusslinie (seitlich/abgewandt) → KEIN Schuss (kein Sound/Blitz/Hülse); (b) Wand dazwischen → KEIN Schuss, kein Decal; (c) freier Blick → normales Feuer + Treffer an den Unterschenkeln; (d) Static in der Schusslinie → feuert, Static wird getroffen.

## Blockers
- Keine.
