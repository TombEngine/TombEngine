#pragma once
#include "Math/Math.h"
#include "Objects/Generic/Object/Pushable/PushableStates.h"

#include "Objects/Generic/Object/BridgeObject.h"

using namespace TEN::Math;

namespace TEN::Entities::Generic
{
	enum class PushableSoundState
	{
		None,
		Move,
		Stop,
		Fall,
		Wade
	};

	struct PushableEdgeAttribs
	{
		bool IsPullable	 = false;
		bool IsPushable	 = false;
		bool IsClimbable = false;

		PushableEdgeAttribs();
		PushableEdgeAttribs(bool isPullable, bool isPushable, bool isClimbable);
	};

	struct PushableStackData
	{
		int Limit			= 0; // Max number of pushables in stack that can be pushed.
		int ItemNumberAbove = 0;
		int ItemNumberBelow = 0;
	};

	struct PushableInfo
	{
		PushableBehaviorState BehaviorState = PushableBehaviorState::Idle;
		PushableSoundState	  SoundState	= PushableSoundState::None;
		int AnimSetID = 0;

		GameVector StartPos = GameVector::Zero;	// ?? XZ used by movement code. Y used to store water height level.
		PushableStackData Stack = {};

		int	  Height			 = 0;
		int	  WaterSurfaceHeight = 0;	 // Used for spawning effects.
		bool  IsOnEdge			 = false;
		float Gravity			 = 0.0f;
		float Oscillation		 = 0.0f; // Used when floating on water surface. Recomended range: (0.0f, 2.0f].

		bool CanFall			= false; // OCB 0.
		bool DoCenterAlign		= false; // OCB 1.
		bool IsBuoyant			= false; // OCB 2.
		bool UseRoomCollision	= false;
		bool UseBridgeCollision = false;
		std::map<int, PushableEdgeAttribs> EdgeAttribs = {};

		std::optional<BridgeObject> Bridge = std::nullopt;

		PushableInfo();
	};
}
