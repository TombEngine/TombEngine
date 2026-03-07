#include "framework.h"
#include "Game/Lara/lara_helpers.h"

#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Game/camera.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/floordata.h"
#include "Game/collision/Point.h"
#include "Game/control/control.h"
#include "Game/control/volume.h"
#include "Game/items.h"
#include "Game/effects/Bubble.h"
#include "Game/effects/Drip.h"
#include "Game/GuiObjects.h"
#include "Game/Lara/PlayerContext.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_collide.h"
#include "Game/Lara/lara_fire.h"
#include "Game/Lara/lara_tests.h"
#include "Game/savegame.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Scripting/Include/ScriptInterfaceLevel.h"
#include "Sound/sound.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"
#include "Specific/trutils.h"

#include "Objects/TR2/Vehicles/skidoo.h"
#include "Objects/TR3/Vehicles/big_gun.h"
#include "Objects/TR3/Vehicles/kayak.h"
#include "Objects/TR3/Vehicles/minecart.h"
#include "Objects/TR3/Vehicles/quad_bike.h"
#include "Objects/TR3/Vehicles/upv.h"
#include "Objects/TR4/Vehicles/jeep.h"
#include "Objects/TR4/Vehicles/motorbike.h"

using namespace TEN::Collision::Floordata;
using namespace TEN::Collision::Point;
using namespace TEN::Control::Volumes;
using namespace TEN::Effects::Bubble;
using namespace TEN::Effects::Drip;
using namespace TEN::Entities::Player;
using namespace TEN::Gui;
using namespace TEN::Input;
using namespace TEN::Math;
using namespace TEN::Utils;

// -----------------------------
// HELPER FUNCTIONS
// For State Control & Collision
// -----------------------------

void HandleLaraMovementParameters(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	// Update AFK pose timer.
	if (player.Control.Count.Pose < (g_GameFlow->GetSettings()->Animations.PoseTimeout * FPS) &&
		!(IsHeld(In::Look) || IsOpticActionHeld())/* &&
		g_GameFlow->HasAFKPose()*/) // TODO: Restore.
	{
		player.Control.Count.Pose++;
	}
	else
	{
		player.Control.Count.Pose = 0;
	}

	// Reset running jump timer.
	if (!IsRunJumpCountableState(item->Animation.ActiveState))
		player.Control.Count.Run = 0;

	// Reset running jump action queue.
	if (!IsRunJumpQueueableState(item->Animation.ActiveState))
		player.Control.IsRunJumpQueued = false;

	// Reset lean.
	if ((!player.Control.IsMoving || (player.Control.IsMoving && !(IsHeld(In::Left) || IsHeld(In::Right)))) &&
		(!player.Control.IsLow && item->Animation.ActiveState != LS_DEATH)) // HACK: Don't interfere with surface alignment in crouch, crawl, and death states.
	{
		if (g_Configuration.IsUsingModernControls())
		{
			// TODO: Must check. Condition is probably more precise.
			if (GetMoveAxis() == Vector2::Zero)
				ResetPlayerLean(item, 1 / 6.0f);
		}
		else
		{
			ResetPlayerLean(item, 1 / 6.0f);
		}
	}

	// Reset crawl flex.
	if (!IsHeld(In::Look) && coll->Setup.Height > LARA_HEIGHT - LARA_HEADROOM && // HACK
		(item->Animation.Velocity.z == 0.0f || (item->Animation.Velocity.z != 0.0f && !(IsHeld(In::Left) || IsHeld(In::Right)))))
	{
		ResetPlayerFlex(item, 0.1f);
	}

	if (!g_Configuration.IsUsingModernControls())
	{
		// Apply and reset turn rate.
		item->Pose.Orientation.y += player.Control.TurnRate.y;
		if (!(IsHeld(In::Left) || IsHeld(In::Right)))
			player.Control.TurnRate.y = 0;
	}

	player.Control.IsLow = false;
	player.Control.IsMonkeySwinging = false;
}

void HandlePlayerStatusEffects(ItemInfo& item, WaterStatus waterStatus, PlayerWaterData& water)
{
	auto& player = GetLaraInfo(item);

	// Update health status.
	if (TestEnvironment(ENV_FLAG_DAMAGE, &item) && item.HitPoints > 0)
		item.HitPoints--;

	// Update poison status.
	if (player.Status.Poison)
	{
		if (player.Status.Poison > LARA_POISON_MAX)
			player.Status.Poison = LARA_POISON_MAX;

		if (!(Wibble & 0xFF))
			DoDamage(&item, player.Status.Poison, true);
	}

	// Update stamina status.
	if (player.Status.Stamina < LARA_STAMINA_MAX && item.Animation.ActiveState != LS_SPRINT)
		player.Status.Stamina++;

	// TODO: Dehardcode values and make cleaner implementation.
	// Handle environmental status effects.
	switch (waterStatus)
	{
	case WaterStatus::Dry:
	case WaterStatus::Wade:
		// TODO: Find best height. -- Sezz 2021.11.10
		if (water.IsSwamp && player.Context.WaterSurfaceDist < -(LARA_HEIGHT + 8))
		{
			if (item.HitPoints >= 0)
			{
				player.Status.Air -= 6;
				if (player.Status.Air < 0)
				{
					player.Status.Air = -1;
					DoDamage(&item, 10, true);
				}
			}
		}
		else if (player.Status.Air < LARA_AIR_MAX && item.HitPoints >= 0)
		{
			// HACK: Special case for UPV.
			if (player.Context.Vehicle == NO_VALUE)
			{
				player.Status.Air += 10;
				if (player.Status.Air > LARA_AIR_MAX)
					player.Status.Air = LARA_AIR_MAX;
			}
		}

		if (item.HitPoints >= 0)
		{
			if (player.Control.WaterStatus == WaterStatus::Dry)
			{
				// HACK: Special case for UPV.
				if (player.Context.Vehicle != NO_VALUE)
				{
					const auto& vehicleItem = g_Level.Items[player.Context.Vehicle];
					if (vehicleItem.ObjectNumber == ID_UPV)
					{
						auto pointColl = GetPointCollision(item, 0, 0, CLICK(1));

						water.IsCold = (water.IsCold || TestEnvironment(ENV_FLAG_COLD, pointColl.GetRoomNumber()));
						if (water.IsCold)
						{
							player.Status.Exposure--;
							if (player.Status.Exposure <= 0)
							{
								player.Status.Exposure = 0;
								DoDamage(&item, 10, true);
							}
						}
					}
				}
				else
				{
					player.Status.Exposure++;
					if (player.Status.Exposure >= LARA_EXPOSURE_MAX)
						player.Status.Exposure = LARA_EXPOSURE_MAX;
				}
			}
			else
			{
				if (water.IsCold)
				{
					player.Status.Exposure--;
					if (player.Status.Exposure <= 0)
					{
						player.Status.Exposure = 0;
						DoDamage(&item, 10, true);
					}
				}
				else
				{
					player.Status.Exposure++;
					if (player.Status.Exposure >= LARA_EXPOSURE_MAX)
						player.Status.Exposure = LARA_EXPOSURE_MAX;
				}
			}
		}

		break;

	case WaterStatus::Underwater:
		if (item.HitPoints >= 0)
		{
			const auto& level = *g_GameFlow->GetLevel(CurrentLevel);
			if (level.GetLaraType() != LaraType::Divesuit)
				player.Status.Air--;

			if (player.Status.Air < 0)
			{
				player.Status.Air = -1;
				DoDamage(&item, 5, true);
			}

			if (water.IsCold)
			{
				player.Status.Exposure -= 2;
				if (player.Status.Exposure <= 0)
				{
					player.Status.Exposure = 0;
					DoDamage(&item, 10, true);
				}
			}
			else
			{
				player.Status.Exposure++;
				if (player.Status.Exposure >= LARA_EXPOSURE_MAX)
					player.Status.Exposure = LARA_EXPOSURE_MAX;
			}
		}

		break;

	case WaterStatus::TreadWater:
		if (item.HitPoints >= 0)
		{
			player.Status.Air += 10;
			if (player.Status.Air > LARA_AIR_MAX)
				player.Status.Air = LARA_AIR_MAX;

			if (water.IsCold)
			{
				player.Status.Exposure -= 2;
				if (player.Status.Exposure <= 0)
				{
					player.Status.Exposure = 0;
					DoDamage(&item, 10, true);
				}
			}
		}

		break;

	default:
		break;
	}

	// Update death counter.
	if (item.HitPoints <= 0)
	{
		item.HitPoints = -1;

		if (player.Control.Count.Death == 0)
			StopSoundTracks(true);

		player.Control.Count.Death++;
		if ((item.Flags & IFLAG_INVISIBLE))
		{
			player.Control.Count.Death++;
			return;
		}
	}
}

static LaraWeaponType GetPlayerScrolledWeaponType(const ItemInfo& item, LaraWeaponType currentWeaponType, bool getPrev)
{
	static const auto SCROLL_WEAPON_TYPES = std::vector<LaraWeaponType>
	{
		LaraWeaponType::Pistol,
		LaraWeaponType::Shotgun,
		LaraWeaponType::Uzi,
		LaraWeaponType::Revolver,
		LaraWeaponType::GrenadeLauncher,
		LaraWeaponType::Crossbow,
		LaraWeaponType::HarpoonGun,
		LaraWeaponType::HK,
		LaraWeaponType::RocketLauncher
	};

	auto getNextIndex = [getPrev](int index)
	{
		return (index + (getPrev ? ((int)SCROLL_WEAPON_TYPES.size() - 1) : 1)) % (int)SCROLL_WEAPON_TYPES.size();
	};

	auto& player = GetLaraInfo(item);

	// Get vector index for current weapon type.
	auto currentIndex = NO_VALUE;
	for (int i = 0; i < SCROLL_WEAPON_TYPES.size(); i++)
	{
		if (SCROLL_WEAPON_TYPES[i] == currentWeaponType)
		{
			currentIndex = i;
			break;
		}
	}

	// Invalid current weapon type; return None type.
	if (currentIndex == NO_VALUE)
		return LaraWeaponType::None;

	// Get next valid weapon type in sequence.
	int nextIndex = getNextIndex(currentIndex);
	while (nextIndex != currentIndex)
	{
		auto nextWeaponType = SCROLL_WEAPON_TYPES[nextIndex];
		if (player.Weapons[(int)nextWeaponType].Present)
			return nextWeaponType;

		nextIndex = getNextIndex(nextIndex);
	}

	// No valid weapon type; return None type.
	return LaraWeaponType::None;
}

void HandlePlayerQuickActions(ItemInfo& item)
{
	auto& player = GetLaraInfo(item);

	// Handle medipacks.
	if (IsClicked(In::SmallMedipack))
	{
		g_Gui.UseItem(item, GAME_OBJECT_ID::ID_SMALLMEDI_ITEM);
	}
	else if (IsClicked(In::LargeMedipack))
	{
		g_Gui.UseItem(item, GAME_OBJECT_ID::ID_BIGMEDI_ITEM);
	}

	// Don't process weapon hotkeys in optics mode.
	if (player.Control.Look.IsUsingBinoculars)
		return;

	// Handle weapon scroll request.
	if (IsClicked(In::PreviousWeapon) || IsClicked(In::NextWeapon))
	{
		auto weaponType = GetPlayerScrolledWeaponType(item, player.Control.Weapon.GunType, IsClicked(In::PreviousWeapon));
		if (weaponType != LaraWeaponType::None)
			player.Control.Weapon.RequestGunType = weaponType;
	}

	auto requestedGunType = LaraWeaponType::None;

	// Handle weapon requests.
	if (IsClicked(In::Weapon1) && player.Weapons[(int)LaraWeaponType::Pistol].Present)
		requestedGunType = LaraWeaponType::Pistol;

	if (IsClicked(In::Weapon2) && player.Weapons[(int)LaraWeaponType::Shotgun].Present)
		requestedGunType = LaraWeaponType::Shotgun;

	if (IsClicked(In::Weapon3) && player.Weapons[(int)LaraWeaponType::Uzi].Present)
		requestedGunType = LaraWeaponType::Uzi;

	if (IsClicked(In::Weapon4) && player.Weapons[(int)LaraWeaponType::Revolver].Present)
		requestedGunType = LaraWeaponType::Revolver;

	if (IsClicked(In::Weapon5) && player.Weapons[(int)LaraWeaponType::GrenadeLauncher].Present)
		requestedGunType = LaraWeaponType::GrenadeLauncher;

	if (IsClicked(In::Weapon6) && player.Weapons[(int)LaraWeaponType::Crossbow].Present)
		requestedGunType = LaraWeaponType::Crossbow;

	if (IsClicked(In::Weapon7) && player.Weapons[(int)LaraWeaponType::HarpoonGun].Present)
		requestedGunType = LaraWeaponType::HarpoonGun;

	if (IsClicked(In::Weapon8) && player.Weapons[(int)LaraWeaponType::HK].Present)
		requestedGunType = LaraWeaponType::HK;

	if (IsClicked(In::Weapon9) && player.Weapons[(int)LaraWeaponType::RocketLauncher].Present)
		requestedGunType = LaraWeaponType::RocketLauncher;

	// TODO: 10th possible weapon, probably grapple gun.
	/*if (IsClicked(In::Weapon10) && player.Weapons[(int)LaraWeaponType::].Present)
		requestedGunType = LaraWeaponType::;*/

	if (requestedGunType != LaraWeaponType::None)
	{
		player.Control.Weapon.RequestGunType = requestedGunType;

		// Reset current weapon if it matches the requested type, so that player can re-unholster it.
		if (player.Control.HandStatus == HandStatus::Free &&
			player.Control.Weapon.RequestGunType == player.Control.Weapon.GunType)
		{
			player.Control.Weapon.GunType = LaraWeaponType::None;
		}
	}
}

bool CanPlayerLookAround(const ItemInfo& item)
{
	const auto& player = GetLaraInfo(item);

	// 1) Check for modern control mode.
	if (g_Configuration.IsUsingModernControls())
		return false;

	// 2) Check if look mode is not None.
	if (player.Control.Look.Mode == LookMode::None)
		return false;

	// 3) Check if drawn weapon has lasersight.
	if (player.Control.HandStatus == HandStatus::WeaponReady &&
		player.Weapons[(int)player.Control.Weapon.GunType].HasLasersight)
	{
		return true;
	}

	// 4) Test for switchable target.
	if (player.Control.HandStatus == HandStatus::WeaponReady &&
		player.TargetEntity != nullptr)
	{
		unsigned int targetableCount = 0;
		for (const auto* targetPtr : player.TargetList)
		{
			if (targetPtr != nullptr)
				targetableCount++;

			if (targetableCount > 1)
				return false;
		}
	}

	return true;
}

static void ClearPlayerLookAroundActions(const ItemInfo& item)
{
	const auto& player = GetLaraInfo(item);

	switch (player.Control.Look.Mode)
	{
	default:
	case LookMode::None:
		break;

	case LookMode::Vertical:
		ClearAction(In::Forward);
		ClearAction(In::Back);
		break;

	case LookMode::Horizontal:
		ClearAction(In::Left);
		ClearAction(In::Right);
		break;

	case LookMode::Free:
		ClearAction(In::Forward);
		ClearAction(In::Back);
		ClearAction(In::Left);
		ClearAction(In::Right);
		break;
	}
}
static void SetPlayerOptics(ItemInfo* item)
{
	constexpr auto OPTIC_RANGE_DEFAULT = ANGLE(0.7f);

	auto& player = GetLaraInfo(*item);

	bool breakOptics = true;

	// Standing; can use optics.
	if (item->Animation.ActiveState == LS_IDLE || item->Animation.AnimNumber == LA_STAND_IDLE)
		breakOptics = false;

	// Crouching; can use optics.
	if ((player.Control.IsLow || !IsHeld(In::Crouch)) &&
		(item->Animation.TargetState == LS_CROUCH_IDLE || item->Animation.AnimNumber == LA_CROUCH_IDLE))
	{
		breakOptics = false;
	}

	// If lasersight and Look is not held, exit optics.
	if (player.Control.Look.IsUsingLasersight && !IsHeld(In::Look))
		breakOptics = true;

	// If lasersight and weapon is holstered, exit optics.
	if (player.Control.Look.IsUsingLasersight && IsHeld(In::Draw))
		breakOptics = true;

	// Engage lasersight if available.
	if (!player.Control.Look.IsUsingLasersight && !breakOptics && IsHeld(In::Look))
	{
		if (player.Control.HandStatus == HandStatus::WeaponReady &&
			((player.Control.Weapon.GunType == LaraWeaponType::HK && player.Weapons[(int)LaraWeaponType::HK].HasLasersight) ||
				(player.Control.Weapon.GunType == LaraWeaponType::Revolver && player.Weapons[(int)LaraWeaponType::Revolver].HasLasersight) ||
				(player.Control.Weapon.GunType == LaraWeaponType::Crossbow && player.Weapons[(int)LaraWeaponType::Crossbow].HasLasersight)))
		{
			player.Control.Look.OpticRange = OPTIC_RANGE_DEFAULT;
			player.Control.Look.IsUsingBinoculars = true;
			player.Control.Look.IsUsingLasersight = true;
			player.Inventory.IsBusy = true;

			g_Camera.PrevBinocularCameraType = g_Camera.oldType;
			return;
		}
	}

	if (!breakOptics)
		return;

	// Not using optics; return early.
	if (!player.Control.Look.IsUsingBinoculars && !player.Control.Look.IsUsingLasersight)
		return;

	player.Control.Look.OpticRange = 0;
	player.Control.Look.IsUsingBinoculars = false;
	player.Control.Look.IsUsingLasersight = false;
	player.Inventory.IsBusy = false;

	g_Camera.type = g_Camera.PrevBinocularCameraType;
	g_Camera.bounce = 0;
	SetFov(g_Camera.PrevFov);
}

static short NormalizeLookAroundTurnRate(short turnRate, short opticRange)
{
	constexpr auto ZOOM_LEVEL_MAX = ANGLE(10.0f);
	constexpr auto ZOOM_LEVEL_REF = ANGLE(17.0f);

	if (opticRange == 0)
		return turnRate;

	return short(turnRate * (ZOOM_LEVEL_MAX - opticRange) / ZOOM_LEVEL_REF);
};

void HandlePlayerLookAround(ItemInfo& item, bool invertXAxis)
{
	constexpr auto OPTIC_RANGE_MAX	= ANGLE(8.5f);
	constexpr auto OPTIC_RANGE_MIN	= ANGLE(0.7f);
	constexpr auto OPTIC_RANGE_RATE = ANGLE(0.35f);
	constexpr auto TURN_RATE_MAX	= ANGLE(4.0f);
	constexpr auto TURN_RATE_ACCEL	= ANGLE(0.75f);

	auto& player = GetLaraInfo(item);

	// Set optics.
	g_Camera.type = CameraType::Look;
	SetPlayerOptics(LaraItem);

	bool isSlow = IsHeld(In::Walk);

	// Zoom optics.
	if (player.Control.Look.IsUsingBinoculars || player.Control.Look.IsUsingLasersight)
	{
		short rangeRate = isSlow ? (OPTIC_RANGE_RATE / 2) : OPTIC_RANGE_RATE;

		// NOTE: Zooming allowed with either StepLeft/StepRight or Walk/Sprint.
		if ((IsHeld(In::StepLeft) && !IsHeld(In::StepRight)) ||
			(IsHeld(In::Walk) && !IsHeld(In::Sprint)))
		{
			player.Control.Look.OpticRange -= rangeRate;
			if (player.Control.Look.OpticRange < OPTIC_RANGE_MIN)
			{
				player.Control.Look.OpticRange = OPTIC_RANGE_MIN;
			}
			else
			{
				SoundEffect(SFX_TR4_BINOCULARS_ZOOM, nullptr, SoundEnvironment::Land, 0.9f);
			}
		}
		else if ((IsHeld(In::StepRight) && !IsHeld(In::StepLeft)) ||
			(IsHeld(In::Sprint) && !IsHeld(In::Walk)))
		{
			player.Control.Look.OpticRange += rangeRate;
			if (player.Control.Look.OpticRange > OPTIC_RANGE_MAX)
			{
				player.Control.Look.OpticRange = OPTIC_RANGE_MAX;
			}
			else
			{
				SoundEffect(SFX_TR4_BINOCULARS_ZOOM, nullptr, SoundEnvironment::Land, 1.0f);
			}
		}
	}

	// Rotate.
	if (g_Configuration.EnableTankCameraControl &&
		(GetMouseAxis() != Vector2::Zero || GetCameraAxis() != Vector2::Zero))
	{
		auto rot = g_Camera.GetControlRotation() / 2;
		player.Control.Look.Orientation.x += -rot.y;
		player.Control.Look.Orientation.y += rot.x;
	}
	else
	{
		auto axisCoeff = Vector2::Zero;

		// Determine X axis coefficient.
		if ((IsHeld(In::Forward) || IsHeld(In::Back)) &&
			(player.Control.Look.Mode == LookMode::Free || player.Control.Look.Mode == LookMode::Vertical))
		{
			axisCoeff.x = GetMoveAxis().y;
		}

		// Determine Y axis coefficient.
		if ((IsHeld(In::Left) || IsHeld(In::Right)) &&
			(player.Control.Look.Mode == LookMode::Free || player.Control.Look.Mode == LookMode::Horizontal))
		{
			axisCoeff.y = GetMoveAxis().x;
		}

		// Determine turn rate base.
		short turnRateMax = isSlow ? (TURN_RATE_MAX / 2) : TURN_RATE_MAX;
		short turnRateAccel = isSlow ? (TURN_RATE_ACCEL / 2) : TURN_RATE_ACCEL;

		// Normalize turn rate base.
		turnRateMax = NormalizeLookAroundTurnRate(turnRateMax, player.Control.Look.OpticRange);
		turnRateAccel = NormalizeLookAroundTurnRate(turnRateAccel, player.Control.Look.OpticRange);

		// Modulate turn rate.
		player.Control.Look.TurnRate = EulerAngles(
			ModulateLaraTurnRate(player.Control.Look.TurnRate.x, turnRateAccel, 0, turnRateMax, axisCoeff.x, invertXAxis),
			ModulateLaraTurnRate(player.Control.Look.TurnRate.y, turnRateAccel, 0, turnRateMax, axisCoeff.y, false),
			0);

		// Apply turn rate.
		player.Control.Look.Orientation += player.Control.Look.TurnRate;
	}

	// Clamp orientation.
	player.Control.Look.Orientation = EulerAngles(
		std::clamp(player.Control.Look.Orientation.x, LOOKCAM_ORIENT_CONSTRAINT.first.x, LOOKCAM_ORIENT_CONSTRAINT.second.x),
		std::clamp(player.Control.Look.Orientation.y, LOOKCAM_ORIENT_CONSTRAINT.first.y, LOOKCAM_ORIENT_CONSTRAINT.second.y),
		0);

	// Visually adapt head and torso orientations.
	player.ExtraHeadRot = player.Control.Look.Orientation / 2;

	if (player.Control.HandStatus != HandStatus::Busy &&
		!player.Control.IsLow &&
		!player.LeftArm.Locked && !player.RightArm.Locked &&
		player.Context.Vehicle == NO_VALUE)
	{
		player.ExtraTorsoRot = player.ExtraHeadRot;
	}

	ClearPlayerLookAroundActions(item);
}

bool HandleLaraVehicle(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	if (lara->Context.Vehicle == NO_VALUE)
		return false;

	if (!g_Level.Items[lara->Context.Vehicle].Active)
	{
		lara->Context.Vehicle = NO_VALUE;
		item->Animation.IsAirborne = true;
		SetAnimation(item, LA_FALL_START);
		return false;
	}

	TestVolumes(lara->Context.Vehicle);

	switch (g_Level.Items[lara->Context.Vehicle].ObjectNumber)
	{
	case ID_QUAD:
		QuadBikeControl(item, coll);
		break;

	case ID_JEEP:
		JeepControl(item);
		break;

	case ID_MOTORBIKE:
		MotorbikeControl(item, coll);
		break;

	case ID_KAYAK:
		KayakControl(item);
		break;

	case ID_SNOWMOBILE:
		SkidooControl(item, coll);
		break;

	case ID_UPV:
		UPVControl(item, coll);
		break;

	case ID_MINECART:
		MinecartControl(item);
		break;

	case ID_BIGGUN:
		BigGunControl(item, coll);
		break;

	// Boats are processed like normal items in loop.
	default:
		HandleWeapon(*item);
	}

	return true;
}

// Tank control version.
void HandlePlayerTurnLean(ItemInfo* item, CollisionInfo* coll, short baseRate, short maxAngle)
{
	auto& player = GetLaraInfo(*item);

	if (item->Animation.Velocity.z == 0.0f)
		return;

	float axisCoeff = GetMoveAxis().x;
	int sign = copysign(1, axisCoeff);
	maxAngle *= axisCoeff;

	if (coll->CollisionType == CollisionType::Left || coll->CollisionType == CollisionType::Right)
		maxAngle *= 0.6f;

	item->Pose.Orientation.z += std::min<short>(baseRate, abs(maxAngle - item->Pose.Orientation.z) / 3) * sign;
}

// TODO: Make static.
// Tank control version.
void HandlePlayerCrawlTurnFlex(ItemInfo& item)
{
	constexpr auto FLEX_RATE_ANGLE		 = ANGLE(2.25f);
	constexpr auto FLEX_ANGLE_CONSTRAINT = ANGLE(50.0f) / 2; // 2 = hardcoded number of bones to flex (head and torso).

	auto& player = GetLaraInfo(item);

	// Check if player is moving.
	if (item.Animation.Velocity.z == 0.0f)
		return;

	float axisCoeff = GetMoveAxis().x;
	int sign = copysign(1, axisCoeff);
	short maxAngleNormalized = FLEX_ANGLE_CONSTRAINT * axisCoeff;

	if (abs(player.ExtraTorsoRot.z) < FLEX_ANGLE_CONSTRAINT)
		player.ExtraTorsoRot.z += std::min<short>(FLEX_RATE_ANGLE, abs(maxAngleNormalized - player.ExtraTorsoRot.z) / 6) * sign;

	if (!IsHeld(In::Look) && item.Animation.ActiveState != LS_CRAWL_BACK)
	{
		player.ExtraHeadRot.z = player.ExtraTorsoRot.z / 2;
		player.ExtraHeadRot.y = player.ExtraHeadRot.z;
	}
}

// Tank control version.
void HandlePlayerTurn(ItemInfo& item, short turnRateAccel, short turnRateMin, short turnRateMax, short leanRate, short leanAngleMax)
{
	// TODO
}

static void HandlePlayerTurnX(ItemInfo& item, float alpha)
{
	constexpr auto BASE_ANGLE = ANGLE(90.0f);

	auto& player = GetLaraInfo(item);

	short headingAngle = GetPlayerHeadingAngleX(item);
	auto targetOrient = EulerAngles(headingAngle, item.Pose.Orientation.y, item.Pose.Orientation.z);

	short deltaAngle = Geometry::GetShortestAngle(item.Pose.Orientation.x, player.Control.RefCameraOrient.x);
	if (abs(deltaAngle) <= BASE_ANGLE)
	{
		item.Pose.Orientation.Lerp(targetOrient, alpha);
	}
	else
	{
		item.Pose.Orientation.InterpolateConstant(targetOrient, BASE_ANGLE * alpha);
	}

	player.Control.HeadingOrientTarget.x = headingAngle;
}

static void HandlePlayerTurnY(ItemInfo& item, float alpha, bool isStrafing, short yAngleOffset)
{
	constexpr auto BASE_ANGLE = ANGLE(90.0f);

	auto& player = GetLaraInfo(item);

	short headingAngle = (isStrafing ? player.Control.RefCameraOrient.y : (GetPlayerHeadingAngleY(item) + yAngleOffset));
	auto targetOrient = EulerAngles(item.Pose.Orientation.x, headingAngle, item.Pose.Orientation.z);

	short deltaAngle = Geometry::GetShortestAngle(item.Pose.Orientation.y, headingAngle);
	if (abs(deltaAngle) <= BASE_ANGLE)
	{
		item.Pose.Orientation.Lerp(targetOrient, alpha);
	}
	else
	{
		item.Pose.Orientation.InterpolateConstant(targetOrient, BASE_ANGLE * alpha);
	}

	player.Control.HeadingOrientTarget.y = headingAngle;
}

// Modern control version.
static void HandlePlayerTurnLean(ItemInfo& item, short leanAngleMax, float alpha, bool isStrafing)
{
	constexpr auto BASE_ANGLE		 = ANGLE(90.0f);
	constexpr auto STRAFE_LEAN_COEFF = 0.5f;

	auto& player = GetLaraInfo(item);

	// Calculate delta angle.
	short deltaAngle = Geometry::GetShortestAngle(item.Pose.Orientation.y, GetPlayerHeadingAngleY(item));
	int sign = std::copysign(1, deltaAngle);

	// Adjust detla angle if strafing.
	if (isStrafing)
	{
		if (abs(deltaAngle) > BASE_ANGLE)
			deltaAngle = ((BASE_ANGLE * 2) * sign) - deltaAngle;

		deltaAngle *= STRAFE_LEAN_COEFF;
	}

	// Calculate target lean orientation.
	float leanAngleAlpha = std::clamp(abs(deltaAngle) / (float)BASE_ANGLE, 0.0f, 1.0f);
	short targetLeanAngle = (leanAngleMax * leanAngleAlpha) * sign;
	auto targetOrient = EulerAngles(item.Pose.Orientation.x, item.Pose.Orientation.y, targetLeanAngle);

	// Lerp to target orientation.
	item.Pose.Orientation.Lerp(targetOrient, alpha);
}

// Modern control version.
static void HandlePlayerTurnFlex(ItemInfo& item, float alpha, bool isStrafing)
{
	constexpr auto BASE_ANGLE					   = ANGLE(90.0f);
	constexpr auto FLEX_ANGLE_Y_CONSTRAINT		   = ANGLE(60.0f);
	constexpr auto UPPER_FLEX_ANGLE_Y_STRAFE_COEFF = 0.9f;
	constexpr auto UPPER_FLEX_ANGLE_Y_ALPHA_COEFF  = 1.5f;
	constexpr auto UPPER_FLEX_ANGLE_Z_COEFF		   = 0.2f;
	constexpr auto TORSO_ROT_COEFF				   = 0.4f;
	constexpr auto HEAD_ROT_COEFF				   = 0.6f;

	auto& player = GetLaraInfo(item);

	// Calculate delta angle.
	short deltaAngle = Geometry::GetShortestAngle(item.Pose.Orientation.y, GetPlayerHeadingAngleY(item));
	int sign = std::copysign(1, deltaAngle);

	// TODO: Reference hip flex angles from TRL, which uses far more artistically cohrent offsets.
	// Adjust detla angle if strafing.
	if (isStrafing && abs(deltaAngle) > BASE_ANGLE)
		deltaAngle = (((BASE_ANGLE * 2) * sign) - deltaAngle) * -1;

	// Calculate angle modifiers.
	float upperFlexAngleYAlpha = std::clamp((abs(deltaAngle) / (float)FLEX_ANGLE_Y_CONSTRAINT) * UPPER_FLEX_ANGLE_Y_ALPHA_COEFF, 0.0f, 1.0f);
	float upperFlexAngleYCoeff = isStrafing ? UPPER_FLEX_ANGLE_Y_STRAFE_COEFF : 1.0f;

	// Calculate lower flex rotation.
	short lowerFlexAngleY = isStrafing ? std::clamp<short>(deltaAngle, -FLEX_ANGLE_Y_CONSTRAINT, FLEX_ANGLE_Y_CONSTRAINT) : 0;
	auto lowerFlexRot = EulerAngles(0, lowerFlexAngleY, 0);

	// Calculate upper flex rotation.
	short upperFlexAngleY = ((FLEX_ANGLE_Y_CONSTRAINT * upperFlexAngleYAlpha) * sign) * upperFlexAngleYCoeff;
	short upperFlexAngleZ = upperFlexAngleY * UPPER_FLEX_ANGLE_Z_COEFF;
	auto upperFlexRot = EulerAngles(player.ExtraHeadRot.x, upperFlexAngleY, upperFlexAngleZ);

	// Flex hips, torso, and head.
	player.LimbRot.Hip.Lerp(lowerFlexRot, alpha);
	player.ExtraTorsoRot.Lerp(upperFlexRot * TORSO_ROT_COEFF, alpha);
	player.ExtraHeadRot.Lerp(upperFlexRot * HEAD_ROT_COEFF, alpha);
}

// Modern control version.
static void HandlePlayerCrawlTurnFlex(ItemInfo& item, float alpha)
{
	constexpr auto FLEX_ANGLE_CONSTRAINT = ANGLE(40.0f);
	constexpr auto FLEX_Y_COEFF			 = 0.75f;
	constexpr auto TORSO_ROT_COEFF		 = 0.4f;
	constexpr auto HEAD_ROT_COEFF		 = 0.6f;

	auto& player = GetLaraInfo(item);

	// Calculate delta angle.
	short deltaAngle = Geometry::GetShortestAngle(item.Pose.Orientation.y, GetPlayerHeadingAngleY(item));
	int sign = std::copysign(1, deltaAngle);

	// Calculate target flex rotation.
	float flexAngleAlpha = std::clamp(abs(deltaAngle) / (float)FLEX_ANGLE_CONSTRAINT, 0.0f, 1.0f);
	short flexAngle = (FLEX_ANGLE_CONSTRAINT * flexAngleAlpha) * sign;
	auto flexRot = EulerAngles(player.ExtraHeadRot.x, player.ExtraHeadRot.y + (flexAngle * FLEX_Y_COEFF), flexAngle);

	// Flex head and torso.
	player.ExtraHeadRot.Lerp(flexRot * HEAD_ROT_COEFF, alpha);
	player.ExtraTorsoRot.Lerp(flexRot * TORSO_ROT_COEFF, alpha);
}

// NOTE: Modern control version.
void HandlePlayerSwimTurnFlex(ItemInfo& item, float alpha)
{
	constexpr auto FLEX_ANGLE_CONSTRAINT = ANGLE(40.0f);
	constexpr auto FLEX_Y_COEFF			 = 0.5f;
	constexpr auto TORSO_ROT_COEFF		 = 0.4f;
	constexpr auto HEAD_ROT_COEFF		 = 0.6f;

	auto& player = GetLaraInfo(item);

	// Calculate delta angles.
	short deltaAngleX = Geometry::GetShortestAngle(item.Pose.Orientation.x, GetPlayerHeadingAngleX(item));
	short deltaAngleZ = Geometry::GetShortestAngle(item.Pose.Orientation.y, GetPlayerHeadingAngleY(item));
	int signX = std::copysign(1, deltaAngleX);
	int signZ = std::copysign(1, deltaAngleZ);

	// Calculate flex angle alphas.
	float flexAngleXAlpha = std::clamp(abs(deltaAngleX) / (float)FLEX_ANGLE_CONSTRAINT, 0.0f, 1.0f);
	float flexAngleZAlpha = std::clamp(abs(deltaAngleZ) / (float)FLEX_ANGLE_CONSTRAINT, 0.0f, 1.0f);

	// Calculate target flex rotation.
	short flexAngleX = (FLEX_ANGLE_CONSTRAINT * flexAngleXAlpha) * signX;
	short flexAngleZ = (FLEX_ANGLE_CONSTRAINT * flexAngleZAlpha) * signZ;
	auto flexRot = EulerAngles(flexAngleX, player.ExtraHeadRot.y + (flexAngleZ * FLEX_Y_COEFF), flexAngleZ);

	// Flex head and torso.
	player.ExtraHeadRot.Lerp(flexRot * HEAD_ROT_COEFF, alpha);
	player.ExtraTorsoRot.Lerp(flexRot * TORSO_ROT_COEFF, alpha);
}

// Modern control version.
void HandlePlayerTurn(ItemInfo& item, float turnAlpha, short leanAngleMax, bool isStrafing, int flags, short yAngleOffset)
{
	// 1) X axis turn.
	if (flags & (int)PlayerTurnFlags::TurnX)
		HandlePlayerTurnX(item, turnAlpha);

	// 2) Y axis turn.
	if (flags & (int)PlayerTurnFlags::TurnY)
		HandlePlayerTurnY(item, turnAlpha, isStrafing, yAngleOffset);

	// 3) Flex.
	if (flags & (int)PlayerTurnFlags::VerticalFlex)
	{
		HandlePlayerTurnFlex(item, turnAlpha, isStrafing);
	}
	else if (flags & (int)PlayerTurnFlags::CrawlFlex)
	{
		HandlePlayerCrawlTurnFlex(item, turnAlpha);
	}
	else if (flags & (int)PlayerTurnFlags::SwimFlex)
	{
		HandlePlayerSwimTurnFlex(item, turnAlpha);
	}

	// 4) Lean.
	HandlePlayerTurnLean(item, leanAngleMax, turnAlpha, isStrafing);
}

void HandlePlayerUpJumpShift(ItemInfo& item)
{
	constexpr auto BASE_ANGLE = ANGLE(90.0f);
	constexpr auto VEL_ACCEL  = 2.0f;
	constexpr auto VEL_MAX	  = 5.0f;

	enum class ShiftType
	{
		ForwardPassive,
		ForwardActive,
		BackwardPassive,
		BackwardActive,
	};

	auto& player = GetLaraInfo(item);

	// Determine shift type.
	auto shiftType = ShiftType::ForwardPassive;
	if (g_Configuration.IsUsingModernControls())
	{
		if (GetMoveAxis() != Vector2::Zero)
		{
			short relMoveAngle = GetPlayerRelHeadingAngleY(item);
			if (abs(relMoveAngle) <= BASE_ANGLE)
			{
				ShiftType::ForwardActive;
			}
			else
			{
				ShiftType::BackwardActive;
			}
		}
		else
		{
			if (item.Animation.Velocity.z < 0.0f)
				shiftType = ShiftType::BackwardPassive;
		}
	}
	else
	{
		if (IsHeld(In::Forward))
		{
			shiftType = ShiftType::ForwardActive;
		}
		else if (IsHeld(In::Back))
		{
			shiftType = ShiftType::BackwardActive;
		}
		else
		{
			if (item.Animation.Velocity.z < 0.0f)
				shiftType = ShiftType::BackwardPassive;
		}
	}

	// Modulate Z velocity.
	switch (shiftType)
	{
	default:
	case ShiftType::ForwardPassive:
		item.Animation.Velocity.z = VEL_ACCEL;
		break;
		
	case ShiftType::ForwardActive:
		item.Animation.Velocity.z += VEL_ACCEL;
		if (item.Animation.Velocity.z > VEL_MAX)
			item.Animation.Velocity.z = VEL_MAX;

		break;

	case ShiftType::BackwardPassive:
		item.Animation.Velocity.z = -VEL_ACCEL;
		break;

	case ShiftType::BackwardActive:
		item.Animation.Velocity.z -= VEL_ACCEL;
		if (item.Animation.Velocity.z < -VEL_MAX)
			item.Animation.Velocity.z = -VEL_MAX;

		break;
	}

	// Flex if moving backward.
	if (item.Animation.Velocity.z < 0.0f)
	{
		// TODO: Holding Back + Left/Right results in player flexing more.
		item.Pose.Orientation.x += std::min<short>(LARA_LEAN_RATE / 3, abs(ANGLE(item.Animation.Velocity.z) - item.Pose.Orientation.x) / 3);
		player.ExtraHeadRot.y += (ANGLE(10.0f) - item.Pose.Orientation.z) / 3;
	}
}

static void GivePlayerItemsCheat(ItemInfo& item)
{
	auto& player = GetLaraInfo(item);

	for (int i = 0; i < 8; ++i)
	{
		if (Objects[ID_PUZZLE_ITEM1 + i].loaded)
			player.Inventory.Puzzles[i] = true;

		player.Inventory.PuzzlesCombo[2 * i] = false;
		player.Inventory.PuzzlesCombo[(2 * i) + 1] = false;
	}

	for (int i = 0; i < 8; ++i)
	{
		if (Objects[ID_KEY_ITEM1 + i].loaded)
			player.Inventory.Keys[i] = true;

		player.Inventory.KeysCombo[2 * i] = false;
		player.Inventory.KeysCombo[(2 * i) + 1] = false;
	}

	for (int i = 0; i < 3; ++i)
	{
		if (Objects[ID_PICKUP_ITEM1 + i].loaded)
			player.Inventory.Pickups[i] = true;

		player.Inventory.PickupsCombo[2 * i] = false;
		player.Inventory.PickupsCombo[(2 * i) + 1] = false;
	}
}

static void GivePlayerWeaponsCheat(ItemInfo& item)
{
	auto& player = GetLaraInfo(item);

	player.Inventory.TotalFlares = -1;
	player.Inventory.TotalSmallMedipacks = -1;
	player.Inventory.TotalLargeMedipacks = -1;

	if (Objects[ID_CROWBAR_ITEM].loaded)
		player.Inventory.HasCrowbar = true;

	if (Objects[ID_LASERSIGHT_ITEM].loaded)
		player.Inventory.HasLasersight = true;

	if (Objects[ID_CLOCKWORK_BEETLE].loaded)
		player.Inventory.BeetleComponents |= BEETLECOMP_FLAG_BEETLE;

	if (Objects[ID_WATERSKIN1_EMPTY].loaded)
		player.Inventory.SmallWaterskin = 1;

	if (Objects[ID_WATERSKIN2_EMPTY].loaded)
		player.Inventory.BigWaterskin = 1;

	if (Objects[ID_PISTOLS_ITEM].loaded)
	{
		auto& weapon = player.Weapons[(int)LaraWeaponType::Pistol];

		weapon.Present = true;
		weapon.SelectedAmmo = WeaponAmmoType::Ammo1;
		weapon.HasLasersight = false;
		weapon.HasSilencer = false;
		weapon.Ammo[(int)WeaponAmmoType::Ammo1].SetInfinite(true);
	}

	if (Objects[ID_REVOLVER_ITEM].loaded)
	{
		auto& weapon = player.Weapons[(int)LaraWeaponType::Revolver];

		weapon.Present = true;
		weapon.SelectedAmmo = WeaponAmmoType::Ammo1;
		weapon.HasLasersight = false;
		weapon.HasSilencer = false;
		weapon.Ammo[(int)WeaponAmmoType::Ammo1].SetInfinite(true);
	}

	if (Objects[ID_UZI_ITEM].loaded)
	{
		auto& weapon = player.Weapons[(int)LaraWeaponType::Uzi];

		weapon.Present = true;
		weapon.SelectedAmmo = WeaponAmmoType::Ammo1;
		weapon.HasLasersight = false;
		weapon.HasSilencer = false;
		weapon.Ammo[(int)WeaponAmmoType::Ammo1].SetInfinite(true);
	}

	if (Objects[ID_SHOTGUN_ITEM].loaded)
	{
		auto& weapon = player.Weapons[(int)LaraWeaponType::Shotgun];

		weapon.Present = true;
		weapon.SelectedAmmo = WeaponAmmoType::Ammo1;
		weapon.HasLasersight = false;
		weapon.HasSilencer = false;
		weapon.Ammo[(int)WeaponAmmoType::Ammo1].SetInfinite(true);
		weapon.Ammo[(int)WeaponAmmoType::Ammo2].SetInfinite(true);
	}

	if (Objects[ID_HARPOON_ITEM].loaded)
	{
		auto& weapon = player.Weapons[(int)LaraWeaponType::HarpoonGun];

		weapon.Present = true;
		weapon.SelectedAmmo = WeaponAmmoType::Ammo1;
		weapon.HasLasersight = false;
		weapon.HasSilencer = false;
		weapon.Ammo[(int)WeaponAmmoType::Ammo1].SetInfinite(true);
	}

	if (Objects[ID_GRENADE_GUN_ITEM].loaded)
	{
		auto& weapon = player.Weapons[(int)LaraWeaponType::GrenadeLauncher];

		weapon.Present = true;
		weapon.SelectedAmmo = WeaponAmmoType::Ammo1;
		weapon.HasSilencer = false;
		weapon.Ammo[(int)WeaponAmmoType::Ammo1].SetInfinite(true);
		weapon.Ammo[(int)WeaponAmmoType::Ammo2].SetInfinite(true);
		weapon.Ammo[(int)WeaponAmmoType::Ammo3].SetInfinite(true);
	}

	if (Objects[ID_ROCKET_LAUNCHER_ITEM].loaded)
	{
		auto& weapon = player.Weapons[(int)LaraWeaponType::RocketLauncher];

		weapon.Present = true;
		weapon.SelectedAmmo = WeaponAmmoType::Ammo1;
		weapon.HasLasersight = false;
		weapon.HasSilencer = false;
		weapon.Ammo[(int)WeaponAmmoType::Ammo1].SetInfinite(true);
	}

	if (Objects[ID_HK_ITEM].loaded)
	{
		auto& weapon = player.Weapons[(int)LaraWeaponType::HK];

		weapon.Present = true;
		weapon.SelectedAmmo = WeaponAmmoType::Ammo1;
		weapon.WeaponMode = LaraWeaponTypeCarried::WTYPE_AMMO_1;
		weapon.HasLasersight = false;
		weapon.HasSilencer = false;
		weapon.Ammo[(int)WeaponAmmoType::Ammo1].SetInfinite(true);
	}

	if (Objects[ID_CROSSBOW_ITEM].loaded)
	{
		auto& weapon = player.Weapons[(int)LaraWeaponType::Crossbow];

		weapon.Present = true;
		weapon.SelectedAmmo = WeaponAmmoType::Ammo1;
		weapon.HasLasersight = false;
		weapon.HasSilencer = false;
		weapon.Ammo[(int)WeaponAmmoType::Ammo1].SetInfinite(true);
		weapon.Ammo[(int)WeaponAmmoType::Ammo2].SetInfinite(true);
		weapon.Ammo[(int)WeaponAmmoType::Ammo3].SetInfinite(true);
	}
}

void HandlePlayerFlyCheat(ItemInfo& item)
{
	auto& player = GetLaraInfo(item);

	if (!g_GameFlow->IsFlyCheatEnabled())
		return;

	static bool dbFlyCheat = true;
	if (KeyMap[OIS::KeyCode::KC_O] && dbFlyCheat)
	{
		if (player.Context.Vehicle == NO_VALUE)
		{
			if (KeyMap[OIS::KeyCode::KC_LSHIFT] || KeyMap[OIS::KeyCode::KC_RSHIFT])
				GivePlayerItemsCheat(item);

			GivePlayerWeaponsCheat(item);

			if (player.Control.WaterStatus != WaterStatus::FlyCheat)
			{
				SetAnimation(item, LA_FLY_CHEAT);
				ResetPlayerFlex(&item);
				item.Animation.Velocity = Vector3::Zero;
				item.Animation.IsAirborne = true;
				item.Pose.Position.y -= CLICK(0.5f);
				item.Pose.Scale = Vector3::One;
				item.HitPoints = LARA_HEALTH_MAX;

				player.Control.WaterStatus = WaterStatus::FlyCheat;
				player.Control.Count.Death = 0;
				player.Control.ToggleClimb = false;
				player.Control.ToggleCrouch = false;
				player.Control.ToggleWalk = false;
				player.Status.Air = LARA_AIR_MAX;
				player.Status.Poison = 0;
				player.Status.Stamina = LARA_STAMINA_MAX;
			}
		}
		else
		{
			SayNo();
		}
	}
	dbFlyCheat = !KeyMap[OIS::KeyCode::KC_O];
}

void HandlePlayerExtraAnim(ItemInfo& item)
{
	if (item.Animation.AnimObjectID != GAME_OBJECT_ID::ID_LARA_EXTRA_ANIMS)
		return;

	auto& anim = GetAnimData(item);
	if (anim.NextAnimNumber != 0 || anim.NextFrameNumber != 0)
		return;

	if (TestLastFrame(item))
		SetAnimation(item, LA_STAND_SOLID);
}

void HandlePlayerWetnessDrips(ItemInfo& item)
{
	auto& player = *GetLaraInfo(&item);

	if (Wibble & 0xF)
		return;

	int jointIndex = 0;
	for (auto& node : player.Effect.DripNodes)
	{
		auto pos = GetJointPosition(&item, jointIndex);
		int roomNumber = GetRoomVector(item.Location, pos).RoomNumber;
		jointIndex++;

		// Node underwater; set max wetness value.
		if (TestEnvironment(ENV_FLAG_WATER, roomNumber))
		{
			node = PLAYER_DRIP_NODE_MAX;
			continue;
		}

		// Node inactive; continue.
		if (node <= 0.0f)
			continue;

		// Spawn drip.
		float chance = (node / PLAYER_DRIP_NODE_MAX) / 2;
		if (Random::TestProbability(chance))
		{
			SpawnWetnessDrip(pos.ToVector3(), item.RoomNumber);

			node -= 1.0f;
			if (node <= 0.0f)
				node = 0.0f;
		}
	}
}

void HandlePlayerDiveBubbles(ItemInfo& item)
{
	constexpr auto BUBBLE_COUNT_MULT = 3;

	auto& player = *GetLaraInfo(&item);

	int jointIndex = 0;
	for (auto& node : player.Effect.BubbleNodes)
	{
		auto pos = GetJointPosition(&item, jointIndex);
		int roomNumber = GetRoomVector(item.Location, pos).RoomNumber;
		jointIndex++;

		// Node inactive; continue.
		if (node <= 0.0f)
			continue;

		// Node above water; continue.
		if (!TestEnvironment(ENV_FLAG_WATER, roomNumber))
			continue;

		// Spawn bubbles.
		float chance = node / PLAYER_BUBBLE_NODE_MAX;
		if (Random::TestProbability(chance))
		{
			unsigned int count = (int)round(node * BUBBLE_COUNT_MULT);
			SpawnDiveBubbles(pos.ToVector3(), roomNumber, count);

			node -= 1.0f;
			if (node <= 0.0f)
				node = 0.0f;
		}
	}
}

void HandlePlayerAirBubbles(ItemInfo* item)
{
	constexpr auto BUBBLE_COUNT_MAX = 3;

	SoundEffect(SFX_TR4_LARA_BUBBLES, &item->Pose, SoundEnvironment::Underwater);

	const auto& level = *g_GameFlow->GetLevel(CurrentLevel);

	auto pos = (level.GetLaraType() == LaraType::Divesuit) ?
		GetJointPosition(item, LM_TORSO, Vector3i(0, -192, -160)).ToVector3() :
		GetJointPosition(item, LM_HEAD, Vector3i(0, -4, -64)).ToVector3();

	unsigned int bubbleCount = Random::GenerateInt(0, BUBBLE_COUNT_MAX);
	for (int i = 0; i < bubbleCount; i++)
		SpawnBubble(pos, item->RoomNumber);
}

// TODO: This approach may present undesirable artefacts where a platform rapidly ascends/descends or the player gets pushed.
// Potential solutions:
// 1. Consider floor tilt when translating objects.
// 2. Object parenting. -- Sezz 2022.10.28
void EasePlayerElevation(ItemInfo* item, int relHeight)
{
	constexpr auto LINEAR_RATE_MIN = 50.0f;

	// Check for wall.
	if (relHeight == NO_HEIGHT)
		return;

	// Handle swamp case.
	if (TestEnvironment(ENV_FLAG_SWAMP, item) && relHeight > 0)
	{
		item->Pose.Position.y += g_GameFlow->GetSettings()->Physics.Gravity / SWAMP_GRAVITY_COEFF;
		return;
	}

	// Handle regular case.
	float linearRate = std::max(LINEAR_RATE_MIN, abs(item->Animation.Velocity.z));
	if (abs(relHeight) > linearRate)
	{
		int sign = std::copysign(1, relHeight);
		item->Pose.Position.y += (int)round(linearRate * sign);
		return;
	}

	// Snap elevation.
	item->Pose.Position.y += relHeight;
}

// TODO: Some states can't make the most of this function due to missing step up/down animations.
// Try implementing leg IK as a substitute to make step animations obsolete. @Sezz 2021.10.09
void HandlePlayerElevationChange(ItemInfo* item, CollisionInfo* coll)
{
	if (!TestEnvironment(ENV_FLAG_SWAMP, item))
	{
		if (CanStepUp(*item, *coll))
		{
			item->DisableInterpolation = true;

			const auto* dispatch = GetStateDispatch(*item, LS_STEP_UP);
			if (dispatch != nullptr)
			{
				SetStateDispatch(*item, *dispatch);
				item->Pose.Position.y += coll->Middle.Floor;
				return;
			}
		}
		else if (CanStepDown(*item, *coll))
		{
			const auto* dispatch = GetStateDispatch(*item, LS_STEP_DOWN);
			if (dispatch != nullptr)
			{
				SetStateDispatch(*item, *dispatch);
				item->Pose.Position.y += coll->Middle.Floor;
				return;
			}
		}
	}

	EasePlayerElevation(item, coll->Middle.Floor);
}

void DoLaraMonkeyStep(ItemInfo* item, CollisionInfo* coll)
{
	EasePlayerElevation(item, coll->Middle.Ceiling);
}

void DoLaraCrawlToHangSnap(ItemInfo* item, CollisionInfo* coll)
{
	coll->Setup.ForwardAngle = item->Pose.Orientation.y + ANGLE(180.0f);
	GetCollisionInfo(coll, item);

	SnapItemToLedge(item, coll);
	LaraResetGravityStatus(item, coll);

	// Bridges behave differently.
	if (coll->Middle.Bridge < 0)
	{
		item->Pose.Translate(item->Pose.Orientation.y, -LARA_RADIUS_CRAWL);
		item->Pose.Orientation.y += ANGLE(180.0f);
	}
}

void DoLaraTightropeBalance(ItemInfo* item)
{
	auto* lara = GetLaraInfo(item);
	const int factor = ((lara->Control.Tightrope.TimeOnTightrope >> 7) & 0xFF) * 128;

	if (IsHeld(In::Left))
		lara->Control.Tightrope.Balance += ANGLE(1.4f);
	if (IsHeld(In::Right))
		lara->Control.Tightrope.Balance -= ANGLE(1.4f);

	if (lara->Control.Tightrope.Balance < 0)
	{
		lara->Control.Tightrope.Balance -= factor;
		if (lara->Control.Tightrope.Balance <= -ANGLE(45.0f))
			lara->Control.Tightrope.Balance = -ANGLE(45.0f);

	}
	else if (lara->Control.Tightrope.Balance > 0)
	{
		lara->Control.Tightrope.Balance += factor;
		if (lara->Control.Tightrope.Balance >= ANGLE(45.0f))
			lara->Control.Tightrope.Balance = ANGLE(45.0f);
	}
	else
		lara->Control.Tightrope.Balance = GetRandomControl() & 1 ? -1 : 1;
}

void DoLaraTightropeLean(ItemInfo* item)
{
	auto* lara = GetLaraInfo(item);

	item->Pose.Orientation.z = lara->Control.Tightrope.Balance / 4;
	lara->ExtraTorsoRot.z = -lara->Control.Tightrope.Balance;
}

void DoLaraTightropeBalanceRegen(ItemInfo* item)
{
	auto* lara = GetLaraInfo(item);

	if (lara->Control.Tightrope.TimeOnTightrope <= 32)
		lara->Control.Tightrope.TimeOnTightrope = 0;
	else
		lara->Control.Tightrope.TimeOnTightrope -= 32;

	if (lara->Control.Tightrope.Balance > 0)
	{
		if (lara->Control.Tightrope.Balance <= ANGLE(0.75f))
			lara->Control.Tightrope.Balance = 0;
		else
			lara->Control.Tightrope.Balance -= ANGLE(0.75f);
	}

	if (lara->Control.Tightrope.Balance < 0)
	{
		if (lara->Control.Tightrope.Balance >= -ANGLE(0.75f))
			lara->Control.Tightrope.Balance = 0;
		else
			lara->Control.Tightrope.Balance += ANGLE(0.75f);
	}
}

void DoLaraFallDamage(ItemInfo* item)
{
	constexpr auto RUMBLE_POWER_COEFF = 0.7f;
	constexpr auto RUMBLE_DELAY		  = 0.3f;

	if (item->Animation.Velocity.y < LARA_DAMAGE_VELOCITY)
		return;

	auto pointColl = GetPointCollision(*item);

	if ((pointColl.TestEnvironmentFlag(RoomEnvFlags::ENV_FLAG_WATER) || pointColl.TestEnvironmentFlag(RoomEnvFlags::ENV_FLAG_SWAMP)) &&
		pointColl.GetWaterBottomHeight() >= CLICK(2))
		return;

	if (item->Animation.Velocity.y >= LARA_DEATH_VELOCITY)
	{
		item->HitPoints = 0;

		// HACK: Process floor death flag here to drown player in rapids after freefall.
		item->Pose.Position.y = item->Floor;
		ProcessSectorFlags(item);
	}
	else
	{
		float base = item->Animation.Velocity.y - (LARA_DAMAGE_VELOCITY - 1.0f);
		DoDamage(item, LARA_HEALTH_MAX * (SQUARE(base) / 196.0f), true);
	}

	float rumblePower = (item->Animation.Velocity.y / LARA_DEATH_VELOCITY) * RUMBLE_POWER_COEFF;
	Rumble(rumblePower, RUMBLE_DELAY);
}

LaraInfo& GetLaraInfo(ItemInfo& item)
{
	if (item.ObjectNumber == ID_LARA)
	{
		auto* player = (LaraInfo*&)item.Data;
		return *player;
	}

	TENLog(fmt::format("Attempted to fetch LaraInfo data from {} moveable.", GetObjectName(item.ObjectNumber)), LogLevel::Warning);

	auto& firstLaraItem = *FindItem(ID_LARA);
	auto* player = (LaraInfo*&)firstLaraItem.Data;
	return *player;
}

const LaraInfo& GetLaraInfo(const ItemInfo& item)
{
	if (item.ObjectNumber == ID_LARA)
	{
		const auto* player = (LaraInfo*&)item.Data;
		return *player;
	}

	TENLog(fmt::format("Attempted to fetch LaraInfo data from {} moveable..", GetObjectName(item.ObjectNumber)), LogLevel::Warning);

	const auto& firstPlayerItem = *FindItem(ID_LARA);
	const auto* player = (LaraInfo*&)firstPlayerItem.Data;
	return *player;
}

LaraInfo*& GetLaraInfo(ItemInfo* item)
{
	if (item->ObjectNumber == ID_LARA)
		return (LaraInfo*&)item->Data;

	TENLog(fmt::format("Attempted to fetch LaraInfo data from {} moveable.", GetObjectName(item->ObjectNumber)), LogLevel::Warning);

	auto& firstPlayerItem = *FindItem(ID_LARA);
	return (LaraInfo*&)firstPlayerItem.Data;
}

PlayerWaterData GetPlayerWaterData(ItemInfo& item)
{
	auto pointColl = GetPointCollision(item);
	int heightFromWater = (pointColl.GetWaterTopHeight() == NO_HEIGHT) ?
		NO_HEIGHT : (std::min(item.Pose.Position.y, pointColl.GetFloorHeight()) - pointColl.GetWaterTopHeight());

	return PlayerWaterData
	{
		TestEnvironment(ENV_FLAG_WATER, &item), TestEnvironment(ENV_FLAG_SWAMP, &item), TestEnvironment(ENV_FLAG_COLD, &item),
		pointColl.GetWaterBottomHeight(), pointColl.GetWaterTopHeight(), heightFromWater
	};
}

JumpDirection GetPlayerJumpDirection(const ItemInfo& item, const CollisionInfo& coll)
{
	const auto& player = GetLaraInfo(item);

	if (g_Configuration.IsUsingModernControls())
	{
		if (IsPlayerStrafing(item) || IsHeld(In::Walk))
		{
			// TODO: Up case.
			short deltaAngle = GetPlayerRelHeadingAngleY(item);
			if (abs(deltaAngle) <= ANGLE(45.0f) && CanJumpForward(item, coll))
			{
				return JumpDirection::Forward;
			}
			else if (abs(deltaAngle + ANGLE(180.0f)) <= ANGLE(45.0f) && CanJumpBackward(item, coll))
			{
				return JumpDirection::Back;
			}
			else if (abs(deltaAngle + ANGLE(90.0f)) <= ANGLE(45.0f) && CanJumpLeft(item, coll))
			{
				return JumpDirection::Left;
			}
			else if (abs(deltaAngle - ANGLE(90.0f)) <= ANGLE(45.0f) && CanJumpRight(item, coll))
			{
				return JumpDirection::Right;
			}
			else if (CanJumpUp(item, coll))
			{
				return JumpDirection::Up;
			}
		}
		else
		{
			if ((IsHeld(In::Forward) || IsHeld(In::Back) ||
				IsHeld(In::Left) || IsHeld(In::Right)) &&
				CanJumpForward(item, coll))
			{
				return JumpDirection::Forward;
			}
			else if (CanJumpUp(item, coll))
			{
				return JumpDirection::Up;
			}
		}
	}
	else
	{
		if (IsHeld(In::Forward) && CanJumpForward(item, coll))
		{
			return JumpDirection::Forward;
		}
		else if (IsHeld(In::Back) && CanJumpBackward(item, coll))
		{
			return JumpDirection::Back;
		}
		else if (IsHeld(In::Left) && CanJumpLeft(item, coll))
		{
			return JumpDirection::Left;
		}
		else if (IsHeld(In::Right) && CanJumpRight(item, coll))
		{
			return JumpDirection::Right;
		}
		else if (CanJumpUp(item, coll))
		{
			return JumpDirection::Up;
		}
	}

	return JumpDirection::None;
}

int GetPlayerStrafeTurnStateID(const ItemInfo& item)
{
	constexpr auto DELTA_ANGLE_CONSTRAINT = ANGLE(2.5f);

	const auto& player = GetLaraInfo(item);

	short deltaAngle = Geometry::GetShortestAngle(player.Control.HeadingOrientTarget.y, item.Pose.Orientation.y);
	if (deltaAngle >= DELTA_ANGLE_CONSTRAINT)
	{
		return LS_TURN_LEFT_SLOW;
	}
	else if (deltaAngle <= -DELTA_ANGLE_CONSTRAINT)
	{
		return LS_TURN_RIGHT_SLOW;
	}

	return NO_VALUE;
}

short GetPlayerHeadingAngleX(const ItemInfo& item)
{
	const auto& player = GetLaraInfo(item);

	if (g_Configuration.IsUsingModernControls())
	{
		if (GetMoveAxis() == Vector2::Zero)
			return player.Control.HeadingOrientTarget.x;

		return player.Control.RefCameraOrient.x;
	}
	else
	{
		return item.Pose.Orientation.x;
	}
}

short GetPlayerHeadingAngleY(const ItemInfo& item)
{
	const auto& player = GetLaraInfo(item);

	if (g_Configuration.IsUsingModernControls())
	{
		// Player is strafing and horizontal velocity is 0; return reference camera azimuth angle.
		float vel = Vector2(item.Animation.Velocity.x, item.Animation.Velocity.z).Length();
		if (IsPlayerStrafing(item) && vel == 0.0f)
			return player.Control.RefCameraOrient.y;

		// Calculate move axis angle.
		auto dir = player.Control.RefMoveAxis;
		dir.Normalize();
		short moveAxisAngle = FROM_RAD(atan2(dir.x, dir.y));

		// TEMP HACK
		// TODO: No hardcoding. Must pass some kind of argument to this function while keeping calls clean.
		if (IsPlayerStrafing(item) &&
			(item.Animation.ActiveState == LS_RUN_FORWARD ||
			item.Animation.ActiveState == LS_WALK_FORWARD))
		{
			if (abs(moveAxisAngle) > ANGLE(90.0f))
				moveAxisAngle = 0;
		}
		else if (item.Animation.ActiveState == LS_SKIP_BACK ||
			item.Animation.ActiveState == LS_WALK_BACK)
		{
			if (abs(moveAxisAngle) < ANGLE(90.0f))
				moveAxisAngle = ANGLE(180.0f);
		}

		/*if (moveAxisAngle < moveAxisAngleConstraint.first &&
			moveAxisAngle > moveAxisAngleConstraint.second)
		{
			moveAxisAngle = defaultMoveAxisAngle;
		}*/

		return (player.Control.RefCameraOrient.y + moveAxisAngle);
	}
	else
	{
		return item.Pose.Orientation.y;
	}
}

short GetPlayerRelHeadingAngleY(const ItemInfo& item)
{
	const auto& player = GetLaraInfo(item);

	auto dir = player.Control.RefMoveAxis;
	dir.Normalize();
	short headingAngle = player.Control.RefCameraOrient.y + FROM_RAD(atan2(dir.x, dir.y));

	return Geometry::GetShortestAngle(headingAngle, item.Pose.Orientation.y);
}

static short GetLegacySlideHeadingAngle(const Vector3& floorNormal)
{
	auto tilt = GetSurfaceTilt(floorNormal, true);

	short headingAngle = ANGLE(0.0f);
	if (tilt.x > 2)
	{
		headingAngle = ANGLE(-90.0f);
	}
	else if (tilt.x < -2)
	{
		headingAngle = ANGLE(90.0f);
	}

	if (tilt.y > 2 && tilt.y > abs(tilt.x))
	{
		headingAngle = ANGLE(180.0f);
	}
	else if (tilt.y < -2 && -tilt.y > abs(tilt.x))
	{
		headingAngle = ANGLE(0.0f);
	}

	return headingAngle;
}

short GetPlayerSlideHeadingAngle(ItemInfo* item, CollisionInfo* coll)
{
	short headingAngle = coll->Setup.ForwardAngle;
	auto pointColl = GetPointCollision(*item);

	// Ground is flat.
	if (pointColl.GetFloorNormal() == -Vector3::UnitY)
		return coll->Setup.ForwardAngle;

	// Return slide heading angle.
	if (g_GameFlow->GetSettings()->Animations.SlideExtended)
	{
		return Geometry::GetSurfaceAspectAngle(pointColl.GetFloorNormal());
	}
	else
	{
		return GetLegacySlideHeadingAngle(pointColl.GetFloorNormal());
	}
}

short ModulateLaraTurnRate(short turnRate, short accelRate, short minTurnRate, short maxTurnRate, float axisCoeff, bool invert)
{
	// Determine sign.
	axisCoeff *= invert ? -1 : 1;
	int sign = std::copysign(1, axisCoeff);

	// Normalize min and max turn rates according to axis coefficient.
	short minTurnRateNorm = minTurnRate * abs(axisCoeff);
	short maxTurnRateNorm = maxTurnRate * abs(axisCoeff);

	// Calculate and return new turn rate.
	short newTurnRate = (turnRate + (accelRate * sign)) * sign;
	newTurnRate = std::clamp(newTurnRate, minTurnRateNorm, maxTurnRateNorm);
	return (newTurnRate * sign);
}

void ModulateLaraTurnRateX(ItemInfo* item, short accelRate, short minTurnRate, short maxTurnRate, bool invert)
{
	auto& player = GetLaraInfo(*item);
	player.Control.TurnRate.x = ModulateLaraTurnRate(player.Control.TurnRate.x, accelRate, minTurnRate, maxTurnRate, GetMoveAxis().y, invert);
}

void ModulateLaraTurnRateY(ItemInfo* item, short accelRate, short minTurnRate, short maxTurnRate, bool invert)
{
	constexpr auto AIRBORNE_AXIS_COEFF_MIN = 1.2f;

	auto& player = GetLaraInfo(*item);

	float axisCoeff = GetMoveAxis().x;
	if (item->Animation.IsAirborne)
	{
		int sign = std::copysign(1, axisCoeff);
		axisCoeff = std::min(AIRBORNE_AXIS_COEFF_MIN, abs(axisCoeff)) * sign;
	}

	player.Control.TurnRate.y = ModulateLaraTurnRate(player.Control.TurnRate.y, accelRate, minTurnRate, maxTurnRate, axisCoeff, invert);
}

static short ResetPlayerTurnRate(short turnRate, short decelRate)
{
	int sign = std::copysign(1, turnRate);
	if (abs(turnRate) > decelRate)
		return (turnRate - (decelRate * sign));

	return 0;
}

void ResetPlayerTurnRateX(ItemInfo& item, short decelRate)
{
	auto& player = GetLaraInfo(item);
	player.Control.TurnRate.x = ResetPlayerTurnRate(player.Control.TurnRate.x, decelRate);
}

void ResetPlayerTurnRateY(ItemInfo& item, short decelRate)
{
	auto& player = GetLaraInfo(item);
	player.Control.TurnRate.y = ResetPlayerTurnRate(player.Control.TurnRate.y, decelRate);
}

void ModulateLaraSwimTurnRates(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	/*if ((IsHeld(In::Forward) || IsHeld(In::Back)))
	ModulateLaraTurnRateX(item, 0, 0, 0);*/

	if (IsHeld(In::Forward))
		item->Pose.Orientation.x -= ANGLE(3.0f);
	else if (IsHeld(In::Back))
		item->Pose.Orientation.x += ANGLE(3.0f);

	if (IsHeld(In::Left) || IsHeld(In::Right))
	{
		ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL, 0, LARA_MED_TURN_RATE_MAX);

		// TODO: ModulateLaraLean() doesn't really work here. -- Sezz 2022.06.22
		if (IsHeld(In::Left))
			item->Pose.Orientation.z -= LARA_LEAN_RATE;
		else if (IsHeld(In::Right))
			item->Pose.Orientation.z += LARA_LEAN_RATE;
	}
}

void ModulateLaraSubsuitSwimTurnRates(ItemInfo* item)
{
	auto* lara = GetLaraInfo(item);

	if (IsHeld(In::Forward) && item->Pose.Orientation.x > -ANGLE(85.0f))
		lara->Control.Subsuit.DXRot = -ANGLE(45.0f);
	else if (IsHeld(In::Back) && item->Pose.Orientation.x < ANGLE(85.0f))
		lara->Control.Subsuit.DXRot = ANGLE(45.0f);
	else
		lara->Control.Subsuit.DXRot = 0;

	if (IsHeld(In::Left) || IsHeld(In::Right))
	{
		ModulateLaraTurnRateY(item, LARA_SUBSUIT_TURN_RATE_ACCEL, 0, LARA_MED_TURN_RATE_MAX);

		if (IsHeld(In::Left))
			item->Pose.Orientation.z -= LARA_LEAN_RATE;
		else if (IsHeld(In::Right))
			item->Pose.Orientation.z += LARA_LEAN_RATE;
	}
}

// TODO: Simplify this function. Some members of SubsuitControlData may be unnecessary. -- Sezz 2022.06.22
void UpdateLaraSubsuitAngles(ItemInfo* item)
{
	auto* lara = GetLaraInfo(item);

	if (lara->Control.Subsuit.VerticalVelocity != 0)
	{
		item->Pose.Position.y += lara->Control.Subsuit.VerticalVelocity / 4;
		lara->Control.Subsuit.VerticalVelocity = ceil((15 / 16) * lara->Control.Subsuit.VerticalVelocity - 1);
	}

	lara->Control.Subsuit.Velocity[0] = -4 * item->Animation.Velocity.y;
	lara->Control.Subsuit.Velocity[1] = -4 * item->Animation.Velocity.y;

	if (lara->Control.Subsuit.XRot >= lara->Control.Subsuit.DXRot)
	{
		if (lara->Control.Subsuit.XRot > lara->Control.Subsuit.DXRot)
		{
			if (lara->Control.Subsuit.XRot > 0 && lara->Control.Subsuit.DXRot < 0)
				lara->Control.Subsuit.XRot = ceil(0.75 * lara->Control.Subsuit.XRot);

			lara->Control.Subsuit.XRot -= ANGLE(2.0f);
			if (lara->Control.Subsuit.XRot < lara->Control.Subsuit.DXRot)
				lara->Control.Subsuit.XRot = lara->Control.Subsuit.DXRot;
		}
	}
	else
	{
		if (lara->Control.Subsuit.XRot < 0 && lara->Control.Subsuit.DXRot > 0)
			lara->Control.Subsuit.XRot = ceil(0.75 * lara->Control.Subsuit.XRot);

		lara->Control.Subsuit.XRot += ANGLE(2.0f);
		if (lara->Control.Subsuit.XRot > lara->Control.Subsuit.DXRot)
			lara->Control.Subsuit.XRot = lara->Control.Subsuit.DXRot;
	}

	if (lara->Control.Subsuit.DXRot != 0)
	{
		short rotation = lara->Control.Subsuit.DXRot >> 3;
		if (rotation < -ANGLE(2.0f))
			rotation = -ANGLE(2.0f);
		else if (rotation > ANGLE(2.0f))
			rotation = ANGLE(2.0f);

		item->Pose.Orientation.x += rotation;
	}

	lara->Control.Subsuit.Velocity[0] += abs(lara->Control.Subsuit.XRot >> 3);
	lara->Control.Subsuit.Velocity[1] += abs(lara->Control.Subsuit.XRot >> 3);

	if (lara->Control.TurnRate.y > 0)
		lara->Control.Subsuit.Velocity[0] += 2 * abs(lara->Control.TurnRate.y);
	else if (lara->Control.TurnRate.y < 0)
		lara->Control.Subsuit.Velocity[1] += 2 * abs(lara->Control.TurnRate.y);

	if (lara->Control.Subsuit.Velocity[0] > BLOCK(1.5f))
		lara->Control.Subsuit.Velocity[0] = BLOCK(1.5f);

	if (lara->Control.Subsuit.Velocity[1] > BLOCK(1.5f))
		lara->Control.Subsuit.Velocity[1] = BLOCK(1.5f);

	if (lara->Control.Subsuit.Velocity[0] != 0 || lara->Control.Subsuit.Velocity[1] != 0)
	{
		auto mul1 = (float)abs(lara->Control.Subsuit.Velocity[0]) / BLOCK(8);
		auto mul2 = (float)abs(lara->Control.Subsuit.Velocity[1]) / BLOCK(8);
		auto vol = ((mul1 + mul2) * 5.0f) + 0.5f;
		SoundEffect(SFX_TR5_VEHICLE_DIVESUIT_ENGINE, &item->Pose, SoundEnvironment::Underwater, 1.0f + (mul1 + mul2), vol);
	}
}

// TODO: Unused; I will pick this back up later. -- Sezz 2022.06.22
void ModulateLaraSlideVelocity(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	constexpr int minVelocity = 50;
	constexpr int maxVelocity = LARA_TERMINAL_VELOCITY;

	if (g_GameFlow->GetSettings()->Animations.SlideExtended)
	{
		auto probe = GetPointCollision(*item);
		short minSlideAngle = ANGLE(33.75f);
		//short steepness = Geometry::GetSurfaceSlopeAngle(probe.FloorTilt);
		//short direction = Geometry::GetSurfaceAspectAngle(probe.FloorTilt);

		float velocityMultiplier = 1 / (float)ANGLE(33.75f);
		//int slideVelocity = std::min<int>(minVelocity + 10 * (steepness * velocityMultiplier), LARA_TERMINAL_VELOCITY);
		//short deltaAngle = abs((short)(direction - item->Pose.Orientation.y));

		//PrintDebugMessage("%d", slideVelocity);

		//lara->ExtraVelocity.x += slideVelocity;
		//lara->ExtraVelocity.y += slideVelocity * phd_sin(steepness);
	}
	//else
	//lara->ExtraVelocity.x += minVelocity;
}

void AlignLaraToSurface(ItemInfo* item, float alpha)
{
	// Determine relative orientation adhering to floor normal.
	auto floorNormal = g_Configuration.IsUsingClassicControls() ? -Vector3::UnitY : GetPointCollision(*item).GetFloorNormal();
	auto orient = Geometry::GetRelOrientToNormal(item->Pose.Orientation.y, floorNormal);

	// Apply extra rotation according to alpha.
	auto extraRot = orient - item->Pose.Orientation;
	item->Pose.Orientation += extraRot * alpha;
}

void SetLaraVault(ItemInfo* item, CollisionInfo* coll, const VaultTestResult& vaultResult)
{
	auto& player = GetLaraInfo(*item);

	ResetPlayerTurnRateY(*item);
	item->Animation.IsAirborne = false;
	player.Context.ProjectedFloorHeight = vaultResult.Height;

	if (vaultResult.SetBusyHands)
		player.Control.HandStatus = HandStatus::Busy;

	if (vaultResult.SnapToLedge)
	{
		SnapItemToLedge(item, coll, 0.2f, false);
		player.Context.TargetOrientation = EulerAngles(0, coll->NearestLedgeAngle, 0);
	}
	else
	{
		player.Context.TargetOrientation = EulerAngles(0, item->Pose.Orientation.y, 0);
	}

	if (vaultResult.SetJumpVelocity)
	{
		int jumpHeight = player.Context.ProjectedFloorHeight - item->Pose.Position.y;
		player.Context.CalcJumpVelocity = GetPlayerJumpVelocity(jumpHeight);
	}
}

void SetLaraLand(ItemInfo* item, CollisionInfo* coll)
{
	// Avoid clearing forward velocity when hitting the ground running.
	if (item->Animation.TargetState != LS_RUN_FORWARD)
		item->Animation.Velocity.z = 0.0f;

	// TODO: Commenting this avoids an unusual bug where if the player hits a ceiling, they won't land. I hope to find a proper solution later. -- Sezz 2022.02.18
	//item->Animation.IsAirborne = false;
	item->Animation.Velocity.y = 0.0f;
	LaraSnapToHeight(item, coll);
}

void SetLaraFallAnimation(ItemInfo* item)
{
	SetAnimation(item, LA_FALL_START);
	item->Animation.IsAirborne = true;
	item->Animation.Velocity.y = 0.0f;
}

void SetLaraFallBackAnimation(ItemInfo* item)
{
	SetAnimation(item, LA_FALL_BACK);
	item->Animation.IsAirborne = true;
	item->Animation.Velocity.y = 0.0f;
}

void SetLaraMonkeyFallAnimation(ItemInfo* item)
{
	// HACK: Disallow release during 180 turn action.
	if (item->Animation.ActiveState == LS_MONKEY_TURN_180)
		return;

	SetAnimation(item, LA_MONKEY_TO_FREEFALL);
	SetLaraMonkeyRelease(item);
}

void SetLaraMonkeyRelease(ItemInfo* item)
{
	auto* lara = GetLaraInfo(item);

	// TODO: Hack. Regrab occurs.
	if (g_Configuration.IsUsingModernControls())
		item->Pose.Position.y += 20;

	item->Animation.IsAirborne = true;
	item->Animation.Velocity.y = 1.0f;
	item->Animation.Velocity.z = 2.0f;
	lara->Control.TurnRate.y = 0;
	lara->Control.HandStatus = HandStatus::Free;
}

// temp
void SetLaraSlideAnimation(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	if (TestEnvironment(ENV_FLAG_SWAMP, item))
		return;

	static short oldAngle = 1;

	short aspectAngle = Geometry::GetSurfaceAspectAngle(coll->FloorNormal);
	short angle = GetLegacySlideHeadingAngle(coll->FloorNormal);

	short delta = angle - item->Pose.Orientation.y;

	ShiftItem(item, coll);

	if (delta < -ANGLE(90.0f) || delta > ANGLE(90.0f))
	{
		if (item->Animation.ActiveState == LS_SLIDE_BACK && oldAngle == angle)
			return;

		SetAnimation(item, LA_SLIDE_BACK_START);
		item->Pose.Orientation.y = angle + ANGLE(180.0f);
	}
	else
	{
		if (item->Animation.ActiveState == LS_SLIDE_FORWARD && oldAngle == angle)
			return;

		SetAnimation(item, LA_SLIDE_FORWARD);
		item->Pose.Orientation.y = angle;
	}

	LaraSnapToHeight(item, coll);
	lara->Control.HeadingOrient.y = angle;
	lara->Control.TurnRate.y = 0;
	oldAngle = angle;
}

// TODO: Do it later.
void newSetLaraSlideAnimation(ItemInfo* item, CollisionInfo* coll)
{
	short headinAngle = GetPlayerSlideHeadingAngle(item, coll);
	short deltaAngle = headinAngle - item->Pose.Orientation.y;

	if (!g_GameFlow->GetSettings()->Animations.SlideExtended)
		item->Pose.Orientation.y = headinAngle;

	// Snap to height upon slide entrance.
	if (item->Animation.ActiveState != LS_SLIDE_FORWARD &&
		item->Animation.ActiveState != LS_SLIDE_BACK)
	{
		LaraSnapToHeight(item, coll);
	}

	// Slide forward.
	if (abs(deltaAngle) <= ANGLE(90.0f))
	{
		if (item->Animation.ActiveState == LS_SLIDE_FORWARD && abs(deltaAngle) <= ANGLE(180.0f))
			return;

		SetAnimation(item, LA_SLIDE_FORWARD);
	}
	// Slide backward.
	else
	{
		if (item->Animation.ActiveState == LS_SLIDE_BACK && abs((short)(deltaAngle - ANGLE(180.0f))) <= -ANGLE(180.0f))
			return;

		SetAnimation(item, LA_SLIDE_BACK_START);
	}
}

void SetLaraCornerAnimation(ItemInfo* item, CollisionInfo* coll, bool flip)
{
	auto* lara = GetLaraInfo(item);

	if (item->HitPoints <= 0)
	{
		SetAnimation(item, LA_FALL_START);
		item->Animation.IsAirborne = true;
		item->Animation.Velocity.z = 2;
		item->Animation.Velocity.y = 1;
		item->Pose.Position.y += CLICK(1);
		item->Pose.Orientation.y += lara->Context.NextCornerPos.Orientation.y / 2;
		lara->Control.HandStatus = HandStatus::Free;
		return;
	}

	if (flip)
	{
		if (lara->Control.IsClimbingLadder)
			SetAnimation(item, LA_LADDER_IDLE);
		else
			SetAnimation(item, LA_HANG_IDLE);

		item->Pose.Position = lara->Context.NextCornerPos.Position;
		item->Pose.Orientation.y = lara->Context.NextCornerPos.Orientation.y;
		coll->Setup.PrevPosition = lara->Context.NextCornerPos.Position;
	}
}

void SetLaraSwimDiveAnimation(ItemInfo* item)
{
	auto* lara = GetLaraInfo(item);

	SetAnimation(item, LA_ONWATER_DIVE);
	item->Animation.TargetState = LS_UNDERWATER_SWIM_FORWARD;
	item->Animation.Velocity.y = g_GameFlow->GetSettings()->Physics.SwimVelocity * 0.4f;
	item->Pose.Orientation.x = -ANGLE(45.0f);
	lara->Control.WaterStatus = WaterStatus::Underwater;
}

void SetLaraVehicle(ItemInfo* item, ItemInfo* vehicle)
{
	auto* lara = GetLaraInfo(item);

	if (vehicle == nullptr)
	{
		if (lara->Context.Vehicle != NO_VALUE)
			g_Level.Items[lara->Context.Vehicle].Active = false;

		lara->Context.Vehicle = NO_VALUE;
	}
	else
	{
		g_Level.Items[vehicle->Index].Active = true;
		lara->Context.Vehicle = vehicle->Index;
	}
}

void ResetPlayerLean(ItemInfo* item, float alpha, bool resetZAxisLean, bool resetXAxisLean)
{
	short xTargetOrient = resetXAxisLean ? 0 : item->Pose.Orientation.x;
	short zTargetOrient = resetZAxisLean ? 0 : item->Pose.Orientation.z;

	item->Pose.Orientation.Lerp(EulerAngles(xTargetOrient, item->Pose.Orientation.y, zTargetOrient), alpha);
}

void ResetPlayerFlex(ItemInfo* item, float alpha)
{
	auto& player = GetLaraInfo(*item);

	player.LimbRot.Hip.Lerp(EulerAngles::Identity, alpha);
	player.ExtraTorsoRot.Lerp(EulerAngles::Identity, alpha);
	player.ExtraHeadRot.Lerp(EulerAngles::Identity, alpha);
}

void ResetPlayerLookAround(ItemInfo& item, float alpha)
{
	auto& player = GetLaraInfo(item);

	player.Control.Look.Orientation = EulerAngles::Identity;
	return;

	if (g_Camera.type != CameraType::Look)
	{
		player.ExtraHeadRot.Lerp(EulerAngles::Identity, alpha);

		if (player.Control.HandStatus != HandStatus::Busy &&
			!player.LeftArm.Locked && !player.RightArm.Locked &&
			player.Context.Vehicle == NO_VALUE)
		{
			player.ExtraTorsoRot = player.ExtraHeadRot;
		}
		else
		{
			if (!player.ExtraHeadRot.x)
				player.ExtraTorsoRot.x = 0;

			if (!player.ExtraHeadRot.y)
				player.ExtraTorsoRot.y = 0;

			if (!player.ExtraHeadRot.z)
				player.ExtraTorsoRot.z = 0;
		}
	}
}

void RumbleLaraHealthCondition(ItemInfo* item)
{
	constexpr auto POWER = 0.2f;
	constexpr auto DELAY = 0.1f;

	const auto& player = GetLaraInfo(*item);

	if (item->HitPoints > LARA_HEALTH_CRITICAL && player.Status.Poison == 0)
		return;

	if (item->HitPoints == 0)
		return;

	bool doPulse = ((GlobalCounter & 0x0F) % 0x0F == 1);
	if (doPulse)
		Rumble(POWER, DELAY);
}

// NOTE: Formula uses kinematic equation of motion for vertical motion under constant acceleration.
float GetPlayerJumpVelocity(float jumpHeight)
{
	constexpr auto JUMP_HEIGHT_MAX	= -CLICK(7.5f);
	constexpr auto JUMP_HEIGHT_MIN	= -CLICK(3.5f);
	constexpr auto A2				= -9600.0f;
	constexpr auto UNIT_CONV_FACTOR = 12.0f;
	constexpr auto OFFSET			= -3.0f;

	jumpHeight = std::clamp(jumpHeight, JUMP_HEIGHT_MAX, JUMP_HEIGHT_MIN);
	return (-sqrt(A2 - (jumpHeight * UNIT_CONV_FACTOR)) + OFFSET);
}
