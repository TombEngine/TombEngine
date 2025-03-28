#pragma once

struct ItemInfo;

namespace TEN::Entities::Generic
{
	enum class PushableBehaviorState
	{
		Idle,
		Move,
		EdgeSlip,
		Fall,
		Sink,
		Float,
		UnderwaterIdle,
		WaterSurfaceIdle,
		Slide,
		MoveStackHorizontal
	};

	void HandlePushableBehaviorState(ItemInfo& pushableItem);
};
