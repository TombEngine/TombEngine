#pragma once

class GameVector;
struct ItemInfo;

namespace TEN::Entities::TR3
{
	enum class CorpseFlag
	{
		None = 0,
		Grounded = 1,
		Hang = 2,
		Fall = 3
	};

	void InitializeCorpse(short itemNumber);
	void ControlCorpse(short itemNumber);
	void HitCorpse(ItemInfo& target, ItemInfo& source, std::optional<GameVector> pos, int damage, bool isExplosive, int jointIndex);
}
