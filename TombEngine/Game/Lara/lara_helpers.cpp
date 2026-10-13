#include "framework.h"
#include "Game/Lara/lara_helpers.h"

#include "Game/camera.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/floordata.h"
#include "Game/collision/Point.h"
#include "Game/control/control.h"
#include "Game/control/volume.h"
#include "Game/items.h"
#include "Game/effects/bubble.h"
#include "Game/effects/drip.h"
#include "Game/GuiObjects.h"
#include "Game/Lara/PlayerContext.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_collide.h"
#include "Game/Lara/lara_fire.h"
#include "Game/Lara/lara_tests.h"
#include "Game/savegame.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Include/ScriptInterfaceGame.h"
#include "Scripting/Include/ScriptInterfaceLevel.h"
#include "Sound/sound.h"
#include "Specific/configuration.h"
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
	auto* lara = GetLaraInfo(item);

	// Update AFK pose timer.
	if (lara->Control.Count.Pose < (g_GameFlow->GetSettings()->Animations.PoseTimeout * FPS) &&
		!(IsHeld(In::Look) || IsOpticActionHeld()))
	{
		lara->Control.Count.Pose++;
	}
	else
	{
		lara->Control.Count.Pose = 0;
	}

	// Reset running jump timer.
	if (!IsRunJumpCountableState(item->Animation.ActiveState))
		lara->Control.Count.Run = 0;

	// Reset running jump action queue.
	if (!IsRunJumpQueueableState(item->Animation.ActiveState))
		lara->Control.IsRunJumpQueued = false;

	// Reset lean.
	if ((!lara->Control.IsMoving || (lara->Control.IsMoving && !(IsHeld(In::Left) || IsHeld(In::Right)))) &&
		(!lara->Control.IsLow && item->Animation.ActiveState != LS_DEATH)) // HACK: Don't interfere with surface alignment in crouch, crawl, and death states.
	{
		ResetPlayerLean(item, 1 / 6.0f);
	}

	// Reset crawl flex.
	if (!IsHeld(In::Look) && coll->Setup.Height > LARA_HEIGHT - LARA_HEADROOM && // HACK
		(item->Animation.Velocity.z == 0.0f || (item->Animation.Velocity.z != 0.0f && !(IsHeld(In::Left) || IsHeld(In::Right)))))
	{
		ResetPlayerFlex(item, 0.1f);
	}

	// Remastered controls: the heading solver turns the player in its states, so the turn rate is zeroed there. -- divezzi, 10.10.26
	if (g_Configuration.IsUsingRemasteredControls() && DoesPlayerRemasteredSolverOwnHeading(*item))
		lara->Control.TurnRate = 0;

	// Apply and reset turn rate.
	item->Pose.Orientation.y += lara->Control.TurnRate;
	if (!(IsHeld(In::Left) || IsHeld(In::Right)))
		lara->Control.TurnRate = 0;

	lara->Control.IsLow = false;
	lara->Control.IsMonkeySwinging = false;
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

	// TODO: Add gamepad quick weapon selection (e.g. DualSense touchpad combined with other inputs) here, setting requestedGunType. -- divezzi, 12.10.26

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

	// 1) Check if look mode is not None.
	if (player.Control.Look.Mode == LookMode::None)
		return false;

	// 2) Check if drawn weapon has lasersight.
	if (player.Control.HandStatus == HandStatus::WeaponReady &&
		player.Weapons[(int)player.Control.Weapon.GunType].HasLasersight)
	{
		return true;
	}

	// 3) Test for switchable target.
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
	constexpr auto TURN_RATE_MAX   = ANGLE(4.0f);
	constexpr auto TURN_RATE_ACCEL = ANGLE(0.75f);

	auto& player = GetLaraInfo(item);

	// Set optics.
	Camera.type = CameraType::Look;

	bool isSlow = IsHeld(In::Walk);
	auto axisCoeff = Vector2::Zero;

	// Remastered controls: look-around reads the keys as pressed, before the movement rewrite. -- divezzi, 10.10.26
	// Determine X axis coefficient.
	if ((IsRawActionHeld(In::Forward) || IsRawActionHeld(In::Back)) &&
		(player.Control.Look.Mode == LookMode::Free || player.Control.Look.Mode == LookMode::Vertical))
	{
		axisCoeff.x = AxisMap[AxisID::Move].y;
	}

	// Determine Y axis coefficient.
	if ((IsRawActionHeld(In::Left) || IsRawActionHeld(In::Right)) &&
		(player.Control.Look.Mode == LookMode::Free || player.Control.Look.Mode == LookMode::Horizontal))
	{
		axisCoeff.y = AxisMap[AxisID::Move].x;
	}

	// Determine turn rate base values.
	short turnRateMax = isSlow ? (TURN_RATE_MAX / 2) : TURN_RATE_MAX;
	short turnRateAccel = isSlow ? (TURN_RATE_ACCEL / 2) : TURN_RATE_ACCEL;

	// Normalize turn rate base values.
	turnRateMax = NormalizeLookAroundTurnRate(turnRateMax, player.Control.Look.OpticRange);
	turnRateAccel = NormalizeLookAroundTurnRate(turnRateAccel, player.Control.Look.OpticRange);

	// Modulate turn rates.
	player.Control.Look.TurnRate = EulerAngles(
		ModulateLaraTurnRate(player.Control.Look.TurnRate.x, turnRateAccel, 0, turnRateMax, axisCoeff.x, invertXAxis),
		ModulateLaraTurnRate(player.Control.Look.TurnRate.y, turnRateAccel, 0, turnRateMax, axisCoeff.y, false),
		0);

	// Apply turn rates.
	player.Control.Look.Orientation += player.Control.Look.TurnRate;
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
		SetAnimation(*item, LA_FALL_START, 0, GetInternalBlendDuration(), BezierCurve2::EaseOut);
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

void HandlePlayerLean(ItemInfo* item, CollisionInfo* coll, short baseRate, short maxAngle)
{
	if (!item->Animation.Velocity.z)
		return;

	float axisCoeff = AxisMap[AxisID::Move].x;
	int sign = copysign(1, axisCoeff);
	short maxAngleNormalized = maxAngle * axisCoeff;

	if (coll->CollisionType == CollisionType::Left || coll->CollisionType == CollisionType::Right)
		maxAngleNormalized *= 0.6f;

	item->Pose.Orientation.z += std::min<short>(baseRate, abs(maxAngleNormalized - item->Pose.Orientation.z) / 3) * sign;
}

void HandlePlayerCrawlFlex(ItemInfo& item)
{
	constexpr auto FLEX_RATE_ANGLE = ANGLE(2.25f);
	constexpr auto FLEX_ANGLE_MAX  = ANGLE(50.0f) / 2; // 2 = hardcoded number of bones to flex (head and torso).

	auto& player = GetLaraInfo(item);

	// Check if player is moving.
	if (item.Animation.Velocity.z == 0.0f)
		return;

	float axisCoeff = AxisMap[AxisID::Move].x;
	int sign = copysign(1, axisCoeff);
	short maxAngleNormalized = FLEX_ANGLE_MAX * axisCoeff;

	if (abs(player.ExtraTorsoRot.z) < FLEX_ANGLE_MAX)
		player.ExtraTorsoRot.z += std::min<short>(FLEX_RATE_ANGLE, abs(maxAngleNormalized - player.ExtraTorsoRot.z) / 6) * sign;

	if (!IsHeld(In::Look) && item.Animation.ActiveState != LS_CRAWL_BACK)
	{
		player.ExtraHeadRot.z = player.ExtraTorsoRot.z / 2;
		player.ExtraHeadRot.y = player.ExtraHeadRot.z;
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
	if (KeyMap[SDL_SCANCODE_O] && dbFlyCheat)
	{
		if (player.Context.Vehicle == NO_VALUE)
		{
			if (KeyMap[SDL_SCANCODE_LSHIFT] || KeyMap[SDL_SCANCODE_RSHIFT])
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
	dbFlyCheat = !KeyMap[SDL_SCANCODE_O];
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

			// Remastered controls: with Smooth Sprint Steps on, a sprint climbs the step without the step-up animation. -- divezzi, 10.10.26
			if (g_Configuration.IsUsingRemasteredControls() && g_Configuration.EnableSmoothSprintSteps &&
				item->Animation.ActiveState == LS_SPRINT)
			{
				item->Pose.Position.y += coll->Middle.Floor;
				return;
			}

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

	return JumpDirection::None;
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

// TODO: Make these two functions methods of LaraInfo someday. -- Sezz 2022.06.26
void ModulateLaraTurnRateX(ItemInfo* item, short accelRate, short minTurnRate, short maxTurnRate, bool invert)
{
	auto* lara = GetLaraInfo(item);

	//lara->Control.TurnRate.x = ModulateLaraTurnRate(lara->Control.TurnRate.x, accelRate, minTurnRate, maxTurnRate, AxisMap[InputAxis::Move].y, invert);
}

// Remastered controls: turn states whose turn keys turn the player at the full rate, since the stick's x axis is camera-relative. -- divezzi, 12.10.26
static bool IsRemasteredUnscaledTurnState(const ItemInfo& item)
{
	if (!g_Configuration.IsUsingRemasteredControls())
		return false;

	switch (item.Animation.ActiveState)
	{
	case LS_TURN_LEFT_SLOW:
	case LS_TURN_RIGHT_SLOW:
	case LS_TURN_LEFT_FAST:
	case LS_TURN_RIGHT_FAST:
		return true;
	}

	return false;
}

void ModulateLaraTurnRateY(ItemInfo* item, short accelRate, short minTurnRate, short maxTurnRate, bool invert)
{
	auto* lara = GetLaraInfo(item);

	float axisCoeff = AxisMap[AxisID::Move].x;
	if (item->Animation.IsAirborne)
	{
		int sign = std::copysign(1, axisCoeff);
		axisCoeff = std::min(1.2f, abs(axisCoeff)) * sign;
	}

	bool isUnscaledOverride = false;
	if (IsRemasteredUnscaledTurnState(*item) &&
		(lara->Control.Remastered.IsArmedTurning || IsHeld(In::Left) || IsHeld(In::Right)))
	{
		bool isTurningLeft = (item->Animation.ActiveState == LS_TURN_LEFT_SLOW ||
							  item->Animation.ActiveState == LS_TURN_LEFT_FAST);
		axisCoeff = isTurningLeft ? -1.0f : 1.0f;
		isUnscaledOverride = true;
	}

	lara->Control.TurnRate/*.y*/ = ModulateLaraTurnRate(lara->Control.TurnRate/*.y*/, accelRate, minTurnRate, maxTurnRate, axisCoeff, invert);

	// Remastered controls: the armed turn does not overshoot the heading it turns to. -- divezzi, 12.10.26
	if (isUnscaledOverride && lara->Control.Remastered.IsArmedTurning)
	{
		short errorMax = (short)abs((int)lara->Control.Remastered.ArmedTurnErrorY);
		lara->Control.TurnRate = std::clamp(lara->Control.TurnRate, (short)-errorMax, (short)errorMax);
	}
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
	player.Control.TurnRate/*.x*/ = ResetPlayerTurnRate(player.Control.TurnRate/*.x*/, decelRate);
}

void ResetPlayerTurnRateY(ItemInfo& item, short decelRate)
{
	auto& player = GetLaraInfo(item);
	player.Control.TurnRate/*.y*/ = ResetPlayerTurnRate(player.Control.TurnRate/*.y*/, decelRate);
}

// Remastered controls: input rewrite and heading solver for camera-relative movement. -- divezzi, 10.10.26

enum class RemasteredInputScope
{
	None,
	Ground,

	// Mid-air. The solver turns the player, and the transform rewrites the actions.
	Airborne,

	// Mid-air. The transform rewrites the actions, and the solver does not turn the player.
	AirborneNoSolve,

	// Weapons ready, standing. The transform presses the turn keys and the stock turn states turn the player.
	ArmedSynthesis
};

// Grab and weapon button: Fire under Remastered controls, Action under Classic controls.
// A script clear of Action holds it off, and a script push of Action presses it.
bool IsRemasteredGrabHeld()
{
	if (!g_Configuration.IsUsingRemasteredControls())
		return IsHeld(In::Action);

	if (IsCleared(In::Action))
		return false;

	if (IsPushed(In::Action))
		return true;

	return IsHeld(In::Fire);
}

// Vehicle gun button: Action under Remastered controls, where Accelerate and Fire share the gamepad's right trigger.
// Fire under Classic controls. A script clear of Fire holds it off, and a script push of Fire presses it.
bool IsRemasteredVehicleFireHeld()
{
	if (!g_Configuration.IsUsingRemasteredControls())
		return IsHeld(In::Fire);

	if (IsCleared(In::Fire))
		return false;

	if (IsPushed(In::Fire))
		return true;

	return IsHeld(In::Action);
}

// Steps an angle toward a target by a share of the remaining error, clamped between a minimum and a maximum step.
// An exact half-turn error keeps the previous turn direction, stored in prevSign.
static short ApproachHeadingAngle(short fromAngle, short toAngle, float rate, short stepMax, int& prevSign)
{
	constexpr auto STEP_MIN = ANGLE(0.5f);

	short deltaAngle = Geometry::GetShortestAngle(fromAngle, toAngle);
	int deltaAngleAbs = abs((int)deltaAngle);
	if (deltaAngleAbs == 0)
		return toAngle;

	float step = ((float)deltaAngleAbs * rate) / PI;
	step = std::clamp(step, (float)STEP_MIN, (float)stepMax);
	if (step >= (float)deltaAngleAbs)
		return toAngle;

	int sign = (deltaAngle < 0) ? -1 : 1;
	if (deltaAngle == SHRT_MIN && prevSign != 0)
		sign = prevSign;

	prevSign = sign;
	return (short)(fromAngle + (short)((int)step * sign));
}

// Weapons drawn and ready. A torch counts as holstered.
static bool IsPlayerRemasteredWeaponReady(const ItemInfo& item)
{
	const auto& player = GetLaraInfo(item);
	return (player.Control.HandStatus == HandStatus::WeaponReady &&
			player.Control.Weapon.GunType != LaraWeaponType::Torch);
}

// Weapons drawn or being drawn. A torch counts as holstered.
static bool IsPlayerInRemasteredCombatBackStance(const ItemInfo& item)
{
	const auto& player = GetLaraInfo(item);
	return ((player.Control.HandStatus == HandStatus::WeaponReady ||
			 player.Control.HandStatus == HandStatus::WeaponDraw) &&
			player.Control.Weapon.GunType != LaraWeaponType::Torch);
}

static bool IsRemasteredStandingState(const ItemInfo& item)
{
	switch (item.Animation.ActiveState)
	{
	case LS_IDLE:
	case LS_SOFT_SPLAT:
	case LS_TURN_LEFT_SLOW:
	case LS_TURN_RIGHT_SLOW:
	case LS_TURN_LEFT_FAST:
	case LS_TURN_RIGHT_FAST:
	case LS_JUMP_PREPARE:
		return true;

	default:
		return false;
	}
}

static bool IsPlayerLockedOnTarget(const ItemInfo& item)
{
	const auto& player = GetLaraInfo(item);

	if (player.TargetEntity == nullptr)
		return false;

	return (player.LeftArm.Locked || player.RightArm.Locked);
}

// Hanging, shimmying, monkey swing, ladder, rope and pole states.
static bool DoesRemasteredInputOwnGrip(const ItemInfo& item)
{
	if (!g_Configuration.IsUsingRemasteredControls())
		return false;

	switch (item.Animation.ActiveState)
	{
	case LS_MONKEY_IDLE:
	case LS_MONKEY_FORWARD:
	case LS_MONKEY_BACK:
	case LS_MONKEY_TURN_LEFT:
	case LS_MONKEY_TURN_RIGHT:
	case LS_MONKEY_TURN_180:
	case LS_MONKEY_SHIMMY_LEFT:
	case LS_MONKEY_SHIMMY_RIGHT:
	case LS_HANG:
	case LS_SHIMMY_LEFT:
	case LS_SHIMMY_RIGHT:
	case LS_SHIMMY_OUTER_LEFT:
	case LS_SHIMMY_OUTER_RIGHT:
	case LS_SHIMMY_INNER_LEFT:
	case LS_SHIMMY_INNER_RIGHT:
	case LS_SHIMMY_45_OUTER_LEFT:
	case LS_SHIMMY_45_OUTER_RIGHT:
	case LS_SHIMMY_45_INNER_LEFT:
	case LS_SHIMMY_45_INNER_RIGHT:
	case LS_LADDER_IDLE:
	case LS_LADDER_STOP:
	case LS_LADDER_UP:
	case LS_LADDER_DOWN:
	case LS_LADDER_LEFT:
	case LS_LADDER_RIGHT:
	case LS_ROPE_IDLE:
	case LS_ROPE_UP:
	case LS_ROPE_SWING:
	case LS_ROPE_UNKNOWN:
	case LS_ROPE_TURN_CLOCKWISE:
	case LS_ROPE_TURN_COUNTER_CLOCKWISE:
	case LS_ROPE_DOWN:
	case LS_POLE_IDLE:
	case LS_POLE_UP:
	case LS_POLE_DOWN:
	case LS_POLE_TURN_CLOCKWISE:
	case LS_POLE_TURN_COUNTER_CLOCKWISE:
		return true;

	default:
		return false;
	}
}

// The grip in a hanging state. Roll always lets go. With the grab toggle off, releasing the grab button lets go too.
static bool GetRemasteredGrip(bool isGrabHeld, bool isRollHeld)
{
	if (isRollHeld)
		return false;

	return (g_Configuration.EnableGrabToggle || isGrabHeld);
}

static bool IsPlayerRemasteredOnKnees(const ItemInfo& item)
{
	switch (item.Animation.ActiveState)
	{
	case LS_CRAWL_IDLE:
	case LS_CRAWL_FORWARD:
	case LS_CRAWL_BACK:
	case LS_CRAWL_TURN_LEFT:
	case LS_CRAWL_TURN_RIGHT:
	case LS_CRAWL_TURN_180:
		return true;

	default:
		return false;
	}
}

// Walk with a pure sideways push, read from the raw keys. The stock crawl turn plays.
static bool IsPlayerRemasteredCrawlTurnHeld()
{
	return (IsRawActionHeld(In::Walk) && (IsRawActionHeld(In::Left) || IsRawActionHeld(In::Right)) &&
			!IsRawActionHeld(In::Forward) && !IsRawActionHeld(In::Back));
}

// A script pushed a direction this frame. That tick plays classic.
static bool IsPlayerRemasteredDirectionPushed()
{
	return (IsPushed(In::Forward) || IsPushed(In::Back) || IsPushed(In::Left) || IsPushed(In::Right));
}

// Turn rate and step cap of the heading solver in the player's current state.
// Scope None leaves the state to play classic.
static RemasteredInputScope GetPlayerRemasteredTurnParams(const ItemInfo& item, float& rate, short& stepMax)
{
	constexpr auto TURN_RATE_IDLE		= 1.0f;
	constexpr auto TURN_RATE_WALK		= 1.0f;
	constexpr auto TURN_RATE_RUN		= 1.0f;
	constexpr auto TURN_RATE_SPRINT		= 0.2f;
	constexpr auto TURN_RATE_WADE		= 0.6f;
	constexpr auto TURN_RATE_CRAWL_IDLE = 0.5f;
	constexpr auto TURN_RATE_CRAWL		= 0.2f;

	constexpr auto TURN_STEP_MAX_IDLE		= ANGLE(90.0f);
	constexpr auto TURN_STEP_MAX_WALK		= ANGLE(45.0f);
	constexpr auto TURN_STEP_MAX_RUN		= ANGLE(30.0f);
	constexpr auto TURN_STEP_MAX_SPRINT		= ANGLE(5.0f);
	constexpr auto TURN_STEP_MAX_WADE		= ANGLE(15.0f);
	constexpr auto TURN_STEP_MAX_CRAWL_IDLE = ANGLE(90.0f);
	constexpr auto TURN_STEP_MAX_CRAWL		= ANGLE(7.0f);

	constexpr auto TURN_RATE_MONKEY		= 0.2f;
	constexpr auto TURN_STEP_MAX_MONKEY = ANGLE(7.0f);

	constexpr auto TURN_RATE_JUMP_PREPARE	  = 1.0f;
	constexpr auto TURN_STEP_MAX_JUMP_PREPARE = (short)SHRT_MAX;
	constexpr auto TURN_RATE_AIRBORNE		  = 0.1f;
	constexpr auto TURN_STEP_MAX_AIRBORNE	  = ANGLE(2.0f);

	constexpr auto TURN_RATE_AIM_LOCKED		= 0.25f;
	constexpr auto TURN_STEP_MAX_AIM_LOCKED = ANGLE(7.0f);

	if (IsPlayerRemasteredDirectionPushed())
		return RemasteredInputScope::None;

	if (IsPlayerRemasteredWeaponReady(item))
	{
		switch (item.Animation.ActiveState)
		{
		case LS_IDLE:
		case LS_SOFT_SPLAT:
		case LS_TURN_LEFT_SLOW:
		case LS_TURN_RIGHT_SLOW:
		case LS_TURN_LEFT_FAST:
		case LS_TURN_RIGHT_FAST:
			return RemasteredInputScope::ArmedSynthesis;
		}
	}

	switch (item.Animation.ActiveState)
	{
	case LS_IDLE:
	case LS_SOFT_SPLAT:
	case LS_TURN_LEFT_SLOW:
	case LS_TURN_RIGHT_SLOW:
	case LS_TURN_LEFT_FAST:
	case LS_TURN_RIGHT_FAST:
		rate = TURN_RATE_IDLE;
		stepMax = TURN_STEP_MAX_IDLE;
		return RemasteredInputScope::Ground;

	case LS_WALK_FORWARD:
		rate = TURN_RATE_WALK;
		stepMax = TURN_STEP_MAX_WALK;
		return RemasteredInputScope::Ground;

	case LS_RUN_FORWARD:
		if (IsPlayerLockedOnTarget(item))
		{
			rate = TURN_RATE_AIM_LOCKED;
			stepMax = TURN_STEP_MAX_AIM_LOCKED;
		}
		else
		{
			rate = TURN_RATE_RUN;
			stepMax = TURN_STEP_MAX_RUN;
		}

		return RemasteredInputScope::Ground;

	case LS_SPRINT:
		rate = TURN_RATE_SPRINT;
		stepMax = TURN_STEP_MAX_SPRINT;
		return RemasteredInputScope::Ground;

	case LS_WADE_FORWARD:
		rate = TURN_RATE_WADE;
		stepMax = TURN_STEP_MAX_WADE;
		return RemasteredInputScope::Ground;

	case LS_STEP_LEFT:
	case LS_STEP_RIGHT:
		rate = TURN_RATE_IDLE;
		stepMax = TURN_STEP_MAX_IDLE;
		return RemasteredInputScope::Ground;

	case LS_CRAWL_IDLE:
	case LS_CRAWL_TURN_LEFT:
	case LS_CRAWL_TURN_RIGHT:
		if (IsPlayerRemasteredCrawlTurnHeld())
			return RemasteredInputScope::None;

		rate = TURN_RATE_CRAWL_IDLE;
		stepMax = TURN_STEP_MAX_CRAWL_IDLE;
		return RemasteredInputScope::Ground;

	case LS_CRAWL_FORWARD:
	case LS_CRAWL_BACK:
		rate = TURN_RATE_CRAWL;
		stepMax = TURN_STEP_MAX_CRAWL;
		return RemasteredInputScope::Ground;

	case LS_MONKEY_IDLE:
	case LS_MONKEY_FORWARD:
	case LS_MONKEY_BACK:
	case LS_MONKEY_TURN_LEFT:
	case LS_MONKEY_TURN_RIGHT:
	case LS_MONKEY_SHIMMY_LEFT:
	case LS_MONKEY_SHIMMY_RIGHT:
		rate = TURN_RATE_MONKEY;
		stepMax = TURN_STEP_MAX_MONKEY;
		return RemasteredInputScope::Ground;

	// Jump wind-up. Holstered, she turns toward the stick before the launch. Armed, it plays classic.
	case LS_JUMP_PREPARE:
		if (IsPlayerRemasteredWeaponReady(item))
			return RemasteredInputScope::None;

		rate = TURN_RATE_JUMP_PREPARE;
		stepMax = TURN_STEP_MAX_JUMP_PREPARE;
		return RemasteredInputScope::Ground;

	// The back jump keeps its launch heading, armed or not, so its roll push is read the same way.
	case LS_JUMP_BACK:
		return RemasteredInputScope::AirborneNoSolve;

	case LS_JUMP_LEFT:
	case LS_JUMP_RIGHT:
		if (IsPlayerRemasteredWeaponReady(item))
			return RemasteredInputScope::None;

		return RemasteredInputScope::AirborneNoSolve;

	// The forward jump is the only jump the solver steers mid-air.
	case LS_JUMP_FORWARD:
		rate = TURN_RATE_AIRBORNE;
		stepMax = TURN_STEP_MAX_AIRBORNE;
		return RemasteredInputScope::Airborne;

	case LS_JUMP_UP:
	case LS_FREEFALL:
	case LS_REACH:
	case LS_SWAN_DIVE:
		return RemasteredInputScope::AirborneNoSolve;

	default:
		return RemasteredInputScope::None;
	}
}

// True if the heading solver turns the player in the current state.
bool DoesPlayerRemasteredSolverOwnHeading(const ItemInfo& item)
{
	float rate = 0.0f;
	short stepMax = 0;
	auto scope = GetPlayerRemasteredTurnParams(item, rate, stepMax);
	return (scope == RemasteredInputScope::Ground || scope == RemasteredInputScope::Airborne);
}

// True if the camera turns toward the player's movement. Ground states only.
bool DoesPlayerRemasteredCameraAssistApply(const ItemInfo& item)
{
	float rate = 0.0f;
	short stepMax = 0;
	return (GetPlayerRemasteredTurnParams(item, rate, stepMax) == RemasteredInputScope::Ground);
}

// Turn rate and step cap of the heading solver in the water states it steers. False in any other state,
// on a script direction push, and with the divesuit, which keeps its own pitch control.
// isUnderwater is set for the states that also follow the camera pitch and swim on Jump: underwater and the fly cheat.
static bool GetPlayerRemasteredWaterTurnParams(const ItemInfo& item, float& rate, short& stepMax, bool& isUnderwater)
{
	constexpr auto TURN_RATE_UNDERWATER = 1.0f;
	constexpr auto TURN_RATE_SURFACE	= 0.5f;
	constexpr auto TURN_STEP_MAX		= ANGLE(179.0f); // Not reached: a step is at most 57 degrees at rate 1.0.

	isUnderwater = false;

	if (IsPlayerRemasteredDirectionPushed())
		return false;

	const auto& level = *g_GameFlow->GetLevel(CurrentLevel);
	if (level.GetLaraType() == LaraType::Divesuit)
		return false;

	switch (item.Animation.ActiveState)
	{
	case LS_FLY_CHEAT:
	case LS_UNDERWATER_IDLE:
	case LS_UNDERWATER_SWIM_FORWARD:
	case LS_UNDERWATER_INERTIA:
		rate = TURN_RATE_UNDERWATER;
		stepMax = TURN_STEP_MAX;
		isUnderwater = true;
		return true;

	case LS_ONWATER_IDLE:
	case LS_ONWATER_FORWARD:
	case LS_ONWATER_LEFT:
	case LS_ONWATER_RIGHT:
	case LS_ONWATER_BACK:
		rate = TURN_RATE_SURFACE;
		stepMax = TURN_STEP_MAX;
		return true;

	default:
		return false;
	}
}

// True if the heading solver turns the player in the current water state.
bool DoesRemasteredSolverOwnWaterHeading(const ItemInfo& item)
{
	if (!g_Configuration.IsUsingRemasteredControls())
		return false;

	float rate = 0.0f;
	short stepMax = 0;
	bool isUnderwater = false;
	return GetPlayerRemasteredWaterTurnParams(item, rate, stepMax, isUnderwater);
}

// Heading the stick points to, relative to the camera. The player's own heading when the stick is centered.
short GetPlayerHeadingAngleY(const ItemInfo& item)
{
	if (!g_Configuration.IsUsingRemasteredControls())
		return item.Pose.Orientation.y;

	const auto& player = GetLaraInfo(item);
	auto dir = player.Control.RefMoveAxis;
	if (dir == Vector2::Zero)
		return item.Pose.Orientation.y;

	dir.Normalize();
	short moveAxisAngle = FROM_RAD(std::atan2(dir.x, dir.y));
	return (short)(player.Control.RefCameraOrient.y + moveAxisAngle);
}

// A diagonal step or wall met within 45 degrees of square-on resets the side collision, so the player
// moves on until the front probe stops her instead of being deflected every tick. Returns true if reset.
bool HandlePlayerRemasteredDiagonalStep(const ItemInfo& item, CollisionInfo& coll)
{
	if (!g_Configuration.IsUsingRemasteredControls())
		return false;

	switch (item.Animation.ActiveState)
	{
	case LS_WALK_FORWARD:
	case LS_RUN_FORWARD:
	case LS_SPRINT:
	case LS_WADE_FORWARD:
		break;

	default:
		return false;
	}

	bool isLeft = (coll.CollisionType == CollisionType::Left);
	if (!isLeft && coll.CollisionType != CollisionType::Right)
		return false;

	auto& sidePos = isLeft ? coll.MiddleLeft : coll.MiddleRight;
	bool isTriangle = isLeft ? coll.TriangleAtLeft() : coll.TriangleAtRight();
	if (!isTriangle || !sidePos.HasDiagonalSplit())
		return false;

	int quarter = ((unsigned short)coll.Setup.ForwardAngle / ANGLE(90.0f)) % 2;
	bool isFacingStep = (sidePos.HasFlippedDiagonalSplit() != (quarter != 0));
	if (!isFacingStep)
		return false;

	coll.CollisionType = CollisionType::None;
	return true;
}

// Safety grab probe: taken behind the player, its base 381 units below her feet. The lip must be level
// across her within 78 units and square to the grid within 35 degrees.
constexpr auto SAFETY_GRAB_PROBE_HEADING_OFFSET	 = (short)0x7ff8;
constexpr auto SAFETY_GRAB_PROBE_VERTICAL_OFFSET = 0x17d;
constexpr auto SAFETY_GRAB_LIP_TOLERANCE		 = 0x4e;
constexpr auto SAFETY_GRAB_ANGLE_TOLERANCE		 = ANGLE(35.0f);

static bool IsAngleNearCardinal(short angle, short toleranceAngle)
{
	short nearestCardinalAngle = (short)(GetQuadrant(angle) * ANGLE(90.0f));
	return (abs((int)Geometry::GetShortestAngle(angle, nearestCardinalAngle)) <= toleranceAngle);
}

// Angle of the diagonal lip under the player, and her distance to it. False if the sector has no diagonal split.
static bool GetRemasteredDiagonalLip(const ItemInfo& item, short& lipAngle, float& lipDistance)
{
	auto pointColl = GetPointCollision(item, 0, 0, -LARA_HEIGHT / 2);
	const auto& sector = pointColl.GetSector();

	short splitAngle = sector.FloorSurface.SplitAngle;
	if (!sector.IsSurfaceSplit(true) ||
		(splitAngle != SectorSurfaceData::SPLIT_ANGLE_0 && splitAngle != SectorSurfaceData::SPLIT_ANGLE_1))
	{
		return false;
	}

	int triID = sector.GetSurfaceTriangleID(item.Pose.Position.x, item.Pose.Position.z, true);
	lipAngle = (short)(splitAngle + ANGLE(triID * 180.0f) + ANGLE(90.0f));

	auto center = GetNearestSectorCenter(item.Pose.Position);
	lipDistance = ((center.x - item.Pose.Position.x) * phd_sin(lipAngle)) +
				  ((center.z - item.Pose.Position.z) * phd_cos(lipAngle));
	return true;
}

// A diagonal lip is accepted when it is level across the player and she faces it within the tolerance.
static bool TestRemasteredSafetyGrabDiagonal(ItemInfo& item, const CollisionInfo& probe, short& turnAngle)
{
	short lipAngle = 0;
	float lipDistance = 0.0f;
	if (!GetRemasteredDiagonalLip(item, lipAngle, lipDistance))
		return false;

	auto lipProbe = probe;
	lipProbe.Setup.Mode = CollisionProbeMode::FreeFlat;
	lipProbe.Setup.ForwardAngle = lipAngle;
	GetCollisionInfo(&lipProbe, &item, Vector3i(0, SAFETY_GRAB_PROBE_VERTICAL_OFFSET, 0));

	bool isLipLevel = (abs(lipProbe.FrontLeft.Floor - lipProbe.FrontRight.Floor) < SAFETY_GRAB_LIP_TOLERANCE);
	bool isFacingLip = (abs((int)Geometry::GetShortestAngle(probe.Setup.ForwardAngle, lipAngle)) <= SAFETY_GRAB_ANGLE_TOLERANCE);
	if (!isLipLevel || !isFacingLip)
		return false;

	turnAngle = lipAngle;
	return true;
}

// Moves the player back from a diagonal lip to the reach radius, if the floor and ceiling there allow it.
static void ApplyRemasteredSafetyGrabStandOff(ItemInfo& item)
{
	constexpr auto STAND_OFF_DISTANCE = LARA_RADIUS * 1.2f;

	short lipAngle = 0;
	float lipDistance = 0.0f;
	if (!GetRemasteredDiagonalLip(item, lipAngle, lipDistance))
		return;

	short pushAngle = (short)(lipAngle + ANGLE(180.0f));
	float pushDistance = STAND_OFF_DISTANCE - lipDistance;
	if (pushDistance <= 0.0f)
		return;

	auto pointColl = GetPointCollision(item, pushAngle, pushDistance, -LARA_HEIGHT / 2);
	int floorHeight = pointColl.GetFloorHeight();
	if (floorHeight == NO_HEIGHT || floorHeight <= item.Pose.Position.y)
		return;

	if (pointColl.GetCeilingHeight() > (item.Pose.Position.y - LARA_HEIGHT))
		return;

	item.Pose.Translate(pushAngle, pushDistance);
}

// Safety grab: running off a drop of about a block with the grab button held puts the player into a reach
// facing the lip she ran off, and the stock reach code catches the ledge.
bool TestRemasteredSafetyGrab(ItemInfo& item, CollisionInfo& coll)
{
	constexpr auto DROP_HEIGHT_MIN		   = BLOCK(1);
	constexpr auto DROP_HEIGHT_TOLERANCE   = CLICK(0.25f);
	constexpr auto PROBE_RADIUS_SCALE	   = 1.3f;
	constexpr auto PROBE_FLOOR_BOUND_LOWER = 0x7f00;
	constexpr auto PROBE_FLOOR_BOUND_UPPER = -0x180;
	constexpr auto PROBE_HEIGHT			   = 0x2fa;
	constexpr auto WATER_DEPTH_MIN		   = 0x1ff;
	constexpr auto CAUGHT_VELOCITY		   = 10.0f;

	if (!g_Configuration.IsUsingRemasteredControls())
		return false;

	auto& player = GetLaraInfo(item);

	if (item.Animation.ActiveState != LS_RUN_FORWARD)
		return false;

	if (coll.Middle.Floor < (DROP_HEIGHT_MIN - DROP_HEIGHT_TOLERANCE))
		return false;

	if (!IsRemasteredGrabHeld())
		return false;

	if (player.Control.HandStatus != HandStatus::Free)
		return false;

	auto probe = coll;
	probe.Setup.Radius = (int)(coll.Setup.Radius * PROBE_RADIUS_SCALE);
	probe.Setup.LowerFloorBound = PROBE_FLOOR_BOUND_LOWER;
	probe.Setup.UpperFloorBound = PROBE_FLOOR_BOUND_UPPER;
	probe.Setup.LowerCeilingBound = 0;
	probe.Setup.Height = PROBE_HEIGHT;
	probe.Setup.ForwardAngle = (short)(item.Pose.Orientation.y + SAFETY_GRAB_PROBE_HEADING_OFFSET);
	GetCollisionInfo(&probe, &item, Vector3i(0, SAFETY_GRAB_PROBE_VERTICAL_OFFSET, 0));

	bool isLipLevel = (abs(probe.FrontLeft.Floor - probe.FrontRight.Floor) < SAFETY_GRAB_LIP_TOLERANCE);
	bool isSquareOn = IsAngleNearCardinal(probe.Setup.ForwardAngle, SAFETY_GRAB_ANGLE_TOLERANCE);

	short turnAngle = (short)(GetQuadrant(probe.Setup.ForwardAngle) * ANGLE(90.0f));
	bool isDiagonalLip = (!isLipLevel || !isSquareOn);
	if (isDiagonalLip && !TestRemasteredSafetyGrabDiagonal(item, probe, turnAngle))
		return false;

	int waterHeight = GetPointCollision(item).GetWaterSurfaceHeight();
	if (waterHeight != NO_HEIGHT && (waterHeight - item.Pose.Position.y) < WATER_DEPTH_MIN)
		return false;

	player.Control.RefMoveAxis = Vector2::Zero;
	player.Control.Remastered.SafetyGrabTurnAngle = turnAngle;
	player.Control.Remastered.SafetyGrabTurnYaw = item.Pose.Orientation.y;
	player.Control.Remastered.IsSafetyGrabTurning = true;

	if (isDiagonalLip)
		ApplyRemasteredSafetyGrabStandOff(item);

	SetAnimation(item, LA_REACH);
	item.Animation.IsAirborne = true;
	item.Animation.ActiveState = LS_REACH;
	item.Animation.TargetState = LS_REACH;
	item.Animation.Velocity.z = CAUGHT_VELOCITY;
	item.Animation.Velocity.y = 0.0f;
	return true;
}

// Tightrope idle: a held push back turns the player once, and held on after the turn she walks.
// Forward releases the turn latch. False leaves the state to the stock checks.
bool HandleRemasteredTightropeIdle(ItemInfo& item)
{
	auto& remastered = GetLaraInfo(item).Control.Remastered;

	if (IsHeld(In::Forward))
	{
		remastered.IsTightropeTurnLatched = false;
		return false;
	}

	if (IsHeld(In::Back) && !IsHeld(In::Roll))
	{
		if (remastered.IsTightropeTurnLatched)
		{
			item.Animation.TargetState = LS_TIGHTROPE_WALK;
			return true;
		}

		remastered.IsTightropeTurnLatched = true;
		item.Animation.TargetState = LS_TIGHTROPE_TURN_180;
		return true;
	}

	if (!IsHeld(In::Back))
		remastered.IsTightropeTurnLatched = false;

	return false;
}

// A script clear of Sprint, Look or Crouch resets its toggle.
static void ResetPlayerRemasteredClearedLatches(ItemInfo& item)
{
	auto& player = GetLaraInfo(item);

	if (IsCleared(In::Sprint))
		player.Control.Remastered.IsSprintToggled = false;

	if (IsCleared(In::Look))
		player.Control.Remastered.IsLookToggled = false;

	if (IsCleared(In::Crouch))
	{
		player.Control.Remastered.CrouchStage = 0;
		player.Control.Remastered.IsCrouchStageUpPending = false;
	}
}

static void ResetRemasteredArmedTurn(ItemInfo& item)
{
	auto& remastered = GetLaraInfo(item).Control.Remastered;
	remastered.IsArmedTurning = false;
	remastered.ArmedTurnErrorY = 0;
}

// A script clear holds the action off, even where the rewrite would set it.
static void MaskRemasteredClearedInput(ItemInfo& item, PlayerInput& input)
{
	ApplyClearedActions(input);

	// A script clear of the turn key the armed turn pressed stops the armed turn.
	auto& player = GetLaraInfo(item);
	if ((IsCleared(In::Left) || IsCleared(In::Right)) &&
		player.Control.Remastered.IsArmedTurning && !(input.Left || input.Right))
	{
		player.Control.TurnRate = 0;
		ResetRemasteredArmedTurn(item);
	}
}

// Sprint toggle: a press while running latches the sprint, the next press or leaving the run releases it.
static void UpdatePlayerRemasteredSprintToggle(ItemInfo& item, PlayerInput& input)
{
	auto& remastered = GetLaraInfo(item).Control.Remastered;
	bool isRunning = (item.Animation.ActiveState == LS_RUN_FORWARD || item.Animation.ActiveState == LS_SPRINT);

	if (!g_Configuration.EnableSprintToggle)
	{
		remastered.IsSprintToggled = false;
	}
	else if (IsClicked(In::Sprint) && !IsPushed(In::Sprint))
	{
		if (remastered.IsSprintToggled)
		{
			remastered.IsSprintToggled = false;
		}
		else if (isRunning)
		{
			remastered.IsSprintToggled = true;
		}
	}

	if (!isRunning)
		remastered.IsSprintToggled = false;

	if (g_Configuration.EnableSprintToggle && isRunning)
		input.Sprint = (remastered.IsSprintToggled || IsPushed(In::Sprint));
}

// Look toggle: a press latches Look and the next press releases it. The latched Look is written held, never clicked.
// Off while locked on a target, where each press switches target, and in states that cannot look.
static void UpdateRemasteredLookToggle(ItemInfo& item, PlayerInput& input, PlayerInput& heldWithoutClickInput)
{
	auto& player = GetLaraInfo(item);
	auto& remastered = player.Control.Remastered;

	if (!g_Configuration.EnableLookToggle || IsPlayerLockedOnTarget(item) ||
		player.Control.Look.Mode == LookMode::None)
	{
		remastered.IsLookToggled = false;
		return;
	}

	if (IsClicked(In::Look) && !IsPushed(In::Look))
		remastered.IsLookToggled = !remastered.IsLookToggled;

	input.Look = (remastered.IsLookToggled || IsPushed(In::Look));
	heldWithoutClickInput.Look = true;
}

// Crouch stages: standing, crouched, on knees. A press on knees rises one stage to the crouch.
static void UpdatePlayerRemasteredCrouchStages(ItemInfo& item, PlayerInput& input)
{
	auto& player = GetLaraInfo(item);
	auto& remastered = player.Control.Remastered;

	if (!g_Configuration.EnableCrouchToggle)
	{
		remastered.CrouchStage = input.Crouch ? 1 : 0;
		remastered.IsCrouchStageUpPending = false;
	}
	else if (IsClicked(In::Crouch) && !IsPushed(In::Crouch))
	{
		if (remastered.CrouchStage == 0)
		{
			remastered.CrouchStage = 1;
		}
		else if (IsPlayerRemasteredOnKnees(item))
		{
			remastered.IsCrouchStageUpPending = true;
		}
		else
		{
			remastered.CrouchStage = 0;
		}
	}

	if (remastered.IsCrouchStageUpPending)
	{
		if (IsPlayerRemasteredOnKnees(item))
		{
			remastered.IsCrouchStageUp = true;
			input.Forward = false;
			input.Back = false;
		}
		else
		{
			remastered.IsCrouchStageUpPending = false;
		}
	}

	if (player.Control.WaterStatus == WaterStatus::Underwater ||
		player.Control.WaterStatus == WaterStatus::TreadWater ||
		DoesRemasteredInputOwnGrip(item))
	{
		remastered.CrouchStage = 0;
		remastered.IsCrouchStageUpPending = false;
	}

	input.Crouch = ((remastered.CrouchStage != 0) || IsPushed(In::Crouch));
}

// True if the stick is pushed firmly along one axis toward the player's back (forward jump)
// or front (back jump). A light push, Look or an already spent push does not roll the player.
// TODO: Roll the player in a left or right running jump when the stick is pushed the opposite way. -- divezzi, 11.10.26
static bool IsPlayerRemasteredJumpRollPushed(const ItemInfo& item, const PlayerInput& input)
{
	constexpr auto PUSH_MIN		  = 0.7f; // About half the stick's travel. Keys and the D-pad give 1.0.
	constexpr auto ROLL_ARC_ANGLE = ANGLE(45.0f);

	const auto& player = GetLaraInfo(item);

	bool isForwardJump = (item.Animation.ActiveState == LS_JUMP_FORWARD);
	if (!isForwardJump && item.Animation.ActiveState != LS_JUMP_BACK)
		return false;

	if (input.Look || player.Control.Remastered.IsJumpRollSpent)
		return false;

	bool isVertical = (input.Forward || input.Back);
	bool isHorizontal = (input.Left || input.Right);
	if (isVertical == isHorizontal)
		return false;

	const auto& moveAxis = player.Control.RefMoveAxis;
	if (abs(isVertical ? moveAxis.y : moveAxis.x) < PUSH_MIN)
		return false;

	short rollAngle = isForwardJump ? (short)(item.Pose.Orientation.y + ANGLE(180.0f)) : item.Pose.Orientation.y;
	short deltaAngle = Geometry::GetShortestAngle(rollAngle, GetPlayerHeadingAngleY(item));
	return (abs((int)deltaAngle) <= ROLL_ARC_ANGLE);
}

// The push that made a roll is spent until the stick is released or the jump ends.
static void UpdatePlayerRemasteredJumpRollLatch(ItemInfo& item)
{
	auto& player = GetLaraInfo(item);

	switch (item.Animation.ActiveState)
	{
	case LS_JUMP_ROLL_180:
		player.Control.Remastered.IsJumpRollSpent = true;
		break;

	case LS_JUMP_FORWARD:
	case LS_JUMP_BACK:
		if (player.Control.RefMoveAxis == Vector2::Zero)
			player.Control.Remastered.IsJumpRollSpent = false;

		break;

	default:
		player.Control.Remastered.IsJumpRollSpent = false;
		break;
	}
}

// Presses the turn key toward a target heading while the error is at least minAngle, so the stock turn states turn
// the player. With the stick centered while turning, Walk and the sidesteps are released.
static void SynthesizeRemasteredTurnActions(ItemInfo& item, PlayerInput& input, short toAngle, short minAngle)
{
	auto& player = GetLaraInfo(item);
	auto& remastered = player.Control.Remastered;

	short deltaAngle = Geometry::GetShortestAngle(item.Pose.Orientation.y, toAngle);

	input.Left = false;
	input.Right = false;

	if (abs((int)deltaAngle) >= (int)minAngle)
	{
		if (deltaAngle > 0)
		{
			input.Right = true;
		}
		else
		{
			input.Left = true;
		}
	}

	if (!(input.Left || input.Right) && remastered.IsArmedTurning)
		player.Control.TurnRate = 0;

	remastered.IsArmedTurning = (input.Left || input.Right);
	remastered.ArmedTurnErrorY = remastered.IsArmedTurning ? deltaAngle : (short)0;

	if (remastered.IsArmedTurning && player.Control.RefMoveAxis == Vector2::Zero)
	{
		input.Walk = false;
		input.StepLeft = false;
		input.StepRight = false;
	}
}

// Rewrites the movement actions so the stick moves the player relative to the camera.
// On the ground the heading solver turns her, so a push becomes Forward, and a push toward the camera a fast 180.
static void TransformPlayerRemasteredInput(ItemInfo& item, PlayerInput& input, PlayerInput& heldWithoutClickInput)
{
	constexpr auto BACK_ARC_MIN_ANGLE	= ANGLE(135.0f);
	constexpr auto CRAWL_BACK_ARC_ANGLE = ANGLE(45.0f);
	constexpr auto ARMED_TURN_MIN_ANGLE = ANGLE(1.0f);

	auto& player = GetLaraInfo(item);

	ResetPlayerRemasteredClearedLatches(item);
	UpdatePlayerRemasteredSprintToggle(item, input);
	UpdateRemasteredLookToggle(item, input, heldWithoutClickInput);
	UpdatePlayerRemasteredCrouchStages(item, input);
	player.Control.RefMoveAxis = GetMoveAxis();
	UpdatePlayerRemasteredJumpRollLatch(item);

	// Water: the solver turns the player, so the turn keys, side strokes and back stroke are dropped.
	// The stick is held as Forward and, underwater, also as Jump, the stroke; there the Jump key alone no longer swims,
	// but a script push of Jump still does. At the surface Jump is left alone, where a Jump press is the dive.
	float waterRate = 0.0f;
	short waterStepMax = 0;
	bool isUnderwater = false;
	if (GetPlayerRemasteredWaterTurnParams(item, waterRate, waterStepMax, isUnderwater))
	{
		bool isPushingStick = (player.Control.RefMoveAxis != Vector2::Zero);

		input.Left = false;
		input.Right = false;
		input.StepLeft = false;
		input.StepRight = false;
		input.Back = false;
		input.Forward = isPushingStick;

		// A script clear of Forward stops the stroke. Jump is written as held, never clicked, so a resurface in the same tick does not dive.
		if (isUnderwater)
		{
			input.Jump = ((isPushingStick && !IsCleared(In::Forward)) || IsPushed(In::Jump));
			heldWithoutClickInput.Jump = true;
		}

		return;
	}

	// Hanging states: the grip holds until Roll, or until the grab button is released when the grab toggle is off.
	// Roll is consumed so it does not also ask for a turn, except on the rope, where it is the drop.
	if (DoesRemasteredInputOwnGrip(item))
	{
		bool isFireCleared = (IsCleared(In::Fire) && !IsPushed(In::Action));
		input.Action = (!isFireCleared && GetRemasteredGrip(IsRemasteredGrabHeld(), input.Roll));

		if (item.Animation.ActiveState != LS_ROPE_UP)
			input.Roll = false;

		if (input.Forward && input.Back)
			input.Back = false;
	}

	if (!IsPlayerRemasteredDirectionPushed() && !IsPlayerInRemasteredCombatBackStance(item) &&
		(item.Animation.ActiveState == LS_WALK_BACK || item.Animation.ActiveState == LS_RUN_BACK))
	{
		input.Back = false;
	}

	if (IsPlayerRemasteredDirectionPushed())
	{
		ResetRemasteredArmedTurn(item);
		return;
	}

	// Stick centered: with weapons ready the player turns to face the camera direction, otherwise she stops turning.
	if (player.Control.RefMoveAxis == Vector2::Zero)
	{
		float armedRate = 0.0f;
		short armedStepMax = 0;
		if (!input.Look && !input.Jump &&
			(Camera.type == CameraType::Chase || Camera.type == CameraType::Combat) &&
			GetPlayerRemasteredTurnParams(item, armedRate, armedStepMax) == RemasteredInputScope::ArmedSynthesis)
		{
			SynthesizeRemasteredTurnActions(item, input, player.Control.RefCameraOrient.y, ARMED_TURN_MIN_ANGLE);
		}
		else
		{
			ResetRemasteredArmedTurn(item);
			player.Control.TurnRate = 0;
		}

		return;
	}

	float turnRate = 0.0f;
	short turnStepMax = 0;
	auto scope = GetPlayerRemasteredTurnParams(item, turnRate, turnStepMax);
	if (scope == RemasteredInputScope::None)
		return;

	ResetRemasteredArmedTurn(item);

	// Mid-air: no tank turn. Back and Forward roll the player only on the roll push.
	// Otherwise Forward is held so the landing chains into a run, except in the back jump.
	if (scope == RemasteredInputScope::Airborne || scope == RemasteredInputScope::AirborneNoSolve)
	{
		bool isRollPushed = IsPlayerRemasteredJumpRollPushed(item, input);
		bool isBackJump = (item.Animation.ActiveState == LS_JUMP_BACK);

		if (isRollPushed)
			player.Control.Remastered.BackResolution = RemasteredBackResolution::Roll;

		input.Left = false;
		input.Right = false;
		input.Back = (isRollPushed && !isBackJump);
		input.Forward = (isRollPushed || !isBackJump);
		return;
	}

	auto dir = player.Control.RefMoveAxis;
	dir.Normalize();
	short moveAxisAngle = FROM_RAD(std::atan2(dir.x, dir.y));
	bool isPushingBack = (abs((int)moveAxisAngle) >= BACK_ARC_MIN_ANGLE);

	// Walk with a push toward the camera on knees, facing away from it: the stock crawl back plays.
	if (input.Walk && isPushingBack &&
		(item.Animation.ActiveState == LS_CRAWL_IDLE ||
		 item.Animation.ActiveState == LS_CRAWL_FORWARD ||
		 item.Animation.ActiveState == LS_CRAWL_BACK))
	{
		short frameDeltaAngle = Geometry::GetShortestAngle(item.Pose.Orientation.y, player.Control.RefCameraOrient.y);
		if (abs((int)frameDeltaAngle) <= CRAWL_BACK_ARC_ANGLE)
		{
			player.Control.Remastered.BackResolution = RemasteredBackResolution::TankBack;
			input.Left = false;
			input.Right = false;
			return;
		}
	}

	// Weapons drawn, standing: a pure push toward the camera steps the player back without turning her.
	bool isBackAlone = (input.Back && !input.Left && !input.Right);
	if (isBackAlone && IsPlayerInRemasteredCombatBackStance(item) && IsRemasteredStandingState(item))
	{
		player.Control.Remastered.BackResolution = RemasteredBackResolution::TankBack;
		input.Forward = false;
		input.Left = false;
		input.Right = false;
		return;
	}

	// Weapons ready, standing: the turn keys turn the player toward the stick, and Forward is pressed so she moves.
	if (scope == RemasteredInputScope::ArmedSynthesis)
	{
		input.Left = false;
		input.Right = false;

		if (input.Look)
			return;

		if (input.Jump)
			return;

		SynthesizeRemasteredTurnActions(item, input, GetPlayerHeadingAngleY(item), ARMED_TURN_MIN_ANGLE);
		input.Forward = true;
		input.Back = false;
		return;
	}

	input.Left = false;
	input.Right = false;
	input.StepLeft = false;
	input.StepRight = false;

	if (isPushingBack)
		player.Control.Remastered.BackResolution = RemasteredBackResolution::Turn180;

	input.Back = false;
	input.Forward = true;
}

void ApplyRemasteredInput(ItemInfo& item)
{
	auto& player = GetLaraInfo(item);

	// Under Classic controls all Remastered state is cleared, so the toggles are off on a switch back.
	if (!g_Configuration.IsUsingRemasteredControls())
	{
		player.Control.Remastered = {};
		return;
	}

	player.Control.Remastered.BackResolution = RemasteredBackResolution::None;
	player.Control.Remastered.IsCrouchStageUp = false;

	if (player.Context.Vehicle != NO_VALUE)
		return;

	auto input = ReadPlayerInput();
	auto heldWithoutClickInput = PlayerInput{};
	TransformPlayerRemasteredInput(item, input, heldWithoutClickInput);
	MaskRemasteredClearedInput(item, input);
	WritePlayerInput(input, heldWithoutClickInput);
}

// Turns the player toward the lip during the safety grab reach. The turn steps from its own last yaw,
// so a side deflection added by the stock reach code in between is discarded. True while it turns.
static bool HandleRemasteredSafetyGrabTurn(ItemInfo& item)
{
	constexpr auto TURN_RATE	 = 1.0f;
	constexpr auto TURN_STEP_MAX = ANGLE(60.0f);

	auto& player = GetLaraInfo(item);
	auto& remastered = player.Control.Remastered;

	if (!remastered.IsSafetyGrabTurning)
		return false;

	if (!item.Animation.IsAirborne ||
		(item.Animation.ActiveState != LS_REACH && item.Animation.ActiveState != LS_FREEFALL))
	{
		remastered.IsSafetyGrabTurning = false;
		return false;
	}

	item.Pose.Orientation.y = ApproachHeadingAngle(remastered.SafetyGrabTurnYaw, remastered.SafetyGrabTurnAngle, TURN_RATE, TURN_STEP_MAX, remastered.HeadingTurnSign);
	remastered.SafetyGrabTurnYaw = item.Pose.Orientation.y;

	player.Control.MoveAngle = item.Pose.Orientation.y;
	player.Control.TurnRate = 0;

	if (item.Pose.Orientation.y == remastered.SafetyGrabTurnAngle)
		remastered.IsSafetyGrabTurning = false;

	return true;
}

// Turns the player toward the heading the stick points to, at the turn rate of her current state.
void HandlePlayerRemasteredHeading(ItemInfo& item)
{
	constexpr auto TURN_RATE_180	 = 1.0f;
	constexpr auto TURN_STEP_MAX_180 = ANGLE(90.0f);

	if (!g_Configuration.IsUsingRemasteredControls())
		return;

	auto& player = GetLaraInfo(item);

	if (HandleRemasteredSafetyGrabTurn(item))
		return;

	if (!DoesPlayerRemasteredSolverOwnHeading(item))
		return;

	player.Control.TurnRate = 0;

	// No turn on a tank back step or on a mid-air roll push.
	if (player.Control.Remastered.BackResolution == RemasteredBackResolution::TankBack ||
		player.Control.Remastered.BackResolution == RemasteredBackResolution::Roll)
	{
		return;
	}

	if (player.Control.IsMoving)
		return;

	if (player.Control.RefMoveAxis == Vector2::Zero)
		return;

	float rate = 0.0f;
	short stepMax = 0;
	GetPlayerRemasteredTurnParams(item, rate, stepMax);

	if (player.Control.Remastered.BackResolution == RemasteredBackResolution::Turn180)
	{
		rate = TURN_RATE_180;
		stepMax = TURN_STEP_MAX_180;
	}

	if (IsHeld(In::Look))
		return;

	short headingAngle = GetPlayerHeadingAngleY(item);
	item.Pose.Orientation.y = ApproachHeadingAngle(item.Pose.Orientation.y, headingAngle, rate, stepMax, player.Control.Remastered.HeadingTurnSign);
}

// Water: turns the player toward the heading the stick points to and, underwater, pitches her toward the camera elevation.
// Zeroes the classic turn rate in the water states it steers. Stands down while she aligns, looks, or the stick is centered.
// Underwater it also holds the roll at zero.
void HandleRemasteredWaterHeading(ItemInfo& item)
{
	constexpr auto PITCH_RATE		= 1.0f;
	constexpr auto PITCH_STEP_MAX	= ANGLE(11.25f);

	if (!g_Configuration.IsUsingRemasteredControls())
		return;

	auto& player = GetLaraInfo(item);

	float rate = 0.0f;
	short stepMax = 0;
	bool isUnderwater = false;
	if (!GetPlayerRemasteredWaterTurnParams(item, rate, stepMax, isUnderwater))
		return;

	player.Control.TurnRate = 0;

	if (player.Control.IsMoving)
		return;

	if (IsHeld(In::Look))
		return;

	if (player.Control.RefMoveAxis == Vector2::Zero)
		return;

	auto& remastered = player.Control.Remastered;
	short headingAngle = GetPlayerHeadingAngleY(item);
	item.Pose.Orientation.y = ApproachHeadingAngle(item.Pose.Orientation.y, headingAngle, rate, stepMax, remastered.HeadingTurnSign);

	// Surface swimming stays level.
	if (!isUnderwater)
		return;

	// A positive camera elevation puts the camera below the player, looking up, so she pitches up.
	int pitchTurnSign = 0;
	item.Pose.Orientation.x = ApproachHeadingAngle(item.Pose.Orientation.x, player.Control.RefCameraOrient.x, PITCH_RATE, PITCH_STEP_MAX, pitchTurnSign);
	item.Pose.Orientation.z = 0;
}

void ModulateLaraSwimTurnRates(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	// Remastered controls: the stick steers in water, so the Forward it holds does not pitch the player down here. -- divezzi, 12.10.26
	if (DoesRemasteredSolverOwnWaterHeading(*item))
		return;

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

	if (lara->Control.TurnRate > 0)
		lara->Control.Subsuit.Velocity[0] += 2 * abs(lara->Control.TurnRate);
	else if (lara->Control.TurnRate < 0)
		lara->Control.Subsuit.Velocity[1] += 2 * abs(lara->Control.TurnRate);

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
	auto floorNormal = GetPointCollision(*item).GetFloorNormal();
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
	SetAnimation(*item, LA_FALL_START, 0, GetInternalBlendDuration(), BezierCurve2::EaseOut);
	item->Animation.IsAirborne = true;
	item->Animation.Velocity.y = 0.0f;
}

void SetLaraFallBackAnimation(ItemInfo* item)
{
	SetAnimation(*item, LA_FALL_BACK, 0, GetInternalBlendDuration(), BezierCurve2::EaseOut);
	item->Animation.IsAirborne = true;
	item->Animation.Velocity.y = 0.0f;
}

void SetLaraMonkeyFallAnimation(ItemInfo* item)
{
	// HACK: Disallow release during 180 turn action.
	if (item->Animation.ActiveState == LS_MONKEY_TURN_180)
		return;

	SetAnimation(*item, LA_MONKEY_TO_FREEFALL, 0, GetInternalBlendDuration(), BezierCurve2::EaseOut);
	SetLaraMonkeyRelease(item);
}

void SetLaraMonkeyRelease(ItemInfo* item)
{
	auto* lara = GetLaraInfo(item);

	item->Animation.IsAirborne = true;
	item->Animation.Velocity.y = 1.0f;
	item->Animation.Velocity.z = 2.0f;
	lara->Control.TurnRate = 0;
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

		SetAnimation(*item, LA_SLIDE_BACK_START, 0, GetInternalBlendDuration(), BezierCurve2::EaseOut);
		item->Pose.Orientation.y = angle + ANGLE(180.0f);
	}
	else
	{
		if (item->Animation.ActiveState == LS_SLIDE_FORWARD && oldAngle == angle)
			return;

		SetAnimation(*item, LA_SLIDE_FORWARD, 0, GetInternalBlendDuration(), BezierCurve2::EaseOut);
		item->Pose.Orientation.y = angle;
	}

	LaraSnapToHeight(item, coll);
	lara->Control.MoveAngle = angle;
	lara->Control.TurnRate = 0;
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

		SetAnimation(*item, LA_SLIDE_FORWARD, 0, GetInternalBlendDuration(), BezierCurve2::EaseOut);
	}
	// Slide backward.
	else
	{
		if (item->Animation.ActiveState == LS_SLIDE_BACK && abs((short)(deltaAngle - ANGLE(180.0f))) <= -ANGLE(180.0f))
			return;

		SetAnimation(*item, LA_SLIDE_BACK_START, 0, GetInternalBlendDuration(), BezierCurve2::EaseOut);
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

	SetAnimation(*item, LA_ONWATER_DIVE, 0, GetInternalBlendDuration(), BezierCurve2::EaseInOut);
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
		auto previousVehicle = lara->Context.Vehicle;

		if (previousVehicle == NO_VALUE)
			return;

		g_GameScript->OnVehicleLeave(previousVehicle, false);

		auto vehicleObjectNumber = g_Level.Items[previousVehicle].ObjectNumber;
		if (vehicleObjectNumber != ID_SPEEDBOAT && vehicleObjectNumber != ID_RUBBER_BOAT) // Leave boat vehicles active for inertia.
			g_Level.Items[previousVehicle].Active = false;

		lara->Context.Vehicle = NO_VALUE;
		g_GameScript->OnVehicleLeave(previousVehicle, true);
	}
	else
	{
		if (vehicle->ObjectNumber != ID_SPEEDBOAT && vehicle->ObjectNumber != ID_RUBBER_BOAT) // Boats are activated elsewhere.
			g_Level.Items[vehicle->Index].Active = true;

		lara->Context.Vehicle = vehicle->Index;
		g_GameScript->OnVehicleEnter(vehicle->Index, true);
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

	player.ExtraHeadRot.Lerp(EulerAngles::Identity, alpha);
	player.ExtraTorsoRot.Lerp(EulerAngles::Identity, alpha);
}

void ResetPlayerLookAround(ItemInfo& item, float alpha)
{
	auto& player = GetLaraInfo(item);

	player.Control.Look.Orientation = EulerAngles::Identity;

	if (Camera.type != CameraType::Look)
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

	if (item->HitPoints <= 0)
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
