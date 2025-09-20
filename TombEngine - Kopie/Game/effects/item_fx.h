#pragma once

#include "Scripting/Internal/TEN/Types/Color/Color.h"

struct ItemInfo;

namespace TEN::Effects::Items
{
	constexpr int DEFAULT_NONLETHAL_EFFECT_TIMEOUT = 48;

	void ItemBurn(ItemInfo* item, int timeout = -1);
	void ItemCustomBurn(ItemInfo* item, const Vector3& color1 = Vector3::Zero, const Vector3& color2 = Vector3::Zero, int timeout = -1);
	void ItemRedLaserBurn(ItemInfo* item, int timeout);
	void ItemBlueElectricBurn(ItemInfo* item, int timeout = DEFAULT_NONLETHAL_EFFECT_TIMEOUT);
	void ItemElectricBurn(ItemInfo* item, int timeout = DEFAULT_NONLETHAL_EFFECT_TIMEOUT);
	void ItemSmoke(ItemInfo* item, int timeout = DEFAULT_NONLETHAL_EFFECT_TIMEOUT);
	void ItemSetAsCorpse(ItemInfo* item, int timeout = -1);
	void LavaBurn(ItemInfo* item);
	void LaraBreath(ItemInfo* item);
}
