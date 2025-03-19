#include "framework.h"
#include "Game/Lara/lara_fire.h"

#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/collision/Sphere.h"
#include "Game/control/los.h"
#include "Game/control/lot.h"
#include "Game/effects/effects.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara_flare.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_one_gun.h"
#include "Game/Lara/lara_struct.h"
#include "Game/Lara/lara_tests.h"
#include "Game/Lara/lara_two_guns.h"
#include "Game/misc.h"
#include "Game/savegame.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Objects/Generic/Object/burning_torch.h"
#include "Objects/Generic/Object/objects.h"
#include "Scripting/Include/Objects/ScriptInterfaceObjectsHandler.h"
#include "Scripting/Include/ScriptInterfaceGame.h"
#include "Scripting/Include/ScriptInterfaceLevel.h"
#include "Scripting/Internal/TEN/Objects/Lara/WeaponTypes.h"
#include "Sound/sound.h"
#include "Specific/configuration.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"
#include "Specific/trutils.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Sphere;
using namespace TEN::Entities::Generic;
using namespace TEN::Input;
using namespace TEN::Math;
using namespace TEN::Utils;

int FlashGrenadeAftershockTimer = 0;

// States in which Lara will hold an active flare out in front.
const auto FlarePoseStates = std::vector<int>
{
	LS_WALK_FORWARD,
	LS_RUN_FORWARD,
	LS_IDLE,
	LS_TURN_RIGHT_SLOW,
	LS_TURN_LEFT_SLOW,
	LS_WALK_BACK,
	LS_TURN_RIGHT_FAST,
	LS_TURN_LEFT_FAST,
	LS_STEP_RIGHT,
	LS_STEP_LEFT,
	LS_SWITCH_DOWN,
	LS_SWITCH_UP,
	LS_WADE_FORWARD,
	LS_CROUCH_IDLE,
	LS_CROUCH_TURN_LEFT,
	LS_CROUCH_TURN_RIGHT,
	LS_SOFT_SPLAT
};

const auto UnavailableFlarePoseAnims = std::vector<int>
{
	LA_WATERLEVER_PULL,
	LA_BUTTON_GIANT_PUSH,
	LA_BUTTON_LARGE_PUSH,
	LA_JUMPSWITCH_PULL,
	LA_UNDERWATER_CEILING_SWITCH_PULL,
	LA_VALVE_TURN
};

WeaponInfo Weapons[(int)LaraWeaponType::NumWeapons] =
{
	// No weapon
	{
		std::pair(EulerAngles::Identity, EulerAngles::Identity),
		std::pair(EulerAngles::Identity, EulerAngles::Identity),
		std::pair(EulerAngles::Identity, EulerAngles::Identity),
		0,
		0,
		0,
		0.0f,
		0,
		0,
		0,
		0,
		0,
		0
	},

	// Pistols
	{
		std::pair(EulerAngles(ANGLE(-80.0f), ANGLE(-60.0f), 0), EulerAngles(ANGLE(80.0f), ANGLE(60.0f), 0)),
		std::pair(EulerAngles(ANGLE(-80.0f), ANGLE(-170.0f), 0), EulerAngles(ANGLE(80.0f), ANGLE(60.0f), 0)),
		std::pair(EulerAngles(ANGLE(-80.0f), ANGLE(-60.0f), 0), EulerAngles(ANGLE(80.0f), ANGLE(170.0f), 0)),
		ANGLE(10.0f),
		ANGLE(8.0f),
		650,
		BLOCK(8),
		1,
		9,
		3,
		0,
		SFX_TR4_PISTOL_FIRE,
		0
	},

	// Revolver
	{
		std::pair(EulerAngles(ANGLE(-80.0f), ANGLE(-60.0f), 0), EulerAngles(ANGLE(80.0f), ANGLE(60.0f), 0)),
		std::pair(EulerAngles(ANGLE(-80.0f), ANGLE(-10.0f), 0), EulerAngles(ANGLE(80.0f), ANGLE(10.0f), 0)),
		std::pair(EulerAngles::Identity, EulerAngles::Identity),
		ANGLE(10.0f),
		ANGLE(4.0f),
		650,
		BLOCK(8),
		21,
		16,
		3,
		0,
		SFX_TR4_REVOLVER_FIRE,
		0
	},

	// Uzis
	{
		std::pair(EulerAngles(ANGLE(-80.0f), ANGLE(-60.0f), 0), EulerAngles(ANGLE(80.0f), ANGLE(60.0f), 0)),
		std::pair(EulerAngles(ANGLE(-80.0f), ANGLE(-170.0f), 0), EulerAngles(ANGLE(80.0f), ANGLE(60.0f), 0)),
		std::pair(EulerAngles(ANGLE(-80.0f), ANGLE(-60.0f), 0), EulerAngles(ANGLE(80.0f), ANGLE(170.0f), 0)),
		ANGLE(10.0f),
		ANGLE(8.0f),
		650,
		BLOCK(8),
		1,
		3,
		3,
		0,
		SFX_TR4_UZI_FIRE,
		0
	},

	// Shotgun
	{
		std::pair(EulerAngles(ANGLE(-70.0f), ANGLE(-60.0f), 0), EulerAngles(ANGLE(65.0f), ANGLE(60.0f), 0)),
		std::pair(EulerAngles(ANGLE(-70.0f), ANGLE(-80.0f), 0), EulerAngles(ANGLE(65.0f), ANGLE(80.0f), 0)),
		std::pair(EulerAngles(ANGLE(-70.0f), ANGLE(-80.0f), 0), EulerAngles(ANGLE(65.0f), ANGLE(80.0f), 0)),
		ANGLE(10.0f),
		ANGLE(10.0f),
		500,
		BLOCK(8),
		3,
		9,
		3,
		9,
		SFX_TR4_SHOTGUN_FIRE,
		0
	},

	// HK
	{
		std::pair(EulerAngles(ANGLE(-70.0f), ANGLE(-60.0f), 0), EulerAngles(ANGLE(65.0f), ANGLE(60.0f), 0)),
		std::pair(EulerAngles(ANGLE(-70.0f), ANGLE(-80.0f), 0), EulerAngles(ANGLE(65.0f), ANGLE(80.0f), 0)),
		std::pair(EulerAngles(ANGLE(-70.0f), ANGLE(-80.0f), 0), EulerAngles(ANGLE(65.0f), ANGLE(80.0f), 0)),
		ANGLE(10.0f),
		ANGLE(4.0f),
		500,
		BLOCK(12),
		4,
		0,
		3,
		16,
		0, // FIRE / SILENCER_FIRE
		0
	},

	// Crossbow
	{
		std::pair(EulerAngles(ANGLE(-70.0f), ANGLE(-60.0f), 0), EulerAngles(ANGLE(65.0f), ANGLE(60.0f), 0)),
		std::pair(EulerAngles(ANGLE(-70.0f), ANGLE(-80.0f), 0), EulerAngles(ANGLE(65.0f), ANGLE(80.0f), 0)),
		std::pair(EulerAngles(ANGLE(-70.0f), ANGLE(-80.0f), 0), EulerAngles(ANGLE(65.0f), ANGLE(80.0f), 0)),
		ANGLE(10.0f),
		ANGLE(8.0f),
		500,
		BLOCK(8),
		5,
		0,
		2,
		9,
		SFX_TR4_CROSSBOW_FIRE,
		20
	},

	// Flare
	{
		std::pair(EulerAngles::Identity, EulerAngles::Identity),
		std::pair(EulerAngles::Identity, EulerAngles::Identity),
		std::pair(EulerAngles::Identity, EulerAngles::Identity),
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0
	},

	// Torch
	{
		std::pair(EulerAngles(ANGLE(-55.0f), ANGLE(-30.0f), 0), EulerAngles(ANGLE(55.0f), ANGLE(30.0f), 0)),
		std::pair(EulerAngles(ANGLE(-55.0f), ANGLE(-30.0f), 0), EulerAngles(ANGLE(55.0f), ANGLE(30.0f), 0)),
		std::pair(EulerAngles(ANGLE(-55.0f), ANGLE(-30.0f), 0), EulerAngles(ANGLE(55.0f), ANGLE(30.0f), 0)),
		ANGLE(10.0f),
		ANGLE(8.0f),
		400,
		BLOCK(8),
		3,
		0,
		2,
		0,
		SFX_TR4_UZI_FIRE,
		0
	},

	// Grenade launcher
	{
		std::pair(EulerAngles(ANGLE(-70.0f), ANGLE(-60.0f), 0), EulerAngles(ANGLE(65.0f), ANGLE(60.0f), 0)),
		std::pair(EulerAngles(ANGLE(-70.0f), ANGLE(-80.0f), 0), EulerAngles(ANGLE(65.0f), ANGLE(80.0f), 0)),
		std::pair(EulerAngles(ANGLE(-70.0f), ANGLE(-80.0f), 0), EulerAngles(ANGLE(65.0f), ANGLE(80.0f), 0)),
		ANGLE(10.0f),
		ANGLE(8.0f),
		500,
		BLOCK(8),
		20,
		0,
		2,
		9,
		0,
		30
	},

	// Harpoon gun
	{
		std::pair(EulerAngles(ANGLE(-75.0f), ANGLE(-60.0f), 0), EulerAngles(ANGLE(75.0f), ANGLE(60.0f), 0)),
		std::pair(EulerAngles(ANGLE(-75.0f), ANGLE(-20.0f), 0), EulerAngles(ANGLE(75.0f), ANGLE(20.0f), 0)),
		std::pair(EulerAngles(ANGLE(-75.0f), ANGLE(-80.0f), 0), EulerAngles(ANGLE(75.0f), ANGLE(80.0f), 0)),
		ANGLE(10.0f),
		ANGLE(8.0f),
		500,
		BLOCK(8),
		6,
		0,
		2,
		10,
		0,
		0
	},

	// Rocket launcher
	{
		std::pair(EulerAngles(ANGLE(-70.0f), ANGLE(-60.0f), 0), EulerAngles(ANGLE(65.0f), ANGLE(60.0f), 0)),
		std::pair(EulerAngles(ANGLE(-70.0f), ANGLE(-80.0f), 0), EulerAngles(ANGLE(65.0f), ANGLE(80.0f), 0)),
		std::pair(EulerAngles(ANGLE(-70.0f), ANGLE(-80.0f), 0), EulerAngles(ANGLE(65.0f), ANGLE(80.0f), 0)),
		ANGLE(10.0f),
		ANGLE(8.0f),
		500,
		BLOCK(8),
		30,
		0,
		2,
		12,
		77,
		30
	},

	// Snowmobile
	{
		std::pair(EulerAngles(ANGLE(-55.0f), ANGLE(-30.0f), 0), EulerAngles(ANGLE(55.0f), ANGLE(30.0f), 0)),
		std::pair(EulerAngles(ANGLE(-55.0f), ANGLE(-30.0f), 0), EulerAngles(ANGLE(55.0f), ANGLE(30.0f), 0)),
		std::pair(EulerAngles(ANGLE(-55.0f), ANGLE(-30.0f), 0), EulerAngles(ANGLE(55.0f), ANGLE(30.0f), 0)),
		ANGLE(10.0f),
		ANGLE(8.0f),
		400,
		BLOCK(8),
		3,
		0,
		2,
		0,
		SFX_TR4_UZI_FIRE,
		0
	}
};

// TODO: Use map like in GetWeaponAnimData();
const WeaponInfo& GetWeaponInfo(LaraWeaponType weaponType)
{
	if ((int)weaponType < 0 || (int)weaponType >= (int)LaraWeaponType::NumWeapons)
		return Weapons[0];

	return (Weapons[(int)weaponType]);
}

void InitializeWeaponInfo(const Settings& settings)
{
	for (const auto& [name, weaponType] : WEAPON_TYPES)
	{
		if ((int)weaponType <= 0 || (int)weaponType >= (int)LaraWeaponType::NumWeapons)
			continue;

		auto& weapon = Weapons[(int)weaponType];
		const auto& weaponSettings = settings.Weapons[(int)weaponType - 1]; // Lua counts from 1.

		weapon.Damage = weaponSettings.Damage;
		weapon.AlternateDamage = weaponSettings.AlternateDamage;
		weapon.FlashTime = weaponSettings.FlashDuration;
		weapon.GunHeight = weaponSettings.WaterLevel;
		weapon.ShotAccuracy = ANGLE(weaponSettings.Accuracy);
		weapon.TargetDist = weaponSettings.Distance;
		weapon.RecoilFrame = weaponSettings.Interval;
	}
}

void InitializeNewWeapon(ItemInfo& laraItem)
{
	auto& player = *GetLaraInfo(&laraItem);

	player.TargetEntity = nullptr;
	player.LeftArm.AnimObjectID =
	player.RightArm.AnimObjectID = GetWeaponObjectID(player.Control.Weapon.GunType);
	player.LeftArm.FrameNumber =
	player.RightArm.FrameNumber = 0;
	player.LeftArm.Orientation =
	player.RightArm.Orientation = EulerAngles::Identity;
	player.LeftArm.Locked =
	player.RightArm.Locked = false;
	player.LeftArm.GunFlash =
	player.RightArm.GunFlash = 0;

	switch (player.Control.Weapon.GunType)
	{
	case LaraWeaponType::Pistol:
	case LaraWeaponType::Uzi:
		if (player.Control.HandStatus != HandStatus::Free)
			DrawPistolMeshes(laraItem, player.Control.Weapon.GunType);

		break;

	case LaraWeaponType::Shotgun:
	case LaraWeaponType::Revolver:
	case LaraWeaponType::HK:
	case LaraWeaponType::GrenadeLauncher:
	case LaraWeaponType::HarpoonGun:
	case LaraWeaponType::RocketLauncher:
		if (player.Control.HandStatus != HandStatus::Free)
			DrawShotgunMeshes(laraItem, player.Control.Weapon.GunType);

		break;

	case LaraWeaponType::Flare:
		if (player.Control.HandStatus != HandStatus::Free)
			DrawFlareMeshes(laraItem);

		break;

	default:
		break;
	}
}

Ammo& GetAmmo(LaraInfo& lara, LaraWeaponType weaponType)
{
	return lara.Weapons[(int)weaponType].Ammo[(int)lara.Weapons[(int)weaponType].SelectedAmmo];
}

GameVector GetTargetPoint(ItemInfo& targetEntity)
{
	const auto& bounds = GetClosestKeyframe(targetEntity).BoundingBox;

	auto center = Vector3i(
		(bounds.X1 + bounds.X2) / 2,
		bounds.Y1 + (bounds.GetHeight() / 3),
		(bounds.Z1 + bounds.Z2) / 2);

	float sinY = phd_sin(targetEntity.Pose.Orientation.y);
	float cosY = phd_cos(targetEntity.Pose.Orientation.y);

	return GameVector(
		targetEntity.Pose.Position.x + ((center.x * cosY) + (center.z * sinY)),
		targetEntity.Pose.Position.y + center.y,
		targetEntity.Pose.Position.z + ((center.z * cosY) - (center.x * sinY)),
		targetEntity.RoomNumber);
}

HolsterSlot GetWeaponHolsterSlot(LaraWeaponType weaponType)
{
	switch (weaponType)
	{
	case LaraWeaponType::Pistol:
		return HolsterSlot::Pistols;

	case LaraWeaponType::Uzi:
		return HolsterSlot::Uzis;

	case LaraWeaponType::Revolver:
		return HolsterSlot::Revolver;

	case LaraWeaponType::Shotgun:
		return HolsterSlot::Shotgun;

	case LaraWeaponType::HK:
		return HolsterSlot::HK;

	case LaraWeaponType::HarpoonGun:
		return HolsterSlot::Harpoon;

	case LaraWeaponType::Crossbow:
		return HolsterSlot::Crossbow;

	case LaraWeaponType::GrenadeLauncher:
		return HolsterSlot::GrenadeLauncher;

	case LaraWeaponType::RocketLauncher:
		return HolsterSlot::RocketLauncher;

	default:
		return HolsterSlot::Empty;
	}
}

GAME_OBJECT_ID GetWeaponObjectID(LaraWeaponType weaponType)
{
	switch (weaponType)
	{
	case LaraWeaponType::Uzi:
		return ID_UZI_ANIM;

	case LaraWeaponType::Shotgun:
		return ID_SHOTGUN_ANIM;

	case LaraWeaponType::Revolver:
		return ID_REVOLVER_ANIM;

	case LaraWeaponType::Crossbow:
		return ID_CROSSBOW_ANIM;

	case LaraWeaponType::HK:
		return ID_HK_ANIM;

	case LaraWeaponType::Flare:
		return ID_FLARE_ANIM;

	case LaraWeaponType::GrenadeLauncher:
		return ID_GRENADE_ANIM;

	case LaraWeaponType::RocketLauncher:
		return ID_ROCKET_ANIM;

	case LaraWeaponType::HarpoonGun:
		return ID_HARPOON_ANIM;

	default:
		return ID_PISTOLS_ANIM;
	}
}

GAME_OBJECT_ID GetWeaponObjectMeshID(ItemInfo& laraItem, LaraWeaponType weaponType)
{
	const auto& player = *GetLaraInfo(&laraItem);

	switch (weaponType)
	{
	case LaraWeaponType::Revolver:
		return (player.Weapons[(int)LaraWeaponType::Revolver].HasLasersight ? ID_LARA_REVOLVER_LASER : ID_REVOLVER_ANIM);

	case LaraWeaponType::Uzi:
		return ID_UZI_ANIM;

	case LaraWeaponType::Shotgun:
		return ID_SHOTGUN_ANIM;

	case LaraWeaponType::HK:
		return ID_HK_ANIM;

	case LaraWeaponType::Crossbow:
		return (player.Weapons[(int)LaraWeaponType::Crossbow].HasLasersight ? ID_LARA_CROSSBOW_LASER : ID_CROSSBOW_ANIM);

	case LaraWeaponType::GrenadeLauncher:
		return ID_GRENADE_ANIM;

	case LaraWeaponType::HarpoonGun:
		return ID_HARPOON_ANIM;

	case LaraWeaponType::RocketLauncher:
		return ID_ROCKET_ANIM;

	default:
		return ID_PISTOLS_ANIM;
	}
}

static void ClearPlayerTargets(ItemInfo& playerItem)
{
	auto& player = GetLaraInfo(playerItem);

	player.TargetEntity = nullptr;
	player.TargetList.fill(nullptr);
	player.LastTargets.fill(nullptr);
}

void HandleWeapon(ItemInfo& laraItem)
{
	auto& player = *GetLaraInfo(&laraItem);

	if (player.LeftArm.GunFlash > 0)
		--player.LeftArm.GunFlash;

	if (player.RightArm.GunFlash > 0)
		--player.RightArm.GunFlash;

	if (player.RightArm.GunSmoke > 0)
		--player.RightArm.GunSmoke;

	if (player.LeftArm.GunSmoke > 0)
		--player.LeftArm.GunSmoke;

	if (FlashGrenadeAftershockTimer)
		FlashGrenadeAftershockTimer--;

	if (player.Control.Weapon.GunType == LaraWeaponType::Torch)
	{
		DoFlameTorch();
		return;
	}

	if (laraItem.HitPoints <= 0)
	{
		player.Control.HandStatus = HandStatus::Free;
	}
	else if (player.Control.HandStatus == HandStatus::Free)
	{
		// Draw weapon.
		if (IsHeld(In::Draw))
		{
			// No weapon; no actions.
			if (player.Control.Weapon.LastGunType != LaraWeaponType::None)
				player.Control.Weapon.RequestGunType = player.Control.Weapon.LastGunType;
		}
		// Draw flare.
		else if (IsHeld(In::Flare))
		{
			if (player.Control.Weapon.GunType == LaraWeaponType::Flare)
			{
				player.Control.HandStatus = HandStatus::WeaponUndraw;
			}
			else if (player.Inventory.TotalFlares && !player.Control.Look.IsUsingBinoculars)
			{
				if (player.Inventory.TotalFlares != NO_VALUE)
					player.Inventory.TotalFlares--;

				player.Control.Weapon.RequestGunType = LaraWeaponType::Flare;
			}
		}

		if ((IsHeld(In::Draw) && player.Control.Weapon.LastGunType != LaraWeaponType::None) ||
			player.Control.Weapon.RequestGunType != player.Control.Weapon.GunType)
		{
			if (player.Control.IsLow && 
				player.Control.Weapon.RequestGunType >= LaraWeaponType::Shotgun && 
				player.Control.Weapon.RequestGunType != LaraWeaponType::Flare && 
				player.Control.Weapon.RequestGunType != LaraWeaponType::Torch)
			{
				if (player.Control.Weapon.GunType == LaraWeaponType::Flare)
					player.Control.Weapon.RequestGunType = LaraWeaponType::Flare;
			}
			else if (player.Control.Weapon.RequestGunType == LaraWeaponType::Flare ||
				(player.Context.Vehicle == NO_VALUE &&
					(player.Control.Weapon.RequestGunType == LaraWeaponType::HarpoonGun ||
						player.Control.WaterStatus == WaterStatus::Dry ||
						(player.Control.WaterStatus == WaterStatus::Wade &&
							player.Context.WaterSurfaceDist > -GetWeaponInfo(player.Control.Weapon.GunType).GunHeight))))
			{
				if (player.Control.Weapon.GunType == LaraWeaponType::Flare)
				{
					CreateFlare(laraItem, ID_FLARE_ITEM, 0);
					UndrawFlareMeshes(laraItem);
					player.Flare.ControlLeft = false;
					player.Flare.Life = 0;
				}

				player.Control.Weapon.GunType = player.Control.Weapon.RequestGunType;
				InitializeNewWeapon(laraItem);
				player.LeftArm.FrameNumber = 0;
				player.RightArm.FrameNumber = 0;
				player.Control.HandStatus = HandStatus::WeaponDraw;
			}
			else
			{
				player.Control.Weapon.LastGunType = player.Control.Weapon.RequestGunType;

				if (player.Control.Weapon.GunType != LaraWeaponType::Flare)
				{
					player.Control.Weapon.GunType = player.Control.Weapon.RequestGunType;
				}
				else
				{
					player.Control.Weapon.RequestGunType = LaraWeaponType::Flare;
				}
			}
		}
	}
	else if (player.Control.HandStatus == HandStatus::WeaponReady)
	{
		if (IsHeld(In::Draw) ||
			player.Control.Weapon.RequestGunType != player.Control.Weapon.GunType)
		{
			player.Control.HandStatus = HandStatus::WeaponUndraw;
		}
		else if (player.Control.Weapon.GunType != LaraWeaponType::HarpoonGun &&
			player.Control.WaterStatus != WaterStatus::Dry &&
			(player.Control.WaterStatus != WaterStatus::Wade ||
				player.Context.WaterSurfaceDist < -GetWeaponInfo(player.Control.Weapon.GunType).GunHeight))
		{
			player.Control.HandStatus = HandStatus::WeaponUndraw;
		}
	}
	else if (IsHeld(In::Flare) &&
		player.Control.HandStatus == HandStatus::Busy &&
		laraItem.Animation.ActiveState == LS_CRAWL_IDLE &&
		laraItem.Animation.AnimNumber == LA_CRAWL_IDLE)
	{
			if (player.Inventory.TotalFlares)
			{
				if (player.Inventory.TotalFlares != NO_VALUE)
					player.Inventory.TotalFlares--;

				player.Control.Weapon.RequestGunType = LaraWeaponType::Flare;
			}
	}

	switch (player.Control.HandStatus)
	{
	case HandStatus::WeaponDraw:
		if (player.Control.Weapon.GunType != LaraWeaponType::Flare &&
			player.Control.Weapon.GunType != LaraWeaponType::None)
		{
			player.Control.Weapon.LastGunType = player.Control.Weapon.GunType;
		}

		switch (player.Control.Weapon.GunType)
		{
		case LaraWeaponType::Pistol:
		case LaraWeaponType::Revolver:
		case LaraWeaponType::Uzi:
			if (Camera.type != CameraType::Look && Camera.type != CameraType::Heavy)
				Camera.type = CameraType::Combat;

			DrawPistols(laraItem, player.Control.Weapon.GunType);
			break;

		case LaraWeaponType::Shotgun:
		case LaraWeaponType::Crossbow:
		case LaraWeaponType::HK:
		case LaraWeaponType::GrenadeLauncher:
		case LaraWeaponType::RocketLauncher:
		case LaraWeaponType::HarpoonGun:
			if (Camera.type != CameraType::Look && Camera.type != CameraType::Heavy)
				Camera.type = CameraType::Combat;

			DrawShotgun(laraItem, player.Control.Weapon.GunType);
			break;

		case LaraWeaponType::Flare:
			DrawFlare(laraItem);
			break;

		default:
			player.Control.HandStatus = HandStatus::Free;
			break;
		}

		break;

	case HandStatus::Special:
		DrawFlare(laraItem);
		break;

	case HandStatus::WeaponUndraw:
		ClearPlayerTargets(laraItem);
		laraItem.Model.MeshIndex[LM_HEAD] = laraItem.Model.BaseMesh + LM_HEAD;

		switch (player.Control.Weapon.GunType)
		{
		case LaraWeaponType::Pistol:
		case LaraWeaponType::Revolver:
		case LaraWeaponType::Uzi:
			UndrawPistols(laraItem, player.Control.Weapon.GunType);
			break;

		case LaraWeaponType::Shotgun:
		case LaraWeaponType::Crossbow:
		case LaraWeaponType::HK:
		case LaraWeaponType::GrenadeLauncher:
		case LaraWeaponType::RocketLauncher:
		case LaraWeaponType::HarpoonGun:
			UndrawShotgun(laraItem, player.Control.Weapon.GunType);
			break;

		case LaraWeaponType::Flare:
			UndrawFlare(laraItem);
			break;

		default:
			return;
		}

		break;

	case HandStatus::WeaponReady:
		if (!IsHeld(In::Action) || !GetAmmo(player, player.Control.Weapon.GunType))
		{
			laraItem.Model.MeshIndex[LM_HEAD] = laraItem.Model.BaseMesh + LM_HEAD;
		}
		else
		{
			laraItem.Model.MeshIndex[LM_HEAD] = Objects[ID_LARA_SCREAM].meshIndex + LM_HEAD;
		}

		if (Camera.type != CameraType::Look &&
			Camera.type != CameraType::Heavy)
		{
			Camera.type = CameraType::Combat;
		}

		if (IsHeld(In::Action) && !player.Control.Look.IsUsingLasersight)
		{
			if (!GetAmmo(player, player.Control.Weapon.GunType))
			{
				bool hasPistols = (player.Weapons[(int)LaraWeaponType::Pistol].Present && Objects[ID_PISTOLS_ITEM].loaded && GetAmmo(player, LaraWeaponType::Pistol));
				player.Control.Weapon.RequestGunType = hasPistols ? LaraWeaponType::Pistol : LaraWeaponType::None;
				return;
			}
		}

		switch (player.Control.Weapon.GunType)
		{
		case LaraWeaponType::Pistol:
		case LaraWeaponType::Uzi:
			HandlePistols(laraItem, player.Control.Weapon.GunType);
			break;

		case LaraWeaponType::Shotgun:
		case LaraWeaponType::Crossbow:
		case LaraWeaponType::HK:
		case LaraWeaponType::GrenadeLauncher:
		case LaraWeaponType::RocketLauncher:
		case LaraWeaponType::HarpoonGun:
		case LaraWeaponType::Revolver:
			RifleHandler(laraItem, player.Control.Weapon.GunType);
			LasersightWeaponHandler(laraItem, player.Control.Weapon.GunType);
			break;

		default:
			return;
		}

		break;

	case HandStatus::Free:
		if (player.Control.Weapon.GunType == LaraWeaponType::Flare)
		{
			if (player.Context.Vehicle != NO_VALUE || TestState(laraItem.Animation.ActiveState, FlarePoseStates))
			{
				if (player.Flare.ControlLeft)
				{
					if (player.LeftArm.FrameNumber)
					{
						if (++player.LeftArm.FrameNumber == 110)
							player.LeftArm.FrameNumber = 0;
					}
				}
				else
				{
					player.LeftArm.FrameNumber = 95;
					player.Flare.ControlLeft = true;
				}
			}
			else
			{
				player.Flare.ControlLeft = false;
			}

			DoFlareInHand(laraItem, player.Flare.Life);
			SetFlareArm(laraItem, player.LeftArm.FrameNumber);
		}

		break;

	case HandStatus::Busy:
		if (player.Control.Weapon.GunType == LaraWeaponType::Flare)
		{
			if (laraItem.Model.MeshIndex[LM_LHAND] == Objects[ID_FLARE_ANIM].meshIndex + LM_LHAND)
			{
				player.Flare.ControlLeft = (player.Context.Vehicle != NO_VALUE || TestState(laraItem.Animation.ActiveState, FlarePoseStates));

				if (Contains(UnavailableFlarePoseAnims, laraItem.Animation.AnimNumber))
					player.Flare.ControlLeft = false;

				DoFlareInHand(laraItem, player.Flare.Life);
				SetFlareArm(laraItem, player.LeftArm.FrameNumber);
			}
		}

		break;
	}
}

void AimWeapon(ItemInfo& laraItem, ArmInfo& arm, const WeaponInfo& weaponInfo)
{
	const auto& player = *GetLaraInfo(&laraItem);

	auto targetArmOrient = arm.Locked ? player.TargetArmOrient : EulerAngles::Identity;
	arm.Orientation.InterpolateConstant(targetArmOrient, weaponInfo.AimSpeed);
}

// TODO: Include snowmobile gun in GetAmmo(), otherwise the player won't be able to shoot while controlling it. -- TokyoSU 2023.04.21
FireWeaponType FireWeapon(LaraWeaponType weaponType, ItemInfo* targetEntity, ItemInfo& laraItem, const EulerAngles& armOrient)
{
	auto& player = *GetLaraInfo(&laraItem);
	auto& ammo = GetAmmo(player, weaponType);

	if (ammo.GetCount() == 0 && !ammo.HasInfinite())
		return FireWeaponType::NoAmmo;

	if (!ammo.HasInfinite())
		ammo--;

	const auto& weapon = GetWeaponInfo(weaponType);

	auto wobbledArmOrient = EulerAngles(
		armOrient.x + (Random::GenerateAngle(0, ANGLE(180.0f)) - ANGLE(90.0f)) * weapon.ShotAccuracy / 65536,
		armOrient.y + (Random::GenerateAngle(0, ANGLE(180.0f)) - ANGLE(90.0f)) * weapon.ShotAccuracy / 65536,
		0);

	auto muzzleOffset = GetJointPosition(&laraItem, LM_RHAND);
	auto pos = Vector3i(laraItem.Pose.Position.x, muzzleOffset.y, laraItem.Pose.Position.z);

	// Calculate ray from wobbled orientation.
	auto directionNorm = wobbledArmOrient.ToDirection();
	auto origin = pos.ToVector3();
	auto target = origin + (directionNorm * weapon.TargetDist);
	auto ray = Ray(origin, directionNorm);

	player.Control.Weapon.HasFired = true;
	player.Control.Weapon.Fired = true;

	auto vOrigin = GameVector(pos);
	short roomNumber = laraItem.RoomNumber;
	GetFloor(pos.x, pos.y, pos.z, &roomNumber);
	vOrigin.RoomNumber = roomNumber;

	if (targetEntity == nullptr)
	{
		auto vTarget = GameVector(target);
		GetTargetOnLOS(&vOrigin, &vTarget, false, true);
		return FireWeaponType::Miss;
	}

	auto spheres = targetEntity->GetSpheres();
	int closestJointIndex = NO_VALUE;
	float closestDist = INFINITY;
	for (int i = 0; i < spheres.size(); i++)
	{
		float dist = 0.0f;
		if (ray.Intersects(spheres[i], dist))
		{
			if (dist < closestDist)
			{
				closestDist = dist;
				closestJointIndex = i;
			}
		}
	}
	
	if (closestJointIndex == NO_VALUE)
	{
		auto vTarget = GameVector(target);
		GetTargetOnLOS(&vOrigin, &vTarget, false, true);
		return FireWeaponType::Miss;
	}
	else
	{
		target = origin + (directionNorm * closestDist);
		auto vTarget = GameVector(target);

		// NOTE: It seems that entities hit by the player in the normal way must have GetTargetOnLOS return false.
		// It's strange, but this replicates original behaviour until we fully understand what is happening.
		if (!GetTargetOnLOS(&vOrigin, &vTarget, false, true))
			HitTarget(&laraItem, targetEntity, &vTarget, weapon.Damage, false, closestJointIndex);

		return FireWeaponType::PossibleHit;
	}
}

void FindNewTarget(ItemInfo& laraItem, const WeaponInfo& weaponInfo)
{
	if (!g_Configuration.EnableAutoTargeting)
		return;

	auto& player = *GetLaraInfo(&laraItem);

	if (player.Control.Look.OpticRange)
	{
		player.TargetEntity = nullptr;
		return;
	}

	auto origin = GameVector(
		laraItem.Pose.Position.x,
		GetJointPosition(&laraItem, LM_RHAND).y, // Muzzle offset.
		laraItem.Pose.Position.z,
		laraItem.RoomNumber);

	ItemInfo* closestEntityPtr = nullptr;

	float closestDistance = INFINITY;
	short closestHeadingAngle = MAXSHORT;
	unsigned int targetCount = 0;
	float maxDistance = weaponInfo.TargetDist;

	for (auto* creaturePtr : ActiveCreatures)
	{
		// Continue loop if no item.
		if (creaturePtr->ItemNumber == NO_VALUE)
			continue;

		auto& item = g_Level.Items[creaturePtr->ItemNumber];

		// Check if creature is alive.
		if (item.HitPoints <= 0)
			continue;

		// Check distance.
		float distance = Vector3::Distance(origin.ToVector3(), item.Pose.Position.ToVector3());
		if (distance > maxDistance)
			continue;

		// Assess line of sight.
		auto target = GetTargetPoint(item);
		if (!LOS(&origin, &target))
			continue;

		// Assess whether relative orientation falls within weapon's lock constraints.
		auto orient = Geometry::GetOrientToPoint(origin.ToVector3(), target.ToVector3()) - (laraItem.Pose.Orientation + player.ExtraTorsoRot);
		if (orient.x >= weaponInfo.LockOrientConstraint.first.x &&
			orient.y >= weaponInfo.LockOrientConstraint.first.y &&
			orient.x <= weaponInfo.LockOrientConstraint.second.x &&
			orient.y <= weaponInfo.LockOrientConstraint.second.y)
		{
			if (targetCount >= player.TARGET_COUNT_MAX - 1)
				break;

			player.TargetList[targetCount] = &item;
			targetCount++;

			if (distance < closestDistance &&
				abs(orient.y) < (closestHeadingAngle + ANGLE(15.0f)))
			{
				closestEntityPtr = &item;
				closestDistance = distance;
				closestHeadingAngle = abs(orient.y);
			}
		}
	}

	player.TargetList[targetCount] = nullptr;
	if (player.TargetList[0] == nullptr)
	{
		player.TargetEntity = nullptr;
	}
	else
	{
		for (const auto* targetPtr : player.TargetList)
		{
			if (targetPtr == nullptr)
				player.TargetEntity = nullptr;

			if (targetPtr == player.TargetEntity)
				break;
		}

		if (IsClicked(In::Look) || player.Control.HandStatus != HandStatus::Free)
		{
			if (player.TargetEntity == nullptr)
			{
				player.TargetEntity = closestEntityPtr;
				player.LastTargets[0] = nullptr;
			}
			else if (IsClicked(In::Look))
			{
				player.TargetEntity = nullptr;
				bool flag = true;

				for (const auto& targetPtr : player.TargetList)
				{
					bool doLoop = false;
					for (const auto* lastTargetPtr : player.LastTargets)
					{
						if (lastTargetPtr == targetPtr)
						{
							doLoop = true;
							break;
						}
					}

					if (!doLoop)
					{
						player.TargetEntity = targetPtr;
						if (player.TargetEntity)
							flag = false;

						break;
					}
				}

				if (flag)
				{
					player.TargetEntity = closestEntityPtr;
					player.LastTargets[0] = nullptr;
				}
			}
		}
	}

	if (player.TargetEntity != player.LastTargets[0])
	{
		for (int slot = LaraInfo::TARGET_COUNT_MAX - 1; slot > 0; --slot)
			player.LastTargets[slot] =  player.LastTargets[slot - 1];
		
		player.LastTargets[0] = player.TargetEntity;
	}

	LaraTargetInfo(laraItem, weaponInfo);
}

void LaraTargetInfo(ItemInfo& laraItem, const WeaponInfo& weaponInfo)
{
	auto& player = *GetLaraInfo(&laraItem);

	if (player.TargetEntity == nullptr)
	{
		player.RightArm.Locked = false;
		player.LeftArm.Locked = false;
		player.TargetArmOrient = EulerAngles::Identity;
		return;
	}

	auto origin = GameVector(
		laraItem.Pose.Position.x,
		GetJointPosition(&laraItem, LM_RHAND).y, // Muzzle offset.
		laraItem.Pose.Position.z,
		laraItem.RoomNumber);
	auto target = GetTargetPoint(*player.TargetEntity);

	auto orient = Geometry::GetOrientToPoint(origin.ToVector3(), target.ToVector3()) - laraItem.Pose.Orientation;

	if (LOS(&origin, &target))
	{
		if (orient.x >= weaponInfo.LockOrientConstraint.first.x &&
			orient.y >= weaponInfo.LockOrientConstraint.first.y &&
			orient.x <= weaponInfo.LockOrientConstraint.second.x &&
			orient.y <= weaponInfo.LockOrientConstraint.second.y)
		{
			player.RightArm.Locked = true;
			player.LeftArm.Locked = true;
		}
		else
		{
			if (player.LeftArm.Locked)
			{
				if (orient.x < weaponInfo.LeftOrientConstraint.first.x ||
					orient.y < weaponInfo.LeftOrientConstraint.first.y ||
					orient.x > weaponInfo.LeftOrientConstraint.second.x ||
					orient.y > weaponInfo.LeftOrientConstraint.second.y)
				{
					player.LeftArm.Locked = false;
				}
			}

			if (player.RightArm.Locked)
			{
				if (orient.x < weaponInfo.RightOrientConstraint.first.x ||
					orient.y < weaponInfo.RightOrientConstraint.first.y ||
					orient.x > weaponInfo.RightOrientConstraint.second.x ||
					orient.y > weaponInfo.RightOrientConstraint.second.y)
				{
					player.RightArm.Locked = false;
				}
			}
		}
	}
	else
	{
		player.RightArm.Locked = false;
		player.LeftArm.Locked = false;
	}

	player.TargetArmOrient = orient;
}

void HitTarget(ItemInfo* laraItem, ItemInfo* targetEntity, GameVector* hitPos, int damage, bool isExplosive, int bestJointIndex)
{
	const auto& object = Objects[targetEntity->ObjectNumber];

	targetEntity->HitStatus = true;
	if (targetEntity->IsCreature())
		GetCreatureInfo(targetEntity)->HurtByLara = true;

	if (object.HitRoutine == nullptr)
		return;

	if (hitPos != nullptr)
	{
		hitPos->RoomNumber = targetEntity->RoomNumber;
		object.HitRoutine(*targetEntity, *laraItem, *hitPos, damage, isExplosive, bestJointIndex);
	}
	else
	{
		object.HitRoutine(*targetEntity, *laraItem, std::nullopt, damage, isExplosive, bestJointIndex);
	}
}

void SmashItem(short itemNumber)
{
	const auto& item = g_Level.Items[itemNumber];

	if (item.ObjectNumber >= ID_SMASH_OBJECT1 && item.ObjectNumber <= ID_SMASH_OBJECT8)
		SmashObject(itemNumber);
}
