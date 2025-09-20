#pragma once

void PulseLightControl(short itemNumber);
void TriggerAlertLight(int x, int y, int z, int r, int g, int b, short angle, short roomNumber, int falloff);
void StrobeLightControl(short itemNumber);
void ColorLightControl(short itemNumber);
void ElectricalLightControl(short itemNumber);
void InitializeElectricalLight(short itemNumber);
void BlinkingLightControl(short itemNumber);
