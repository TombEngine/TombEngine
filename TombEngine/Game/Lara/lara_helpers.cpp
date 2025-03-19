#include "framework.h"
#include "Game/Lara/lara_helpers.h"

#include <OISKeyboard.h>

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

	// Handle weapon requests.
	if (IsClicked(In::Weapon1) && player.Weapons[(int)LaraWeaponType::Pistol].Present)
		player.Control.Weapon.RequestGunType = LaraWeaponType::Pistol;

	if (IsClicked(In::Weapon2) && player.Weapons[(int)LaraWeaponType::Shotgun].Present)
		player.Control.Weapon.RequestGunType = LaraWeaponType::Shotgun;

	if (IsClicked(In::Weapon3) && player.Weapons[(int)LaraWeaponType::Uzi].Present)
		player.Control.Weapon.RequestGunType = LaraWeaponType::Uzi;

	if (IsClicked(In::Weapon4) && player.Weapons[(int)LaraWeaponType::Revolver].Present)
		player.Control.Weapon.RequestGunType = LaraWeaponType::Revolver;

	if (IsClicked(In::Weapon5) && player.Weapons[(int)LaraWeaponType::GrenadeLauncher].Present)
		player.Control.Weapon.RequestGunType = LaraWeaponType::GrenadeLauncher;

	if (IsClicked(In::Weapon6) && player.Weapons[(int)LaraWeaponType::Crossbow].Present)
		player.Control.Weapon.RequestGunType = LaraWeaponType::Crossbow;

	if (IsClicked(In::Weapon7) && player.Weapons[(int)LaraWeaponType::HarpoonGun].Present)
		player.Control.Weapon.RequestGunType = LaraWeaponType::HarpoonGun;

	if (IsClicked(In::Weapon8) && player.Weapons[(int)LaraWeaponType::HK].Present)
		player.Control.Weapon.RequestGunType = LaraWeaponType::HK;

	if (IsClicked(In::Weapon9) && player.Weapons[(int)LaraWeaponType::RocketLauncher].Present)
		player.Control.Weapon.RequestGunType = LaraWeaponType::RocketLauncher;

	// TODO: 10th possible weapon, probably grapple gun.
	/*if (IsClicked(In::Weapon10) && player.Weapons[(int)LaraWeaponType::].Present)
		player.Control.Weapon.RequestGunType = LaraWeaponType::;*/
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

	// Determine X axis coefficient.
	if ((IsHeld(In::Forward) || IsHeld(In::Back)) &&
		(player.Control.Look.Mode == LookMode::Free || player.Control.Look.Mode == LookMode::Vertical))
	{
		axisCoeff.x = AxisMap[InputAxisID::Move].y;
	}

	// Determine Y axis coefficient.
	if ((IsHeld(In::Left) || IsHeld(In::Right)) &&
		(player.Control.Look.Mode == LookMode::Free || player.Control.Look.Mode == LookMode::Horizontal))
	{
		axisCoeff.y = AxisMap[InputAxisID::Move].x;
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
		SetAnimation(*item, LA_FALL_START);
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

	float axisCoeff = AxisMap[InputAxisID::Move].x;
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

	float axisCoeff = AxisMap[InputAxisID::Move].x;
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

	if (item->Animation.Velocity.y >= LARA_DAMAGE_VELOCITY)
	{
		if (item->Animation.Velocity.y >= LARA_DEATH_VELOCITY)
		{
			item->HitPoints = 0;
		}
		else
		{
			float base = item->Animation.Velocity.y - (LARA_DAMAGE_VELOCITY - 1.0f);
			DoDamage(item, LARA_HEALTH_MAX * (SQUARE(base) / 196.0f));
		}

		float rumblePower = (item->Animation.Velocity.y / LARA_DEATH_VELOCITY) * RUMBLE_POWER_COEFF;
		Rumble(rumblePower, RUMBLE_DELAY);
	}
}

LaraInfo& GetLaraInfo(ItemInfo& item)
{
	if (item.ObjectNumber == ID_LARA)
	{
		auto* player = (LaraInfo*&)item.Data;
		return *player;
	}

	TENLog(std::string("Attempted to fetch LaraInfo data from entity with object ID ") + std::to_string(item.ObjectNumber), LogLevel::Warning);

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

	TENLog(std::string("Attempted to fetch LaraInfo data from entity with object ID ") + std::to_string(item.ObjectNumber), LogLevel::Warning);

	const auto& firstPlayerItem = *FindItem(ID_LARA);
	const auto* player = (LaraInfo*&)firstPlayerItem.Data;
	return *player;
}

LaraInfo*& GetLaraInfo(ItemInfo* item)
{
	if (item->ObjectNumber == ID_LARA)
		return (LaraInfo*&)item->Data;

	TENLog(std::string("Attempted to fetch LaraInfo data from entity with object ID ") + std::to_string(item->ObjectNumber), LogLevel::Warning);

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

void ModulateLaraTurnRateY(ItemInfo* item, short accelRate, short minTurnRate, short maxTurnRate, bool invert)
{
	auto* lara = GetLaraInfo(item);

	float axisCoeff = AxisMap[InputAxisID::Move].x;
	if (item->Animation.IsAirborne)
	{
		int sign = std::copysign(1, axisCoeff);
		axisCoeff = std::min(1.2f, abs(axisCoeff)) * sign;
	}

	lara->Control.TurnRate/*.y*/ = ModulateLaraTurnRate(lara->Control.TurnRate/*.y*/, accelRate, minTurnRate, maxTurnRate, axisCoeff, invert);
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
	SetAnimation(*item, LA_FALL_START);
	item->Animation.IsAirborne = true;
	item->Animation.Velocity.y = 0.0f;
}

void SetLaraFallBackAnimation(ItemInfo* item)
{
	SetAnimation(*item, LA_FALL_BACK);
	item->Animation.IsAirborne = true;
	item->Animation.Velocity.y = 0.0f;
}

void SetLaraMonkeyFallAnimation(ItemInfo* item)
{
	// HACK: Disallow release during 180 turn action.
	if (item->Animation.ActiveState == LS_MONKEY_TURN_180)
		return;

	SetAnimation(*item, LA_MONKEY_TO_FREEFALL);
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

		SetAnimation(*item, LA_SLIDE_BACK_START);
		item->Pose.Orientation.y = angle + ANGLE(180.0f);
	}
	else
	{
		if (item->Animation.ActiveState == LS_SLIDE_FORWARD && oldAngle == angle)
			return;

		SetAnimation(*item, LA_SLIDE_FORWARD);
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

		SetAnimation(*item, LA_SLIDE_FORWARD);
	}
	// Slide backward.
	else
	{
		if (item->Animation.ActiveState == LS_SLIDE_BACK && abs((short)(deltaAngle - ANGLE(180.0f))) <= -ANGLE(180.0f))
			return;

		SetAnimation(*item, LA_SLIDE_BACK_START);
	}
}

void SetLaraCornerAnimation(ItemInfo* item, CollisionInfo* coll, bool flip)
{
	auto* lara = GetLaraInfo(item);

	if (item->HitPoints <= 0)
	{
		SetAnimation(*item, LA_FALL_START);
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
			SetAnimation(*item, LA_LADDER_IDLE);
		else
			SetAnimation(*item, LA_HANG_IDLE);

		item->Pose.Position = lara->Context.NextCornerPos.Position;
		item->Pose.Orientation.y = lara->Context.NextCornerPos.Orientation.y;
		coll->Setup.PrevPosition = lara->Context.NextCornerPos.Position;
	}
}

void SetLaraSwimDiveAnimation(ItemInfo* item)
{
	auto* lara = GetLaraInfo(item);

	SetAnimation(*item, LA_ONWATER_DIVE);
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
