#include "framework.h"
#include "Game/Lara/Optics.h"

#include "Game/camera.h"
#include "Game/control/los.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_flare.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_one_gun.h"
#include "Game/Lara/lara_struct.h"
#include "Game/Lara/lara_two_guns.h"
#include "Game/Setup.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/Input/Input.h"

using namespace TEN::Input;

static void HandlePlayerOpticZoom(ItemInfo& item)
{
	constexpr auto OPTICS_RANGE_MAX	 = ANGLE(8.5f);
	constexpr auto OPTICS_RANGE_MIN	 = ANGLE(0.7f);
	constexpr auto OPTICS_RANGE_RATE = ANGLE(0.35f);

	auto& player = GetLaraInfo(item);
	bool isSlow = IsHeld(In::Walk);

	// Zoom optics.
	if (player.Control.Look.IsUsingBinoculars || player.Control.Look.IsUsingLasersight)
	{
		short rangeRate = isSlow ? (OPTICS_RANGE_RATE / 2) : OPTICS_RANGE_RATE;

		// NOTE: Zooming allowed with either StepLeft/StepRight or Walk/Sprint.
		if ((IsHeld(In::StepLeft) && !IsHeld(In::StepRight)) ||
			(IsHeld(In::Walk) && !IsHeld(In::Sprint)))
		{
			player.Control.Look.OpticRange -= rangeRate;
			if (player.Control.Look.OpticRange < OPTICS_RANGE_MIN)
			{
				player.Control.Look.OpticRange = OPTICS_RANGE_MIN;
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
			if (player.Control.Look.OpticRange > OPTICS_RANGE_MAX)
			{
				player.Control.Look.OpticRange = OPTICS_RANGE_MAX;
			}
			else
			{
				SoundEffect(SFX_TR4_BINOCULARS_ZOOM, nullptr, SoundEnvironment::Land, 1.0f);
			}
		}
	}
}

static void HandlePlayerOpticAnimations(ItemInfo& item)
{
	auto& player = GetLaraInfo(item);

	if (!player.Control.Look.IsUsingBinoculars && !player.Control.Look.IsUsingLasersight)
		return;

	int animNumber = Objects[ID_LARA_BINOCULARS_MESH].loaded ? LA_BINOCULARS_IDLE : LA_STAND_IDLE;
	if (player.Control.Look.IsUsingLasersight)
	{
		switch (player.Control.Weapon.GunType)
		{
			case LaraWeaponType::Crossbow:
				animNumber = Objects[ID_CROSSBOW_ANIM].animIndex + 2;
				break;

			case LaraWeaponType::Revolver:
				animNumber = Objects[ID_REVOLVER_ANIM].animIndex + 3;
				break;

			case LaraWeaponType::HK:
				animNumber = Objects[ID_HK_ANIM].animIndex + 2;
				break;
		}
	}
	else if (player.Control.Look.IsUsingBinoculars)
	{
		// Silently holster any weapon or drop any item currently in hand.
		if (player.Control.Weapon.GunType == LaraWeaponType::Flare ||
			player.Control.Weapon.GunType == LaraWeaponType::Torch)
		{
			CreateFlare(item, player.Control.Weapon.GunType == LaraWeaponType::Flare ? ID_FLARE_ITEM : ID_BURNING_TORCH_ITEM, 0);
			UndrawFlareMeshes(item);

			player.Torch.State = TorchState::Holding;
			player.Torch.IsLit = false;
			player.Flare.ControlLeft = false;
			player.Flare.Life = 0;
			player.Control.Weapon.GunType =
			player.Control.Weapon.RequestGunType = player.Control.Weapon.LastGunType;
		}
		else if (player.Control.Weapon.GunType != LaraWeaponType::None &&
				 player.Control.HandStatus != HandStatus::Free)
		{
			if (player.Control.Weapon.GunType <= LaraWeaponType::Uzi)
			{
				UndrawPistolMesh(item, player.Control.Weapon.GunType, false);
				UndrawPistolMesh(item, player.Control.Weapon.GunType, true);
			}
			else
			{
				if (player.Control.Weapon.WeaponItem != NO_VALUE)
				{
					KillItem(player.Control.Weapon.WeaponItem);
					player.Control.Weapon.WeaponItem = NO_VALUE;
				}
				UndrawShotgunMeshes(item, player.Control.Weapon.GunType);
			}

			player.TargetEntity = nullptr;
		}

		int objNumber = Objects[ID_LARA_BINOCULARS_MESH].loaded ? ID_LARA_BINOCULARS_MESH : player.Skin.Skin;
		item.Model.MeshIndex[LM_RHAND] = Objects[objNumber].meshIndex + LM_RHAND;

		player.Control.HandStatus = HandStatus::Free;
	}
	
	player.LeftArm.Locked =
	player.RightArm.Locked = false;
	player.LeftArm.FrameNumber =
	player.RightArm.FrameNumber = 0;
	player.LeftArm.AnimNumber =
	player.RightArm.AnimNumber = animNumber;
	player.LeftArm.FrameBase =
	player.RightArm.FrameBase = GetAnimData(animNumber).FramePtr;
}

static void ResetPlayerOpticAnimations(ItemInfo& item)
{
	auto& player = GetLaraInfo(item);

	ResetPlayerFlex(&item);

	player.LeftArm.Locked =
	player.RightArm.Locked = false;
	player.LeftArm.AnimNumber =
	player.RightArm.AnimNumber = 0;
	player.LeftArm.FrameNumber =
	player.RightArm.FrameNumber = 0;
	player.RightArm.FrameBase =
	player.LeftArm.FrameBase = GetAnimData(item).FramePtr;
	player.Control.HandStatus = player.Control.Look.IsUsingLasersight ? HandStatus::WeaponReady : HandStatus::Free;

	if (!player.Control.Look.IsUsingLasersight)
		item.Model.MeshIndex[LM_RHAND] = item.Model.BaseMesh + LM_RHAND;

	player.Control.Look.OpticRange = 0;
	player.Control.Look.IsUsingBinoculars = player.Control.Look.IsUsingLasersight = false;
	player.Inventory.IsBusy = false;

	Camera.DisableInterpolation = true;
	Camera.type = BinocularOldCamera;
	Camera.bounce = 0;
	AlterFOV(LastFOV);
	SetScreenFadeIn(OPTICS_FADE_SPEED);
}

static void DoOpticsHighlight(const ItemInfo& item, const Vector3i& origin, const Vector3i& target)
{
	auto origin2 = GameVector(origin, item.RoomNumber);
	auto target2 = GameVector(target);

	const auto& binocularsColor = g_GameFlow->GetSettings()->Camera.BinocularLightColor;
	const auto& lasersightColor = g_GameFlow->GetSettings()->Camera.LasersightLightColor;
	const auto& color = GetLaraInfo(item).Control.Look.IsUsingLasersight ? lasersightColor : binocularsColor;

	SpawnDynamicLight(origin2.x, origin2.y, origin2.z, 12, color.GetR(), color.GetG(), color.GetB());

	if (!LOS(&origin2, &target2))
	{
		int luma = sqrt(SQUARE(origin2.x - target2.x) + SQUARE(origin2.y - target2.y) + SQUARE(origin2.z - target2.z)) * CLICK(1);
		if ((luma + 8) > 31)
			luma = 31;

		auto dir = origin2.ToVector3() - target2.ToVector3();
		dir.Normalize();
		dir *= BLOCK(1);

		byte r = std::max(0, color.GetR() - luma);
		byte g = std::max(0, color.GetG() - luma);
		byte b = std::max(0, color.GetB() - luma);
		SpawnDynamicLight(target2.x + dir.x, target2.y + dir.y, target2.z + dir.z, luma + 12, r, g, b);
	}
}

bool HandlePlayerOptics(ItemInfo& item)
{
	auto& player = GetLaraInfo(item);

	bool breakOptics = true;

	// Standing; can use optics.
	if (item.Animation.ActiveState == LS_IDLE || item.Animation.AnimNumber == LA_STAND_IDLE)
		breakOptics = false;

	// Crouching; can use optics.
	if ((player.Control.IsLow || !IsHeld(In::Crouch)) &&
		(item.Animation.TargetState == LS_CROUCH_IDLE || item.Animation.AnimNumber == LA_CROUCH_IDLE))
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
	if (!breakOptics && !player.Control.Look.IsUsingLasersight && IsHeld(In::Look))
	{
		if (player.Control.HandStatus == HandStatus::WeaponReady &&
			((player.Control.Weapon.GunType == LaraWeaponType::HK       && player.Weapons[(int)LaraWeaponType::HK].HasLasersight) ||
			 (player.Control.Weapon.GunType == LaraWeaponType::Revolver && player.Weapons[(int)LaraWeaponType::Revolver].HasLasersight) ||
			 (player.Control.Weapon.GunType == LaraWeaponType::Crossbow && player.Weapons[(int)LaraWeaponType::Crossbow].HasLasersight)))
		{
			player.Control.Look.OpticRange = OPTICS_RANGE_DEFAULT;
			player.Control.Look.IsUsingBinoculars = true;
			player.Control.Look.IsUsingLasersight = true;
			player.Inventory.IsBusy = true;

			Camera.DisableInterpolation = true;
			BinocularOldCamera = Camera.oldType;
			SetScreenFadeIn(OPTICS_FADE_SPEED);
		}
	}

	// Not using optics; return early.
	if (!player.Control.Look.IsUsingBinoculars && !player.Control.Look.IsUsingLasersight)
		return true;

	AlterFOV(7 * (ANGLE(11.5f) - player.Control.Look.OpticRange), false);

	// Handle various binocular controls.
	if (!player.Control.Look.IsUsingLasersight)
	{
		if (IsClicked(In::Deselect) ||
			IsClicked(In::Roll) ||
			IsClicked(In::Jump) ||
			IsClicked(In::Draw) ||
			IsClicked(In::Look) ||
			IsHeld(In::Flare))
		{
			breakOptics = true;
		}
	}

	// Handle lasersight highlight.
	if (player.Control.Look.IsUsingLasersight || IsHeld(In::Action))
	{
		if (!player.Control.Look.IsUsingLasersight)
			ClearAction(In::Action);

		auto origin = Camera.pos.ToVector3i();
		auto target = Camera.target.ToVector3i();
		DoOpticsHighlight(item, origin, target);
	}

	if (!breakOptics)
	{
		HandlePlayerOpticAnimations(item);
		HandlePlayerOpticZoom(item);
		return true;
	}
	else
	{
		ResetPlayerOpticAnimations(item);
		return false;
	}
}
