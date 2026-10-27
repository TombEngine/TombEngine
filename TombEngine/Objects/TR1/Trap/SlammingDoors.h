#pragma once

struct BiteInfo;
struct ItemInfo;

namespace TEN::Entities::Traps
{
	void InitializeSlammingDoors(short itemNumber);
	void ControlSlammingDoors(short itemNumber);
	void TriggerSlammingDoorSpark(const GameVector& pos, short angle, int count, const Vector4& colorStart);
	void SpawnSlammingDoorSparks(const Vector3i& localOffset, const ItemInfo& item);
}
