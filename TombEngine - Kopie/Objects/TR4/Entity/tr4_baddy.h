#pragma once

class GameVector;
struct ItemInfo;

namespace TEN::Entities::TR4
{
	void InitializeBaddy(short itemNumber);
	void BaddyControl(short itemNumber);
	void Baddy2Hit(ItemInfo& target, ItemInfo& source, std::optional<GameVector> pos, int damage, bool isExplosive, int jointIndex);
}
