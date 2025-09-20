#pragma once

class Vector3i;
struct ItemInfo;

void TriggerChaffEffects(int flareLife);
void TriggerChaffEffects(ItemInfo& item, int age);
void TriggerChaffEffects(ItemInfo& item, const Vector3i& pos, const Vector3i& vel, int speed, bool isUnderwater, int age);
void TriggerChaffSparkles(const Vector3i& pos, const Vector3i& vel, const Color& color, int age, const ItemInfo& item);
void TriggerChaffSmoke(const Vector3i& pos, const Vector3i& vel, int speed, bool isMoving, bool wind);
