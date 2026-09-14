# Progress

## Current Task
Gunship (TR5): Heli soll im IDLE-State die Heckwaffe auf die Unterschenkel der (stationären) Spielfigur abgueren, damit die Schüsse treffen.

## Completed Work
- **Analyse Pitch-Konvention:** Engine-Math = `DirectX::SimpleMath::Matrix::CreateFromYawPitchRoll` (via `using namespace DirectX::SimpleMath` in framework.h). Heli-Pose-Yaw = Ziel-Yaw + 180° (fliegt "hinter dem" Ziel her). Heckwaffe schießt entlang Modellachse `-Z` (+ fester 8° im Firing-Code) → **Waffen-Abguerung = Pose-Pitch + 8°**: positives Pitch = Nase oben = Waffe zeigt nach UNTEN; negatives Pitch = Waffe nach oben.
- **Root Cause verfehlter Schüsse:** Im IDLE überschrieb `UpdateIdleOrientation` (dotForward < 0 wegen Yaw+180) den Pitch konstant mit `-MAX_PITCH` (-20°) → Waffe 12° nach OBEN → alle Schüsse über Laras Kopf. Zusätzlich drückte die `*0.95`-Dämpfung (`!isMoving`) jeden bleibenden IDLE-Pitch gegen 0 (Gleichgewicht ≈ 37% des Ziels).
- **Fix 1:** Neuer Helper `CalculateIdlePitch`: `atan2(Unterschenkel-Höhe - HeliY, hDist) - 8°`, geclampt auf `[0, MAX_PITCH]`. Zielpunkt = 0.25 Sektor über Ziel-Pivot (`LOWER_LEG_OFFSET`).
- **Fix 2:** Beide Pitch-Switches in `ControlGunShip` setzen IDLE jetzt auf `CalculateIdlePitch` statt 0.
- **Fix 3:** `UpdateIdleOrientation` wird nur noch bei `currentState != IDLE` aufgerufen (überschreibt sonst Bewegungs-Pitch ±MAX_PITCH).
- **Fix 4:** Pitch/Bank-Rückstellung (`*0.95`) bei `!isMoving` wird bei IDLE übersprungen → IDLE hält den Zielpitch (Y-Speed-Dämpfung bleibt in allen States aktiv).
- Erwartete Werte: bei 3–4 Sektoren Distanz + 1.5 Sektor Hover → Pitch ≈ +9°…+15° → Abguerung ≈ +17°…+23° → Treffer um die Unterschenkel.

## Modified Files
- `Objects/TR5/Entity/tr5_gunship.cpp`

## Next Step
- In Szene testen: Lara stehen lassen, Heli in Schussreichweite → Schüsse sollen um Laras Unterschenkel treffen (Heli nose-up geneigt).
- Falls zu hoch/tief: `LOWER_LEG_OFFSET` (0.25 Sektor) anpassen. Falls Richtung falsch (Waffe nach oben): Vorzeichen in `CalculateIdlePitch` umdrehen.

## Blockers
- Keine.
