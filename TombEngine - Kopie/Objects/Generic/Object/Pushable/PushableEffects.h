#pragma once

struct ItemInfo;

namespace TEN::Entities::Generic
{
	void HandlePushableRippleEffect(ItemInfo& pushableItem);
	void SpawnPushableSplash(ItemInfo& pushableItem);
	void SpawnPushableBubbles(const ItemInfo& pushableItem);

	// TODO: Move. Not effects.
	void HandlePushableOscillation(ItemInfo& pushableItem);
	void HandlePushableBridgeOscillation(ItemInfo& pushableItem);
	void HandlePushableFallRotation(ItemInfo& pushableItem);
}
