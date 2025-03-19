#include "framework.h"
#include "Game/Lara/lara.h"

#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/floordata.h"
#include "Game/collision/Point.h"
#include "Game/control/flipeffect.h"
#include "Game/control/volume.h"
#include "Game/effects/Hair.h"
#include "Game/effects/item_fx.h"
#include "Game/effects/tomb4fx.h"
#include "Game/Gui.h"
#include "Game/items.h"
#include "Game/Lara/lara_basic.h"
#include "Game/Lara/lara_cheat.h"
#include "Game/Lara/lara_climb.h"
#include "Game/Lara/lara_collide.h"
#include "Game/Lara/lara_crawl.h"
#include "Game/Lara/lara_fire.h"
#include "Game/Lara/lara_hang.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_initialise.h"
#include "Game/Lara/lara_jump.h"
#include "Game/Lara/lara_monkey.h"
#include "Game/Lara/lara_objects.h"
#include "Game/Lara/lara_one_gun.h"
#include "Game/Lara/lara_overhang.h"
#include "Game/Lara/lara_slide.h"
#include "Game/Lara/lara_surface.h"
#include "Game/Lara/lara_swim.h"
#include "Game/Lara/lara_tests.h"
#include "Game/Lara/PlayerStateMachine.h"
#include "Game/misc.h"
#include "Game/savegame.h"
#include "Renderer/Renderer.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Include/ScriptInterfaceLevel.h"
#include "Sound/sound.h"
#include "Specific/Input/Input.h"
#include "Specific/winmain.h"

using namespace TEN::Collision::Floordata;
using namespace TEN::Collision::Point;
using namespace TEN::Control::Volumes;
using namespace TEN::Effects::Hair;
using namespace TEN::Effects::Items;
using namespace TEN::Entities::Player;
using namespace TEN::Input;
using namespace TEN::Math;
using namespace TEN::Gui;

using namespace TEN::Animation;
using TEN::Renderer::g_Renderer;

LaraInfo	  Lara			= {};
ItemInfo*	  LaraItem		= nullptr;
CollisionInfo LaraCollision = {};

void LaraControl(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	// Alert nearby creatures.
	if (player.Control.Weapon.HasFired)
	{
		AlertNearbyGuards(item);
		player.Control.Weapon.HasFired = false;
	}

	// Handle object interation adjustment parameters.
	if (player.Control.IsMoving)
	{
		if (player.Control.Count.PositionAdjust > PLAYER_POSITION_ADJUST_MAX_TIME)
		{
			player.Control.IsMoving = false;
			player.Control.HandStatus = HandStatus::Free;
		}

		++player.Control.Count.PositionAdjust;
	}
	else
	{
		player.Control.Count.PositionAdjust = 0;
	}

	if (!player.Control.IsLocked)
		player.LocationPad = NO_VALUE;

	// FAILSAFE: Force hand status reset.
	if (item->Animation.AnimNumber == LA_STAND_IDLE &&
		item->Animation.ActiveState == LS_IDLE &&
		item->Animation.TargetState == LS_IDLE &&
		!item->Animation.IsAirborne &&
		player.Control.HandStatus == HandStatus::Busy)
	{
		player.Control.HandStatus = HandStatus::Free;
	}

	HandlePlayerQuickActions(*item);
	RumbleLaraHealthCondition(item);

	auto water = GetPlayerWaterData(*item);
	player.Context.WaterSurfaceDist = -water.HeightFromWater;

	if (player.Context.Vehicle == NO_VALUE)
		SpawnPlayerWaterSurfaceEffects(*item, water.WaterHeight, water.WaterDepth);

	bool isWaterOnHeadspace = false;

	// TODO: Move unrelated handling elsewhere.
	// Handle environment state transition.
	if (player.Context.Vehicle == NO_VALUE && player.ExtraAnim == NO_VALUE)
	{
		switch (player.Control.WaterStatus)
		{
		case WaterStatus::Dry:
			for (int i = 0; i < NUM_LARA_MESHES; i++)
				player.Effect.BubbleNodes[i] = 0.0f;

			if (water.HeightFromWater == NO_HEIGHT || water.HeightFromWater < WADE_WATER_DEPTH)
				break;

			Camera.targetElevation = ANGLE(-22.0f);

			// Water is at swim depth; dispatch dive.
			if (water.WaterDepth >= SWIM_WATER_DEPTH && !water.IsSwamp)
			{
				if (water.IsWater)
				{
					item->Pose.Position.y += CLICK(0.5f) - 28; // TODO: Demagic.
					item->Animation.IsAirborne = false;
					player.Control.WaterStatus = WaterStatus::Underwater;
					player.Status.Air = LARA_AIR_MAX;

					for (int i = 0; i < NUM_LARA_MESHES; i++)
						player.Effect.BubbleNodes[i] = PLAYER_BUBBLE_NODE_MAX;

					UpdateLaraRoom(item, 0);
					StopSoundEffect(SFX_TR4_LARA_FALL);

					if (item->Animation.ActiveState == LS_SWAN_DIVE)
					{
						SetAnimation(*item, LA_SWANDIVE_DIVE);
						item->Animation.Velocity.y /= 2;
						item->Pose.Orientation.x = ANGLE(-45.0f);
						player.Control.HandStatus = HandStatus::Free;
					}
					else if (item->Animation.ActiveState == LS_FREEFALL_DIVE)
					{
						SetAnimation(*item, LA_SWANDIVE_FREEFALL_DIVE);
						item->Animation.Velocity.y /= 2;
						item->Pose.Orientation.x = ANGLE(-85.0f);
						player.Control.HandStatus = HandStatus::Free;
					}
					else
					{
						SetAnimation(*item, LA_FREEFALL_DIVE);
						item->Animation.Velocity.y = item->Animation.Velocity.y * (3 / 8.0f);
						item->Pose.Orientation.x = ANGLE(-45.0f);
					}

					ResetPlayerFlex(item);
				}
			}
			// Water is at wade depth; update water status and do special handling.
			else if (water.HeightFromWater >= WADE_WATER_DEPTH)
			{
				player.Control.WaterStatus = WaterStatus::Wade;

				// Make splash ONLY within this particular threshold before swim depth while airborne (SpawnPlayerSplash() above interferes otherwise).
				if (water.WaterDepth > (SWIM_WATER_DEPTH - CLICK(1)) &&
					item->Animation.IsAirborne && !water.IsSwamp)
				{
					item->Animation.TargetState = LS_IDLE;
				}
				// Player is grounded; don't splash again.
				else if (!item->Animation.IsAirborne)
				{
					item->Animation.TargetState = LS_IDLE;
				}
				else if (water.IsSwamp)
				{
					if (item->Animation.ActiveState == LS_SWAN_DIVE ||
						item->Animation.ActiveState == LS_FREEFALL_DIVE)
					{
						item->Pose.Position.y = water.WaterHeight + (BLOCK(1) - 24); // TODO: Demagic.
					}

					SetAnimation(*item, LA_WADE);
				}
			}

			break;

		case WaterStatus::Underwater:
			// Disable potential player resurfacing if health is <= 0.
			// Originals worked without this condition, but TEN does not. -- Lwmte, 11.08.22
			if (item->HitPoints <= 0)
				break;

			// Determine if player's head is above water surface. Needed to prevent
			// pre-TR5 bug where player would keep submerged until root mesh was above water level.
			isWaterOnHeadspace = TestEnvironment(
				ENV_FLAG_WATER, item->Pose.Position.x, item->Pose.Position.y - CLICK(1), item->Pose.Position.z,
				GetPointCollision(*item, 0, 0, -CLICK(1)).GetRoomNumber());

			if (water.WaterDepth == NO_HEIGHT || abs(water.HeightFromWater) >= CLICK(1) || isWaterOnHeadspace ||
				item->Animation.AnimNumber == LA_UNDERWATER_RESURFACE || item->Animation.AnimNumber == LA_ONWATER_DIVE)
			{
				if (!water.IsWater)
				{
					if (water.WaterDepth == NO_HEIGHT || abs(water.HeightFromWater) >= CLICK(1))
					{
						SetAnimation(*item, LA_FALL_START);
						ResetPlayerLean(item);
						ResetPlayerFlex(item);
						item->Animation.IsAirborne = true;
						item->Animation.Velocity.z = item->Animation.Velocity.y;
						item->Animation.Velocity.y = 0.0f;
						player.Control.WaterStatus = WaterStatus::Dry;
					}
					else
					{
						SetAnimation(*item, LA_UNDERWATER_RESURFACE);
						ResetPlayerLean(item);
						ResetPlayerFlex(item);
						item->Animation.Velocity.y = 0.0f;
						item->Pose.Position.y = water.WaterHeight;
						player.Control.WaterStatus = WaterStatus::TreadWater;

						UpdateLaraRoom(item, -(STEPUP_HEIGHT - 3));
					}
				}
			}
			else
			{
				SetAnimation(*item, LA_UNDERWATER_RESURFACE);
				ResetPlayerLean(item);
				ResetPlayerFlex(item);
				item->Animation.Velocity.y = 0.0f;
				item->Pose.Position.y = water.WaterHeight + 1;
				player.Control.WaterStatus = WaterStatus::TreadWater;

				UpdateLaraRoom(item, 0);
			}

			break;

		case WaterStatus::TreadWater:
			if (!water.IsWater)
			{
				if (water.HeightFromWater <= WADE_WATER_DEPTH)
				{
					SetAnimation(*item, LA_FALL_START);
					item->Animation.IsAirborne = true;
					item->Animation.Velocity.z = item->Animation.Velocity.y;
					player.Control.WaterStatus = WaterStatus::Dry;
				}
				else
				{
					SetAnimation(*item, LA_STAND_IDLE);
					player.Control.WaterStatus = WaterStatus::Wade;
				}

				ResetPlayerLean(item);
				ResetPlayerFlex(item);
				item->Animation.Velocity.y = 0.0f;
			}

			break;

		case WaterStatus::Wade:
			Camera.targetElevation = ANGLE(-22.0f);

			if (water.HeightFromWater >= WADE_WATER_DEPTH)
			{
				if (water.HeightFromWater > SWIM_WATER_DEPTH && !water.IsSwamp)
				{
					SetAnimation(*item, LA_ONWATER_IDLE);
					ResetPlayerLean(item);
					ResetPlayerFlex(item);
					item->Animation.IsAirborne = false;
					item->Animation.Velocity.y = 0.0f;
					item->Pose.Position.y += 1 - water.HeightFromWater;
					player.Control.WaterStatus = WaterStatus::TreadWater;

					UpdateLaraRoom(item, 0);
				}
			}
			else
			{
				player.Control.WaterStatus = WaterStatus::Dry;

				if (item->Animation.ActiveState == LS_WADE_FORWARD)
					item->Animation.TargetState = LS_RUN_FORWARD;
			}

			break;
		}
	}

	HandlePlayerStatusEffects(*item, player.Control.WaterStatus, water);

	auto prevPos = item->Pose.Position;

	// Handle environment state.
	switch (player.Control.WaterStatus)
	{
	case WaterStatus::Dry:
	case WaterStatus::Wade:
		LaraAboveWater(item, coll);
		break;

	case WaterStatus::Underwater:
		LaraUnderwater(item, coll);
		break;

	case WaterStatus::TreadWater:
		LaraWaterSurface(item, coll);
		break;

	case WaterStatus::FlyCheat:
		LaraCheat(item, coll);
		break;
	}

	int deltaDist = (int)round(Vector3i::Distance(prevPos, item->Pose.Position));
	SaveGame::Statistics.Game.Distance  += deltaDist;
	SaveGame::Statistics.Level.Distance += deltaDist;

	if (DebugMode)
	{
		DrawNearbyPathfinding(GetPointCollision(*item).GetBottomSector().PathfindingBoxID);
		DrawNearbySectorFlags(*item);
	}
}

void LaraAboveWater(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	// Reset collision setup.
	coll->Setup.Mode = CollisionProbeMode::Quadrants;
	coll->Setup.Radius = LARA_RADIUS;
	coll->Setup.Height = LARA_HEIGHT;
	coll->Setup.UpperCeilingBound = NO_UPPER_BOUND;
	coll->Setup.BlockFloorSlopeUp = false;
	coll->Setup.BlockFloorSlopeDown = false;
	coll->Setup.BlockCeilingSlope = false;
	coll->Setup.BlockDeathFloorDown = false;
	coll->Setup.BlockMonkeySwingEdge = false;
	coll->Setup.EnableObjectPush = true;
	coll->Setup.EnableSpasm = true;
	coll->Setup.ForceSolidStatics = false;
	coll->Setup.PrevPosition = item->Pose.Position;
	coll->Setup.PrevAnimObjectID = item->Animation.AnimObjectID;
	coll->Setup.PrevAnimNumber = item->Animation.AnimNumber;
	coll->Setup.PrevFrameNumber = item->Animation.FrameNumber;
	coll->Setup.PrevState = item->Animation.ActiveState;

	// Handle look-around.
	if (((IsHeld(In::Look) && CanPlayerLookAround(*item)) ||
			(player.Control.Look.IsUsingBinoculars || player.Control.Look.IsUsingLasersight)) &&
		player.ExtraAnim == NO_VALUE)
	{
		HandlePlayerLookAround(*item);
	}
	else
	{
		// TODO: Extend ResetLaraFlex() to be a catch-all function.
		ResetPlayerLookAround(*item);
	}
	player.Control.Look.Mode = LookMode::None;

	UpdateLaraRoom(item, -LARA_HEIGHT / 2);

	// Process vehicles.
	if (HandleLaraVehicle(item, coll))
		return;

	HandlePlayerBehaviorState(*item, *coll, PlayerBehaviorStateRoutineType::Control);
	HandleLaraMovementParameters(item, coll);
	AnimateItem(*item);

	if (player.ExtraAnim == NO_VALUE)
	{
		// Check for collision with items.
		DoObjectCollision(item, coll);

		// Handle player behavior state collision.
		if (player.Context.Vehicle == NO_VALUE)
			HandlePlayerBehaviorState(*item, *coll, PlayerBehaviorStateRoutineType::Collision);
	}

	HandleWeapon(*item);
	LaraBreath(item);

	ProcessSectorFlags(item);
	TestTriggers(item, false);
	TestVolumes(item->Index, &coll->Setup);
}

void LaraWaterSurface(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.IsLow = false;
	Camera.targetElevation = -ANGLE(22.0f);

	// Reset collision setup.
	coll->Setup.Mode = CollisionProbeMode::FreeForward;
	coll->Setup.Radius = LARA_RADIUS;
	coll->Setup.Height = LARA_HEIGHT_SURFACE;
	coll->Setup.LowerFloorBound = NO_LOWER_BOUND;
	coll->Setup.UpperFloorBound = -CLICK(0.5f);
	coll->Setup.LowerCeilingBound = LARA_RADIUS;
	coll->Setup.UpperCeilingBound = NO_UPPER_BOUND;
	coll->Setup.BlockFloorSlopeUp = false;
	coll->Setup.BlockFloorSlopeDown = false;
	coll->Setup.BlockCeilingSlope = false;
	coll->Setup.BlockDeathFloorDown = false;
	coll->Setup.BlockMonkeySwingEdge = false;
	coll->Setup.EnableObjectPush = true;
	coll->Setup.EnableSpasm = false;
	coll->Setup.ForceSolidStatics = false;
	coll->Setup.PrevPosition = item->Pose.Position;

	// Handle look-around.
	if (IsHeld(In::Look) && CanPlayerLookAround(*item))
	{
		HandlePlayerLookAround(*item);
	}
	else
	{
		ResetPlayerLookAround(*item);
	}

	player.Control.Count.Pose = 0;

	HandlePlayerBehaviorState(*item, *coll, PlayerBehaviorStateRoutineType::Control);

	const auto& level = *g_GameFlow->GetLevel(CurrentLevel);
	bool hasDivesuit = (level.GetLaraType() == LaraType::Divesuit);

	// TODO: Subsuit gradually slows down at rate of 0.5 degrees. @Sezz 2022.06.23
	// Apply and reset turn rate.
	item->Pose.Orientation.y += player.Control.TurnRate;
	if (!(IsHeld(In::Left) || IsHeld(In::Right)))
		player.Control.TurnRate = 0;

	if (hasDivesuit)
		UpdateLaraSubsuitAngles(item);

	// Reset lean.
	if (!player.Control.IsMoving && !(IsHeld(In::Left) || IsHeld(In::Right)))
		ResetPlayerLean(item, 1 / 8.0f);

	if (player.Context.WaterCurrentActive && player.Control.WaterStatus != WaterStatus::FlyCheat)
		LaraWaterCurrent(item, coll);

	AnimateItem(*item);
	item->Pose.Translate(player.Control.MoveAngle, item->Animation.Velocity.y);

	DoObjectCollision(item, coll);

	if (player.Context.Vehicle == NO_VALUE)
		HandlePlayerBehaviorState(*item, *coll, PlayerBehaviorStateRoutineType::Collision);

	UpdateLaraRoom(item, LARA_RADIUS);
	HandleWeapon(*item);

	ProcessSectorFlags(item);
	TestTriggers(item, false);
	TestVolumes(item->Index);
}

void LaraUnderwater(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.IsLow = false;

	// Reset collision setup.
	coll->Setup.Mode = CollisionProbeMode::Quadrants;
	coll->Setup.Radius = LARA_RADIUS_UNDERWATER;
	coll->Setup.Height = LARA_HEIGHT;
	coll->Setup.LowerFloorBound = NO_LOWER_BOUND;
	coll->Setup.UpperFloorBound = -(LARA_RADIUS_UNDERWATER + (LARA_RADIUS_UNDERWATER / 3));
	coll->Setup.LowerCeilingBound = LARA_RADIUS_UNDERWATER + (LARA_RADIUS_UNDERWATER / 3);
	coll->Setup.UpperCeilingBound = NO_UPPER_BOUND;
	coll->Setup.BlockFloorSlopeUp = false;
	coll->Setup.BlockFloorSlopeDown = false;
	coll->Setup.BlockCeilingSlope = false;
	coll->Setup.BlockDeathFloorDown = false;
	coll->Setup.BlockMonkeySwingEdge = false;
	coll->Setup.EnableObjectPush = true;
	coll->Setup.EnableSpasm = false;
	coll->Setup.ForceSolidStatics = false;
	coll->Setup.PrevPosition = item->Pose.Position;

	// Handle look-around.
	if (IsHeld(In::Look) && CanPlayerLookAround(*item))
	{
		HandlePlayerLookAround(*item);
	}
	else
	{
		ResetPlayerLookAround(*item);
	}

	player.Control.Count.Pose = 0;

	HandlePlayerBehaviorState(*item, *coll, PlayerBehaviorStateRoutineType::Control);

	const auto& level = *g_GameFlow->GetLevel(CurrentLevel);
	bool hasDivesuit = (level.GetLaraType() == LaraType::Divesuit);

	// TODO: Subsuit gradually slowed down at rate of 0.5 degrees. @Sezz 2022.06.23
	// Apply and reset turn rate.
	item->Pose.Orientation.y += player.Control.TurnRate;
	if (!(IsHeld(In::Left) || IsHeld(In::Right)))
		player.Control.TurnRate = 0;

	if (hasDivesuit)
		UpdateLaraSubsuitAngles(item);

	if (!player.Control.IsMoving && !(IsHeld(In::Left) || IsHeld(In::Right)))
		ResetPlayerLean(item, 1 / 8.0f, true, false);

	if (item->Pose.Orientation.x < -ANGLE(85.0f))
	{
		item->Pose.Orientation.x = -ANGLE(85.0f);
	}
	else if (item->Pose.Orientation.x > ANGLE(85.0f))
	{
		item->Pose.Orientation.x = ANGLE(85.0f);
	}

	if (hasDivesuit)
	{
		if (item->Pose.Orientation.z > ANGLE(44.0f))
		{
			item->Pose.Orientation.z = ANGLE(44.0f);
		}
		else if (item->Pose.Orientation.z < -ANGLE(44.0f))
		{
			item->Pose.Orientation.z = -ANGLE(44.0f);
		}
	}
	else
	{
		if (item->Pose.Orientation.z > ANGLE(22.0f))
		{
			item->Pose.Orientation.z = ANGLE(22.0f);
		}
		else if (item->Pose.Orientation.z < -ANGLE(22.0f))
		{
			item->Pose.Orientation.z = -ANGLE(22.0f);
		}
	}

	if (player.Context.WaterCurrentActive && player.Control.WaterStatus != WaterStatus::FlyCheat)
		LaraWaterCurrent(item, coll);

	AnimateItem(*item);
	item->Pose.Translate(item->Pose.Orientation, item->Animation.Velocity.y);

	DoObjectCollision(item, coll);

	if (player.Context.Vehicle == NO_VALUE)
		HandlePlayerBehaviorState(*item, *coll, PlayerBehaviorStateRoutineType::Collision);

	UpdateLaraRoom(item, 0);
	HandleWeapon(*item);

	ProcessSectorFlags(item);
	TestTriggers(item, false);
	TestVolumes(item->Index);
}

void LaraCheat(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	item->HitPoints = LARA_HEALTH_MAX;
	player.Status.Air = LARA_AIR_MAX;
	player.Status.Exposure = LARA_EXPOSURE_MAX;
	player.Status.Poison = 0;
	player.Status.Stamina = LARA_STAMINA_MAX;
	
	LaraUnderwater(item, coll);

	if (IsHeld(In::Walk) && !IsHeld(In::Look))
	{
		if (TestEnvironment(ENV_FLAG_WATER, item) ||
			(player.Context.WaterSurfaceDist > 0 && player.Context.WaterSurfaceDist != NO_HEIGHT))
		{
			SetAnimation(*item, LA_UNDERWATER_IDLE);
			player.Control.WaterStatus = WaterStatus::Underwater;
		}
		else
		{
			SetAnimation(*item, LA_STAND_IDLE);
			item->Pose.Orientation.x = 0;
			item->Pose.Orientation.z = 0;
			player.Control.WaterStatus = WaterStatus::Dry;
		}

		ResetPlayerFlex(item);
		InitializeLaraMeshes(item);
		item->Animation.IsAirborne = false;
		item->HitPoints = LARA_HEALTH_MAX;
		player.Control.HandStatus = HandStatus::Free;
	}
}

void UpdateLara(ItemInfo* item, bool isTitle)
{
	if (isTitle && !g_GameFlow->IsLaraInTitleEnabled())
		return;

	// HACK: backup controls until proper control lock is implemented -- Lwmte, 07.12.22
	auto actionMap = ActionMap;

	if (isTitle)
		ClearAllActions();

	// Control player.
	InItemControlLoop = true;

	LaraControl(item, &LaraCollision);
	HandlePlayerFlyCheat(*item);
	InItemControlLoop = false;
	KillMoveItems();

	if (isTitle)
		ActionMap = actionMap;

	// Update player animations.
	g_Renderer.UpdateLaraAnimations(true);

	// Update player effects.
	HairEffect.Update(*item);
	HandlePlayerWetnessDrips(*item);
	HandlePlayerDiveBubbles(*item);
	ProcessEffects(item);
}

// Offset values may be used to account for the quirk of room traversal only being able to occur at portals.
bool UpdateLaraRoom(ItemInfo* item, int height, int xOffset, int zOffset)
{
	auto point = Geometry::TranslatePoint(item->Pose.Position, item->Pose.Orientation.y, zOffset, height, xOffset);

	// Hacky L-shaped Location traversal.
	item->Location = GetRoomVector(item->Location, point);
	item->Location = GetRoomVector(item->Location, Vector3i(item->Pose.Position.x, point.y, item->Pose.Position.z));
	item->Floor = GetSurfaceHeight(item->Location, item->Pose.Position.x, item->Pose.Position.z, true).value_or(NO_HEIGHT);

	if (item->RoomNumber != item->Location.RoomNumber)
	{
		ItemNewRoom(item->Index, item->Location.RoomNumber);
		return true;
	}

	return false;
}
