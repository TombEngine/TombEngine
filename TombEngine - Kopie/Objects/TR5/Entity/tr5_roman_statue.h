#pragma once

class GameVector;
class Vector3i;
struct ItemInfo;

namespace TEN::Entities::Creatures::TR5
{
	void InitializeRomanStatue(short itemNumber);
	void RomanStatueControl(short itemNumber);
	void RomanStatueHit(ItemInfo& target, ItemInfo& source, std::optional<GameVector> pos, int damage, bool isExplosive, int jointIndex);
	void TriggerRomanStatueMissileSparks(Vector3i* pos, char fxObject);;
}
