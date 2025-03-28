#include "framework.h"
#include "Objects/Generic/Object/Pushable/PushableStates.h"

#include "Game/animation.h"
#include "Game/control/flipeffect.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Objects/Generic/Object/Pushable/PushableBridge.h"
#include "Objects/Generic/Object/Pushable/PushableCollision.h"
#include "Objects/Generic/Object/Pushable/PushableEffects.h"
#include "Objects/Generic/Object/Pushable/PushableObject.h"
#include "Objects/Generic/Object/Pushable/PushableStack.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Input;

namespace TEN::Entities::Generic
{
	constexpr auto PUSHABLE_FALL_VELOCITY_MAX  = BLOCK(1 / 8.0f);
	constexpr auto PUSHABLE_WATER_VELOCITY_MAX = BLOCK(1 / 16.0f);

	constexpr auto PUSHABLE_RUMBLE_FALL_VELOCITY = 96.0f;
	constexpr auto PUSHABLE_EDGE_SLIP_VELOCITY	 = 0.8f;

	constexpr auto PUSHABLE_GRAVITY_AIR	  = 8.0f;
	constexpr auto PUSHABLE_GRAVITY_WATER = 4.0f;
	constexpr auto PUSHABLE_GRAVITY_ACCEL = 0.5f;

	constexpr auto PUSHABLE_WATER_SURFACE_DISTANCE = CLICK(0.5f);

	static void HandleIdleState(ItemInfo& pushableItem)
	{
		auto& pushable = GetPushableInfo(pushableItem);
		auto& playerItem = *LaraItem;
		auto& player = GetLaraInfo(playerItem);

		// 1) Check if player is interacting.
		if (player.Context.InteractedItem == pushableItem.Index)
		{
			if (PushableIdleConditions(pushableItem))
			{
				// Pushing.
				if (IsHeld(In::Forward))
				{
					int pushAnimNumber = (pushable.IsOnEdge) ? 
						PushableAnimSets[pushable.AnimSetID].EdgeAnimNumber :
						PushableAnimSets[pushable.AnimSetID].PushAnimNumber;
					SetAnimation(LaraItem, pushAnimNumber);
				}
				// Pulling.
				else if (IsHeld(In::Back))
				{
					int pullAnimNumber = PushableAnimSets[pushable.AnimSetID].PullAnimNumber;
					SetAnimation(LaraItem, pullAnimNumber);
				}

				pushable.StartPos = pushableItem.Pose.Position;
				pushable.StartPos.RoomNumber = pushableItem.RoomNumber;
				pushable.BehaviorState = PushableBehaviorState::Move;

				// Unstack lower pushables.
				UnstackPushable(pushableItem.Index);

				// Prepare upper pushables in stack for movement.
				StartMovePushableStack(pushableItem.Index);

				ResetPlayerFlex(LaraItem);

				DisablePushableBridge(pushableItem);
			}
			else if (playerItem.Animation.ActiveState != LS_PUSHABLE_GRAB &&
				playerItem.Animation.ActiveState != LS_PUSHABLE_PULL &&
				playerItem.Animation.ActiveState != LS_PUSHABLE_PUSH &&
				playerItem.Animation.ActiveState != LS_PUSHABLE_EDGE_SLIP)
			{
				player.Context.InteractedItem = NO_VALUE;
			}
		}

		// Get pushable collision.
		auto pushableColl = GetPushableCollision(pushableItem);

		switch (pushableColl.EnvType)
		{	
		case PushableEnvironmentType::FlatFloor:
			if (pushableColl.FloorHeight != pushableItem.Pose.Position.y)
			{
				pushableItem.Pose.Position.y = pushableColl.FloorHeight;

				int relHeight = pushableColl.FloorHeight - pushableItem.Pose.Position.y;
				SetPushableVerticalPos(pushableItem, relHeight);
			}

			break;

		case PushableEnvironmentType::WaterFloor:
			{
				if (pushableColl.FloorHeight != pushableItem.Pose.Position.y)
				{
					pushableItem.Pose.Position.y = pushableColl.FloorHeight;

					int relHeight = pushableColl.FloorHeight - pushableItem.Pose.Position.y;
					SetPushableVerticalPos(pushableItem, relHeight);
				}

				int waterheight = abs(pushableColl.FloorHeight - pushable.WaterSurfaceHeight);
				if (waterheight > GetPushableHeight(pushableItem))
				{
					if (pushable.IsBuoyant && pushable.Stack.ItemNumberAbove == NO_VALUE)
					{
						pushable.BehaviorState = PushableBehaviorState::Float;
						pushable.Gravity = 0.0f;
					}
					else
					{
						pushable.BehaviorState = PushableBehaviorState::UnderwaterIdle;
					}
				}
				else
				{
					HandlePushableRippleEffect(pushableItem);
				}
			}
			break;

		case PushableEnvironmentType::Air:
			// Pass to fall state if distance to floor is beyond threshold.
			if (abs(pushableColl.FloorHeight - pushableItem.Pose.Position.y) > CLICK(0.75f))
			{
				pushable.BehaviorState = PushableBehaviorState::Fall;
				SetPushableStopperFlag(false, pushableItem.Pose.Position, pushableItem.RoomNumber);
				DisablePushableBridge(pushableItem);
			}
			else
			{
				pushableItem.Pose.Position.y = pushableColl.FloorHeight;

				int relHeight = pushableColl.FloorHeight - pushableItem.Pose.Position.y;
				SetPushableVerticalPos(pushableItem, relHeight);
			}

			break;

		case PushableEnvironmentType::Water:
			DisablePushableBridge(pushableItem);
			SetPushableStopperFlag(false, pushableItem.Pose.Position, pushableItem.RoomNumber);

			if (pushable.IsBuoyant && pushable.Stack.ItemNumberAbove == NO_VALUE)
			{
				pushable.BehaviorState = PushableBehaviorState::Float;
				pushable.Gravity = 0.0f;
			}
			else
			{
				pushable.BehaviorState = PushableBehaviorState::Sink;
				pushable.Gravity = PUSHABLE_GRAVITY_WATER;
			}

			break;

		default:
			TENLog("Error handling pushable collision in idle state for pushable item " + std::to_string(pushableItem.Index), LogLevel::Warning, LogConfig::All);
			break;
		}
	}

	static void HandleMoveState(ItemInfo& pushableItem)
	{
		auto& pushable = GetPushableInfo(pushableItem);
		auto& playerItem = *LaraItem;
		auto& player = GetLaraInfo(playerItem);

		bool isPlayerPulling = playerItem.Animation.ActiveState == LS_PUSHABLE_PULL;

		int quadrant = GetQuadrant(playerItem.Pose.Orientation.y);
		int newPosX = pushable.StartPos.x;
		int newPosZ = pushable.StartPos.z;

		int displaceDepth = 0;
		int displaceBox = GameBoundingBox(LaraItem).Z2;

		if (pushable.SoundState == PushableSoundState::Move)
			pushable.SoundState = PushableSoundState::Stop;

		displaceDepth = GetLastFrame(GAME_OBJECT_ID::ID_LARA, playerItem.Animation.AnimNumber)->BoundingBox.Z2;
		
		if (isPlayerPulling)
		{
			displaceBox -= (displaceDepth + BLOCK(1));
		}
		else
		{
			displaceBox -= displaceDepth - (pushable.IsOnEdge ? BLOCK(0.5f) : BLOCK(1));
		}

		// Player is pushing or pulling.
		if (playerItem.Animation.FrameNumber != (g_Level.Anims[playerItem.Animation.AnimNumber].frameEnd - 1))
		{
			// 1) Determine displacement.
			switch (quadrant)
			{
				case NORTH:
					newPosZ += displaceBox;
					break;

				case EAST:
					newPosX += displaceBox;
					break;

				case SOUTH:
					newPosZ -= displaceBox;
					break;

				case WEST:
					newPosX -= displaceBox;
					break;

				default:
					break;
			}

			
			// Distance too far; return early. NOTE: May happen as animation bounds change.
			if (abs(pushableItem.Pose.Position.z - newPosZ) > BLOCK(0.75f))
				return;

			if (abs(pushableItem.Pose.Position.x - newPosX) > BLOCK(0.75f))
				return;

			int travelledDist = Vector3i::Distance(pushableItem.Pose.Position, pushable.StartPos.ToVector3i());
			if (pushable.IsOnEdge && travelledDist >= BLOCK(0.5f))
			{
				pushable.BehaviorState = PushableBehaviorState::EdgeSlip;
				return;
			}

			// 2) Move pushable.
			// Move only if the movement direction aligns with push/pull action.

			// Z axis.
			if (isPlayerPulling)
			{
				if ((quadrant == NORTH && pushableItem.Pose.Position.z > newPosZ) ||
					(quadrant == SOUTH && pushableItem.Pose.Position.z < newPosZ))
				{
					pushableItem.Pose.Position.z = newPosZ;
					pushable.SoundState = PushableSoundState::Move;
				}
			}
			else
			{
				if ((quadrant == NORTH && pushableItem.Pose.Position.z < newPosZ) ||
					(quadrant == SOUTH && pushableItem.Pose.Position.z > newPosZ))
				{
					pushableItem.Pose.Position.z = newPosZ;
					pushable.SoundState = PushableSoundState::Move;
				}
			}

			// X axis.
			if (isPlayerPulling)
			{
				if ((quadrant == EAST && pushableItem.Pose.Position.x > newPosX) ||
					(quadrant == WEST && pushableItem.Pose.Position.x < newPosX))
				{
					pushableItem.Pose.Position.x = newPosX;
					pushable.SoundState = PushableSoundState::Move;
				}
			}
			else
			{
				if ((quadrant == EAST && pushableItem.Pose.Position.x < newPosX) ||
					(quadrant == WEST && pushableItem.Pose.Position.x > newPosX))
				{
					pushableItem.Pose.Position.x = newPosX;
					pushable.SoundState = PushableSoundState::Move;
				}
			}

			if (pushable.WaterSurfaceHeight != NO_HEIGHT)
				HandlePushableRippleEffect(pushableItem);
		}
		// Push/pull animation finished.
		else
		{
			// 1) Realign with sector center.
			pushableItem.Pose.Position = GetNearestSectorCenter(pushableItem.Pose.Position);

			// 2) Connect to stack if applicable.
			int foundStack = SearchNearPushablesStack(pushableItem.Index);
			StackPushable(pushableItem.Index, foundStack);
			SetPushableStopperFlag(false, pushable.StartPos.ToVector3i(), pushable.StartPos.RoomNumber);

			pushable.StartPos = pushableItem.Pose.Position;
			pushable.StartPos.RoomNumber = pushableItem.RoomNumber;
			
			// Get pushable collision.
			auto pushableColl = GetPushableCollision(pushableItem);

			switch (pushableColl.EnvType)
			{
			case PushableEnvironmentType::FlatFloor:
			case PushableEnvironmentType::WaterFloor:
				// Activate trigger.
				TestTriggers(&pushableItem, true, pushableItem.Flags & IFLAG_ACTIVATION_MASK);

				// Check if push/pull must stop.
				if (!PushableAnimSets[pushable.AnimSetID].EnableAnimLoop ||
					!IsHeld(In::Action) ||
					!TestPushableMovementConditions(pushableItem, !isPlayerPulling, isPlayerPulling) ||
					!IsPushableValid(pushableItem))
				{
					playerItem.Animation.TargetState = LS_IDLE;
					pushable.BehaviorState = PushableBehaviorState::Idle;

					// Set upper pushables back to normal(?).
					StopMovePushableStack(pushableItem.Index);
					EnablePushableBridge(pushableItem);

					// Connect to another stack.
					int foundStack = SearchNearPushablesStack(pushableItem.Index);
					StackPushable(pushableItem.Index, foundStack);

					// TODO: Better solution that also works with pushable block anims.
					if (pushable.AnimSetID == 0)
						pushable.SoundState = PushableSoundState::Stop;
				}
				else if (playerItem.Animation.ActiveState == LS_PUSHABLE_PUSH && pushable.IsOnEdge)
				{
					playerItem.Animation.TargetState = LS_PUSHABLE_EDGE_SLIP;

					auto movementDir = pushableItem.Pose.Position.ToVector3() - playerItem.Pose.Position.ToVector3();
					movementDir.Normalize();
					playerItem.Pose.Position = playerItem.Pose.Position + movementDir * BLOCK(1);

					DisablePushableBridge(pushableItem);
				}
				else
				{
					DisablePushableBridge(pushableItem);
				}

				break;

			case PushableEnvironmentType::Air:
				pushable.BehaviorState = PushableBehaviorState::Fall;
				pushable.SoundState = PushableSoundState::None;
				playerItem.Animation.TargetState = LS_IDLE;
				player.Context.InteractedItem = NO_VALUE;
				return;

			case PushableEnvironmentType::SlopedFloor:
				// TODO: If slippery slope, link to slide state.
				break;

			case PushableEnvironmentType::Water:
				pushable.BehaviorState = PushableBehaviorState::Sink;
				pushable.SoundState = PushableSoundState::None;
				playerItem.Animation.TargetState = LS_IDLE;
				player.Context.InteractedItem = NO_VALUE;
				break;

			default:
				TENLog("Error handling pushable collision state in move state for pushable item number " + std::to_string(pushableItem.Index), LogLevel::Warning, LogConfig::All, false);
				break;
			}
		}
	}

	// TODO: Remove.
	static float InterpolateCubic(float value0, float value1, float value2, float value3, float alpha)
	{
		alpha = std::clamp(alpha, 0.0f, 1.0f);

		float p = (value3 - value2) - (value0 - value1);
		float q = (value0 - value1) - p;
		float r = value2 - value0;
		float s = value1;
		float x = alpha;
		float xSquared = SQUARE(x);
		return ((p * xSquared * x) + (q * xSquared) + (r * x) + s);
	}

	static void HandleEdgeSlipState(ItemInfo& pushableItem)
	{
		constexpr auto LEAN_ANGLE_MAX = ANGLE(40.0f);

		auto& pushable = GetPushableInfo(pushableItem);

		// Get pushable collision.
		auto pushableColl = GetPushableCollision(pushableItem);
		
		// Calculate movement direction.
		auto moveDir = (pushableItem.Pose.Position - pushable.StartPos.ToVector3i()).ToVector3();
		moveDir.y = 0.0f;
		moveDir.Normalize();

		// Define origin and target.
		auto origin = Geometry::TranslatePoint(pushable.StartPos.ToVector3(), moveDir, BLOCK(0.5f));
		auto target = Geometry::TranslatePoint(pushable.StartPos.ToVector3(), moveDir, BLOCK(1));
		target.y = pushable.StartPos.y + BLOCK(1);

		// Calculate current position based on interpolation.
		auto currentPos = pushableItem.Pose.Position.ToVector3();

		float& elapsedTime = pushableItem.Animation.Velocity.y;
		float alpha = std::clamp(elapsedTime / PUSHABLE_EDGE_SLIP_VELOCITY, 0.0f, 1.0f);

		// TODO: Try Lerp() instead.
		currentPos = Vector3(
			InterpolateCubic(origin.x, origin.x, target.x, target.x, alpha),
			InterpolateCubic(origin.y, origin.y, target.y - 700, target.y, alpha),
			InterpolateCubic(origin.z, origin.z, target.z, target.z, alpha));

		// Calculate lean angle based on movement direction.
		float leanAngle = LEAN_ANGLE_MAX * alpha;

		if (currentPos.y > pushableColl.FloorHeight)
		{
			currentPos.y = pushableColl.FloorHeight;
			pushableItem.Pose.Orientation = EulerAngles(0, pushableItem.Pose.Orientation.y, 0);
		}
		else
		{
			// Handle slip lean.
			int pushableQuadrant = GetQuadrant(pushableItem.Pose.Orientation.y);
			int movementQuadrant = GetQuadrant(FROM_RAD(atan2(moveDir.z, moveDir.x)));
			
			movementQuadrant = (movementQuadrant + pushableQuadrant) % 4;
			switch (movementQuadrant)
			{
				// TODO: Use CardinalDirection enum.
				case 0: //EAST
					pushableItem.Pose.Orientation = EulerAngles(0, pushableItem.Pose.Orientation.y, leanAngle);
					break;

				case 1: //NORTH
					pushableItem.Pose.Orientation = EulerAngles(-leanAngle, pushableItem.Pose.Orientation.y, 0);
					break;

				case 2: //WEST
					pushableItem.Pose.Orientation = EulerAngles(0, pushableItem.Pose.Orientation.y, -leanAngle);
					break;

				case 3: //SOUTH
					pushableItem.Pose.Orientation = EulerAngles(leanAngle, pushableItem.Pose.Orientation.y, 0);
					break;
			}
		}

		pushableItem.Pose.Position = currentPos;
		elapsedTime += DELTA_TIME;

		// Handle sounds.
		if (alpha <= 0.5f)
		{
			pushable.SoundState = PushableSoundState::Move;
		}
		else
		{
			if (pushable.SoundState == PushableSoundState::Move)
				pushable.SoundState = PushableSoundState::Stop;
		}

		// Check if movement is completed.
		if (alpha >= 1.0f)
		{
			currentPos = GetNearestSectorCenter(pushableItem.Pose.Position).ToVector3();

			switch (pushableColl.EnvType)
			{
			case PushableEnvironmentType::Air:
				pushable.BehaviorState = PushableBehaviorState::Fall;
				pushableItem.Animation.Velocity.y = PUSHABLE_FALL_VELOCITY_MAX / 2;
				break;

			case PushableEnvironmentType::FlatFloor:
			case PushableEnvironmentType::WaterFloor:
				pushableItem.Animation.Velocity.y = 0.0f;
				pushableItem.Pose.Position.y = pushableColl.FloorHeight;
				pushableItem.Pose.Orientation = EulerAngles(0, pushableItem.Pose.Orientation.y, 0);
				pushable.BehaviorState = PushableBehaviorState::Idle;

				EnablePushableBridge(pushableItem);
				break;

			case PushableEnvironmentType::Water:
				pushableItem.Animation.Velocity.y = PUSHABLE_WATER_VELOCITY_MAX / 2;
				pushable.BehaviorState = PushableBehaviorState::Sink;
					
				SpawnPushableSplash(pushableItem);
				break;

			case PushableEnvironmentType::SlopedFloor:
				pushableItem.Animation.Velocity.y = 0.0f;
				pushable.BehaviorState = PushableBehaviorState::Idle;
				break;

			default:
				TENLog("Error handling pushable collision in edge slip state for pushable item number " + std::to_string(pushableItem.Index), LogLevel::Warning, LogConfig::All);
				break;
			}			
		}
	}

	static void HandleFallState(ItemInfo& pushableItem)
	{
		auto& pushable = GetPushableInfo(pushableItem);

		// Get pushable collision.
		auto pushableColl = GetPushableCollision(pushableItem);

		int foundStack = NO_VALUE;

		switch (pushableColl.EnvType)
		{
		case PushableEnvironmentType::Air:
			pushableItem.Pose.Position.y += pushableItem.Animation.Velocity.y;
			pushableItem.Animation.Velocity.y = std::min(pushableItem.Animation.Velocity.y + pushable.Gravity, PUSHABLE_FALL_VELOCITY_MAX);
			HandlePushableFallRotation(pushableItem);
			break;

		case PushableEnvironmentType::FlatFloor:
		case PushableEnvironmentType::WaterFloor:
			// Connect to another stack.
			foundStack = SearchNearPushablesStack(pushableItem.Index);
			StackPushable(pushableItem.Index, foundStack);

			pushableItem.Pose.Orientation = EulerAngles(0, pushableItem.Pose.Orientation.y, 0);

			// Set stopper flag.
			if (pushable.Stack.ItemNumberBelow == NO_VALUE)
				SetPushableStopperFlag(true, pushableItem.Pose.Position, pushableItem.RoomNumber);

			// Activate trigger.
			TestTriggers(&pushableItem, true, pushableItem.Flags & IFLAG_ACTIVATION_MASK);

			// Shake floor if pushable landed at high enough velocity.
			if (pushableItem.Animation.Velocity.y >= PUSHABLE_RUMBLE_FALL_VELOCITY)
			{
				FloorShake(&pushableItem);
				pushable.SoundState = PushableSoundState::Fall;
			}

			// Place on floor.
			pushableItem.Animation.Velocity.y = 0.0f;
			pushableItem.Pose.Position.y = pushableColl.FloorHeight;
			pushable.BehaviorState = PushableBehaviorState::Idle;

			EnablePushableBridge(pushableItem);
			break;

		case PushableEnvironmentType::SlopedFloor:
			pushableItem.Animation.Velocity.y = 0.0f;
			pushableItem.Pose.Position.y = pushableColl.FloorHeight;
			pushable.BehaviorState = PushableBehaviorState::Slide;
			break;

		case PushableEnvironmentType::Water:
			pushable.BehaviorState = PushableBehaviorState::Sink;
			SpawnPushableSplash(pushableItem);
			break;

		default:
			TENLog("Error handling pushable collision in fall state for pushable item " + std::to_string(pushableItem.Index), LogLevel::Warning, LogConfig::All, false);
			break;
		}
	}

	static void HandleSinkState(ItemInfo& pushableItem)
	{
		auto& pushable = GetPushableInfo(pushableItem);

		// Get pushable collision.
		auto pushableColl = GetPushableCollision(pushableItem);

		int foundStack = NO_VALUE;

		switch (pushableColl.EnvType)
		{
		case PushableEnvironmentType::Air:
		case PushableEnvironmentType::FlatFloor:
		case PushableEnvironmentType::SlopedFloor:
			// Set stopper flag.
			if (pushable.Stack.ItemNumberBelow == NO_VALUE)
				SetPushableStopperFlag(true, pushableItem.Pose.Position, pushableItem.RoomNumber);

			pushable.BehaviorState = PushableBehaviorState::Fall;
			pushable.Gravity = PUSHABLE_GRAVITY_AIR;
			break;

		case PushableEnvironmentType::Water:
		{
			// Manage influence of gravity.
			if (pushable.IsBuoyant && pushable.Stack.ItemNumberAbove == NO_VALUE)
			{
				pushable.Gravity = pushable.Gravity - PUSHABLE_GRAVITY_ACCEL;
				if (pushable.Gravity <= 0.0f)
				{
					pushable.BehaviorState = PushableBehaviorState::Float;
					return;
				}
			}
			else
			{
				pushable.Gravity = std::max(pushable.Gravity - PUSHABLE_GRAVITY_ACCEL, PUSHABLE_GRAVITY_WATER);
			}

			// Move object.
			pushableItem.Pose.Position.y += pushableItem.Animation.Velocity.y;
			pushableItem.Animation.Velocity.y = std::min(pushableItem.Animation.Velocity.y + pushable.Gravity, PUSHABLE_WATER_VELOCITY_MAX);
			HandlePushableFallRotation(pushableItem);

			// Shallow water.
			int waterheight = abs(pushableColl.FloorHeight - pushable.WaterSurfaceHeight);
			if (waterheight > GetPushableHeight(pushableItem))
			{
				HandlePushableRippleEffect(pushableItem);
			}
			// Deep water.
			else
			{
				SpawnPushableBubbles(pushableItem);
			}
		}
			break;

		case PushableEnvironmentType::WaterFloor:
			if (pushable.IsBuoyant && pushable.Stack.ItemNumberAbove == NO_VALUE)
			{
				pushableItem.Pose.Position.y = pushableColl.FloorHeight;
				pushable.BehaviorState = PushableBehaviorState::Float;
				pushable.Gravity = 0.0f;
			}
			else
			{
				pushableItem.Pose.Position.y = pushableColl.FloorHeight;
				pushableItem.Animation.Velocity.y = 0.0f;
				pushable.BehaviorState = PushableBehaviorState::UnderwaterIdle;
				pushable.Gravity = PUSHABLE_GRAVITY_WATER;

				EnablePushableBridge(pushableItem);

				pushableItem.Pose.Orientation = EulerAngles(0, pushableItem.Pose.Orientation.y, 0);

				// Activate trigger.
				TestTriggers(&pushableItem, true, pushableItem.Flags & IFLAG_ACTIVATION_MASK);
			}
			break;

		default:
			TENLog("Error handling pushable collision in sink state for pushable item " + std::to_string(pushableItem.Index), LogLevel::Warning, LogConfig::All, false);
			break;
		}
	}

	static void HandleFloatState(ItemInfo& pushableItem)
	{
		auto& pushable = GetPushableInfo(pushableItem);

		// Get pushable collision.
		auto pushableColl = GetPushableCollision(pushableItem);

		int targetHeight = 0;

		switch (pushableColl.EnvType)
		{
		case PushableEnvironmentType::Air:
		case PushableEnvironmentType::FlatFloor:
		case PushableEnvironmentType::SlopedFloor:	

			// Set stopper flag.
			if (pushable.Stack.ItemNumberBelow == NO_VALUE)
				SetPushableStopperFlag(true, pushableItem.Pose.Position, pushableItem.RoomNumber);

			pushable.BehaviorState = PushableBehaviorState::Fall;
			pushable.Gravity = PUSHABLE_GRAVITY_AIR;
			break;

		case PushableEnvironmentType::Water:
		case PushableEnvironmentType::WaterFloor:
			// Determine target height.
			if (pushable.WaterSurfaceHeight == NO_HEIGHT)
			{
				if (abs(pushableColl.CeilingHeight - pushableColl.FloorHeight) >= (GetPushableHeight(pushableItem) + PUSHABLE_WATER_SURFACE_DISTANCE))
				{
					targetHeight = (pushableColl.CeilingHeight + pushable.Height) + PUSHABLE_WATER_SURFACE_DISTANCE;
				}
				else
				{
					targetHeight = pushableItem.Pose.Position.y;
				}
			}
			// No ceiling; rise above water surface.
			else
			{
				targetHeight = pushable.WaterSurfaceHeight - PUSHABLE_WATER_SURFACE_DISTANCE + pushable.Height;
			}

			// Manage influence of gravity.
			pushable.Gravity = std::max(pushable.Gravity - PUSHABLE_GRAVITY_ACCEL, -PUSHABLE_GRAVITY_WATER);

			// Move up.
			if (targetHeight < pushableItem.Pose.Position.y)
			{
				pushableItem.Pose.Position.y += pushableItem.Animation.Velocity.y;
				pushableItem.Animation.Velocity.y = std::min(pushableItem.Animation.Velocity.y + pushable.Gravity, PUSHABLE_WATER_VELOCITY_MAX);
				HandlePushableFallRotation(pushableItem);
			}
			// Reached target height.
			else
			{
				pushableItem.Animation.Velocity.y = 0.0f;
				pushableItem.Pose.Position.y = targetHeight;
				pushableItem.Pose.Orientation = EulerAngles(0, pushableItem.Pose.Orientation.y, 0);
				pushable.BehaviorState = PushableBehaviorState::WaterSurfaceIdle;
				pushable.Gravity = PUSHABLE_GRAVITY_WATER;

				EnablePushableBridge(pushableItem);
			}

			break;

		default:
			TENLog("Error handling pushable collision in float state for pushable item " + std::to_string(pushableItem.Index), LogLevel::Warning, LogConfig::All, false);
			break;
		}
	}

	static void HandleUnderwaterState(ItemInfo& pushableItem)
	{
		auto& pushable = GetPushableInfo(pushableItem);

		// Get pushable collision.
		auto pushableColl = GetPushableCollision(pushableItem);

		switch (pushableColl.EnvType)
		{
		case PushableEnvironmentType::FlatFloor:
		case PushableEnvironmentType::SlopedFloor:
		case PushableEnvironmentType::Air:
			// Set stopper flag.
			if (pushable.Stack.ItemNumberBelow == NO_VALUE)
				SetPushableStopperFlag(true, pushableItem.Pose.Position, pushableItem.RoomNumber);

			pushableItem.Animation.Velocity.y = 0.0f;
			pushableItem.Pose.Position.y = pushableColl.FloorHeight;
			pushable.BehaviorState = PushableBehaviorState::Idle;
			pushable.Gravity = PUSHABLE_GRAVITY_AIR;	
			break;

		case PushableEnvironmentType::WaterFloor:
		{
			// Reached water floor; change to idle.
			if (pushable.WaterSurfaceHeight != NO_HEIGHT)
			{
				int waterheight = abs(pushableColl.FloorHeight - pushable.WaterSurfaceHeight);
				if (waterheight < GetPushableHeight(pushableItem))
				{
					pushableItem.Animation.Velocity.y = 0.0f;
					pushableItem.Pose.Position.y = pushableColl.FloorHeight;
					pushable.BehaviorState = PushableBehaviorState::Idle;
					pushable.Gravity = PUSHABLE_GRAVITY_AIR;
				}
			}
			else if (pushable.IsBuoyant && pushable.Stack.ItemNumberAbove == NO_VALUE)
			{
				pushableItem.Animation.Velocity.y = 0.0f;
				pushable.BehaviorState = PushableBehaviorState::Float;
				pushable.Gravity = 0.0f;
			}

			// Remain on floor.
			pushableItem.Pose.Position.y = pushableColl.FloorHeight;
			int relHeight = pushableColl.FloorHeight - pushableItem.Pose.Position.y;
			SetPushableVerticalPos(pushableItem, relHeight);
		}
		break;

		case PushableEnvironmentType::Water:
			if (pushable.IsBuoyant && pushable.Stack.ItemNumberAbove == NO_VALUE)
			{
				pushableItem.Animation.Velocity.y = 0.0f;
				pushable.BehaviorState = PushableBehaviorState::Float;
				pushable.Gravity = 0.0f;
				return;
			}

			// Only pass to sinking if distance is noticeable. If small, stick to floor.
			if (abs(pushableColl.FloorHeight - pushableItem.Pose.Position.y) > CLICK(0.75f))
			{
				// Reset Stopper flag.
				if (pushable.Stack.ItemNumberBelow == NO_VALUE)
					SetPushableStopperFlag(false, pushableItem.Pose.Position, pushableItem.RoomNumber);
					
				pushableItem.Animation.Velocity.y = 0.0f;
				pushable.BehaviorState = PushableBehaviorState::Sink;
				pushable.Gravity = 0.0f;
			}
			else
			{
				pushableItem.Pose.Position.y = pushableColl.FloorHeight;
			}

			break;
		
		default:
			TENLog("Error handling pushable collision in underwater state for pushable item " + std::to_string(pushableItem.Index), LogLevel::Warning, LogConfig::All, false);
			break;
		}
	}

	static void HandleWaterSurfaceState(ItemInfo& pushableItem)
	{
		auto& pushable = GetPushableInfo(pushableItem);

		// Get pushable collision.
		auto pushableColl = GetPushableCollision(pushableItem);

		switch (pushableColl.EnvType)
		{
		case PushableEnvironmentType::FlatFloor:
		case PushableEnvironmentType::SlopedFloor:
		case PushableEnvironmentType::Air:
			pushableItem.Animation.Velocity.y = 0.0f;
			pushableItem.Pose.Orientation = EulerAngles(0, pushableItem.Pose.Orientation.y, 0);
			pushable.BehaviorState = PushableBehaviorState::Fall;
			pushable.Gravity = PUSHABLE_GRAVITY_AIR;

			DisablePushableBridge(pushableItem);
			break;

		case PushableEnvironmentType::Water:
			pushable.UseRoomCollision ? HandlePushableBridgeOscillation(pushableItem) : HandlePushableOscillation(pushableItem);
			HandlePushableRippleEffect(pushableItem);
			break;

		case PushableEnvironmentType::WaterFloor:
		{
			// If shallow water, change to idle state.
			int waterheight = abs(pushableColl.FloorHeight - pushable.WaterSurfaceHeight);
			if (waterheight < GetPushableHeight(pushableItem))
			{
				pushable.BehaviorState = PushableBehaviorState::Idle;
				pushable.Gravity = PUSHABLE_GRAVITY_AIR;
			}
			else
			{
				pushable.BehaviorState = PushableBehaviorState::UnderwaterIdle;
				pushable.Gravity = PUSHABLE_GRAVITY_WATER;
			}

			pushableItem.Animation.Velocity.y = 0.0f;
			pushableItem.Pose.Position.y = pushableColl.FloorHeight;
			pushableItem.Pose.Orientation = EulerAngles(0, pushableItem.Pose.Orientation.y, 0);
		}
			break;

		default:
			TENLog("Error handling pushable collision in water surface state for pushable item " + std::to_string(pushableItem.Index), LogLevel::Warning, LogConfig::All, false);
			break;
		}
	}

	static void HandleSlideState(ItemInfo& pushableItem)
	{
		auto& pushable = GetPushableInfo(pushableItem);

		// TODO:
		// 1. Get floor surface's slope and aspect angles.
		// 2. Calculate speed.
		// 3. Move object.
		// 4. Assess oncoming sector.
			// Is slope -> keep sliding.
			// Is flat floor -> stop sliding.
			// Is ledge -> fall.
			// Is forbidden sector -> freeze.
		// 5. Incorporate effects.
	}

	static void HandleMoveStackHorizontalState(ItemInfo& pushableItem)
	{
		auto& pushable = GetPushableInfo(pushableItem);
		const auto& playerItem = *LaraItem;
		const auto& player = GetLaraInfo(playerItem);

		auto& movingPushableItem = g_Level.Items[player.Context.InteractedItem];
		pushableItem.Pose.Position.x = movingPushableItem.Pose.Position.x;
		pushableItem.Pose.Position.z = movingPushableItem.Pose.Position.z;
	}

	// TODO 
	/*void HandleStackFallState(ItemInfo& pushableItem)
	{
		auto& pushableItem = g_Level.Items[pushableItem.Index];
		auto& pushable = GetPushableInfo(pushableItem);
		const auto& playerItem = *LaraItem;
		const auto& player = GetLaraInfo(playerItem);

		auto& movingPushableItem = g_Level.Items[player.Context.InteractedItem];
		pushableItem.Pose.Position.y = movingPushableItem.Pose.Position.y;
	}*/

	void HandlePushableBehaviorState(ItemInfo& pushableItem)
	{
		// Key = behavior state, key = state function.
		static const auto BEHAVIOR_STATE_MAP = std::unordered_map<PushableBehaviorState, std::function<void(ItemInfo& pushableItem)>>
		{
			{ PushableBehaviorState::Idle, &HandleIdleState },
			{ PushableBehaviorState::Move, &HandleMoveState },
			{ PushableBehaviorState::EdgeSlip, &HandleEdgeSlipState },
			{ PushableBehaviorState::Fall, &HandleFallState },
			{ PushableBehaviorState::Sink, &HandleSinkState },
			{ PushableBehaviorState::Float, &HandleFloatState },
			{ PushableBehaviorState::UnderwaterIdle, &HandleUnderwaterState },
			{ PushableBehaviorState::WaterSurfaceIdle, &HandleWaterSurfaceState },
			{ PushableBehaviorState::Slide, &HandleSlideState },
			{ PushableBehaviorState::MoveStackHorizontal, &HandleMoveStackHorizontalState }
		};

		auto& pushable = GetPushableInfo(pushableItem);

		auto it = BEHAVIOR_STATE_MAP.find(pushable.BehaviorState);
		if (it == BEHAVIOR_STATE_MAP.end())
		{
			TENLog("Attempted to handle missing pushable state " + std::to_string((int)pushable.BehaviorState), LogLevel::Error, LogConfig::All);
			return;
		}

		const auto& [stateKey, stateRoutine] = *it;
		stateRoutine(pushableItem);
	}
}
