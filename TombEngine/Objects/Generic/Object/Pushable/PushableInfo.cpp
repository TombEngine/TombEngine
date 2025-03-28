#include "framework.h"
#include "Objects/Generic/Object/Pushable/PushableInfo.h"

#include "Game/control/control.h"
#include "Math/Math.h"
#include "Objects/Generic/Object/BridgeObject.h"
#include "Objects/Generic/Object/Pushable/PushableBridge.h"

using namespace TEN::Math;

namespace TEN::Entities::Generic
{
	const auto PUSHABLE_EDGE_ATTRIBS_DEFAULT = PushableEdgeAttribs(true, true, false);

	PushableEdgeAttribs::PushableEdgeAttribs()
	{
		*this = PUSHABLE_EDGE_ATTRIBS_DEFAULT;
	}

	PushableEdgeAttribs::PushableEdgeAttribs(bool isPullable, bool isPushable, bool isClimbable)
	{
		IsPullable = isPullable;
		IsPushable = isPushable;
		IsClimbable = isClimbable;
	}

	PushableInfo::PushableInfo()
	{
		constexpr auto DEFAULT_HEIGHT	   = BLOCK(1);
		constexpr auto DEFAULT_GRAVITY	   = 8.0f;
		constexpr auto DEFAULT_OSC		   = 0.75f;
		constexpr auto DEFAULT_STACK_LIMIT = 3;

		BehaviorState = PushableBehaviorState::Idle;
		SoundState = PushableSoundState::None;
		AnimSetID = 0;

		Height = DEFAULT_HEIGHT;
		WaterSurfaceHeight = NO_HEIGHT;
		IsOnEdge = false;
		Gravity = DEFAULT_GRAVITY;
		Oscillation = DEFAULT_OSC;

		Stack.Limit = DEFAULT_STACK_LIMIT;
		Stack.ItemNumberAbove = NO_VALUE;
		Stack.ItemNumberBelow = NO_VALUE;

		CanFall = false;
		DoCenterAlign = true;
		IsBuoyant = false;
		UseRoomCollision = false;
		UseBridgeCollision = false;

		EdgeAttribs =
		{
			{ CardinalDirection::NORTH, PushableEdgeAttribs() },
			{ CardinalDirection::EAST, PushableEdgeAttribs() },
			{ CardinalDirection::SOUTH, PushableEdgeAttribs() },
			{ CardinalDirection::WEST, PushableEdgeAttribs() }
		};
	}
}
