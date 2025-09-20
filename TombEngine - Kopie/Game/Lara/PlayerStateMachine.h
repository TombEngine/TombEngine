#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Player
{
	enum class PlayerBehaviorStateRoutineType
	{
		Control,
		Collision
	};

	void InitializePlayerStateMachine();
	void HandlePlayerBehaviorState(ItemInfo& item, CollisionInfo& coll, PlayerBehaviorStateRoutineType routineType);
}
