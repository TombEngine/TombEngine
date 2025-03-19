#include "framework.h"
#include "Game/Lara/lara_one_gun.h"

#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/Point.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/control/los.h"
#include "Game/effects/Bubble.h"
#include "Game/effects/debris.h"
#include "Game/effects/Drip.h"
#include "Game/effects/effects.h"
#include "Game/effects/item_fx.h"
#include "Game/effects/Ripple.h"
#include "Game/effects/Splash.h"
#include "Game/effects/tomb4fx.h"
#include "Game/effects/weather.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_fire.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_two_guns.h"
#include "Game/misc.h"
#include "Game/savegame.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Objects/Generic/Object/objects.h"
#include "Objects/Generic/Switches/generic_switch.h"
#include "Sound/sound.h"
#include "Specific/clock.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;
using namespace TEN::Effects::Bubble;
using namespace TEN::Effects::Drip;
using namespace TEN::Effects::Environment;
using namespace TEN::Effects::Items;
using namespace TEN::Effects::Ripple;
using namespace TEN::Effects::Splash;
using namespace TEN::Entities::Switches;
using namespace TEN::Input;
using namespace TEN::Math;

constexpr auto TRIGGER_TIMEOUT		 = 5;
constexpr auto GRENADE_FRAG_TIMEOUT  = 4;
constexpr auto GRENADE_FLASH_TIMEOUT = 4;

constexpr auto HARPOON_VELOCITY = BLOCK(0.25f);
constexpr auto HARPOON_TIME		= 10 * FPS;
constexpr auto BOLT_VELOCITY	= BLOCK(0.25f);
constexpr auto BOLT_TIME		= 10 * FPS;
constexpr auto BOLT_STUCK_LIFE	= 5 * FPS;
constexpr auto ROCKET_VELOCITY	= CLICK(2);
constexpr auto ROCKET_TIME		= 4.5f * FPS;
constexpr auto GRENADE_VELOCITY = BLOCK(1 / 8.0f);
constexpr auto GRENADE_TIME		= 4 * FPS;

constexpr auto PROJECTILE_HIT_RADIUS	 = BLOCK(1 / 8.0f);
constexpr auto PROJECTILE_EXPLODE_RADIUS = BLOCK(1);

constexpr auto HK_BURST_MODE_SHOT_COUNT				  = 5;
constexpr auto HK_BURST_AND_SNIPER_MODE_SHOT_INTERVAL = 12.0f;
constexpr auto HK_RAPID_MODE_SHOT_INTERVAL			  = 3.0f;

constexpr auto SHOTGUN_PELLET_COUNT = 6;

static Vector3i GetWeaponSmokeRelOffset(LaraWeaponType weaponType)
{
	switch (weaponType)
	{
	case LaraWeaponType::HK:
		return Vector3i(0, 228, 96);

	case LaraWeaponType::Shotgun:
		return Vector3i(0, 228, 0);

	case LaraWeaponType::GrenadeLauncher:
		return Vector3i(0, 180, 80);

	case LaraWeaponType::RocketLauncher:
		return Vector3i(0, 84, 72);;

	default:
		return Vector3i::Zero;
	}
}

void AnimateShotgun(ItemInfo& laraItem, LaraWeaponType weaponType)
{
	auto& player = *GetLaraInfo(&laraItem);
	const auto& weapon = player.Weapons[(int)LaraWeaponType::HK];

	if (player.LeftArm.GunSmoke > 0)
	{
		auto relOffset = GetWeaponSmokeRelOffset(weaponType);
		auto pos = GetJointPosition(&laraItem, LM_RHAND, relOffset);

		if (laraItem.MeshBits.TestAny())
			TriggerGunSmoke(pos.x, pos.y, pos.z, 0, 0, 0, 0, weaponType, player.LeftArm.GunSmoke);
	}

	auto& item = g_Level.Items[player.Control.Weapon.WeaponItem];
	item.Animation.AnimObjectID = item.ObjectNumber;

	bool isRunning = (weaponType == LaraWeaponType::HK && laraItem.Animation.Velocity.z != 0.0f);

	static bool reloadHarpoonGun = false;
	reloadHarpoonGun = (player.Weapons[(int)weaponType].Ammo->HasInfinite() || weaponType != LaraWeaponType::HarpoonGun) ? false : reloadHarpoonGun;

	if (player.Control.Weapon.Interval != 0.0f)
	{
		player.Control.Weapon.Interval -= 1.0f;
		player.Control.Weapon.Timer = 0.0f;
	}

	switch (item.Animation.ActiveState)
	{
	case WEAPON_STATE_AIM:
		player.Control.Weapon.NumShotsFired = 0;
		player.Control.Weapon.Interval = 0.0f;
		player.Control.Weapon.Timer = 0.0f;

		if (player.Control.WaterStatus == WaterStatus::Underwater || isRunning)
		{
			item.Animation.TargetState = WEAPON_STATE_UNDERWATER_AIM;
		}
		else if ((!IsHeld(In::Action) || player.TargetEntity) && player.LeftArm.Locked == false)
		{
			item.Animation.TargetState = WEAPON_STATE_UNAIM;
		}
		else
		{
			item.Animation.TargetState = WEAPON_STATE_RECOIL;
		}

		if (weaponType == LaraWeaponType::HarpoonGun &&
			!player.Weapons[(int)weaponType].Ammo->HasInfinite())
		{
			if (reloadHarpoonGun)
			{
				item.Animation.TargetState = WEAPON_STATE_RELOAD;
				reloadHarpoonGun = false;
			}
		}

		break;

	case WEAPON_STATE_UNDERWATER_AIM:
		player.Control.Weapon.NumShotsFired = 0;
		player.Control.Weapon.Interval = 0.0f;
		player.Control.Weapon.Timer = 0.0f;

		if (player.Control.WaterStatus == WaterStatus::Underwater || isRunning)
		{
			if ((!IsHeld(In::Action) || player.TargetEntity) && player.LeftArm.Locked == false)
				item.Animation.TargetState = WEAPON_STATE_UNDERWATER_UNAIM;
			else
				item.Animation.TargetState = WEAPON_STATE_UNDERWATER_RECOIL;
		}
		else
		{
			item.Animation.TargetState = WEAPON_STATE_AIM;
		}

		if (weaponType == LaraWeaponType::HarpoonGun &&
			!player.Weapons[(int)weaponType].Ammo->HasInfinite())
		{
			if (reloadHarpoonGun)
			{
				item.Animation.TargetState = WEAPON_STATE_RELOAD;
				reloadHarpoonGun = false;
			}
		}

		break;

	case WEAPON_STATE_RECOIL:
		if (item.Animation.FrameNumber == 0)
		{
			item.Animation.TargetState = WEAPON_STATE_UNAIM;

			if (player.Control.WaterStatus != WaterStatus::Underwater &&
				!isRunning && !reloadHarpoonGun)
			{
				if (IsHeld(In::Action) && (!player.TargetEntity || player.LeftArm.Locked))
				{
					switch (weaponType)
					{
					default:
						FireShotgun(laraItem);
						break;

					case LaraWeaponType::HarpoonGun:
						FireHarpoon(laraItem);

						if (!(player.Weapons[(int)LaraWeaponType::HarpoonGun].Ammo->GetCount() % 4) &&
							!player.Weapons[(int)weaponType].Ammo->HasInfinite())
						{
							reloadHarpoonGun = true;
						}

						break;

					case LaraWeaponType::RocketLauncher:
						FireRocket(laraItem);
						break;

					case LaraWeaponType::GrenadeLauncher:
						FireGrenade(laraItem);
						break;

					case LaraWeaponType::Crossbow:
						FireCrossbow(laraItem);
						break;

					case LaraWeaponType::HK:
						if ((weapon.WeaponMode == LaraWeaponTypeCarried::WTYPE_AMMO_2 ||
							weapon.WeaponMode == LaraWeaponTypeCarried::WTYPE_AMMO_3) &&
							player.Control.Weapon.Interval)
						{
							item.Animation.TargetState = WEAPON_STATE_AIM;
						}
						else
						{
							FireHK(laraItem, false);
							player.Control.Weapon.Timer = 1.0f;
							item.Animation.TargetState = WEAPON_STATE_RECOIL;

							SoundEffect(SFX_TR4_EXPLOSION1, &laraItem.Pose, SoundEnvironment::Land, 1.0f, 0.4f);
							SoundEffect(SFX_TR4_HK_FIRE, &laraItem.Pose);
						}

						break;
					}

					if (weaponType != LaraWeaponType::HK)
						item.Animation.TargetState = WEAPON_STATE_RECOIL;
				}
				else if (player.LeftArm.Locked)
				{
					item.Animation.TargetState = WEAPON_STATE_AIM;
				}
			}

			if (item.Animation.TargetState != WEAPON_STATE_RECOIL &&
				player.Control.Weapon.Timer != 0.0f && weaponType == LaraWeaponType::HK)
			{
				StopSoundEffect(SFX_TR4_HK_FIRE);
				SoundEffect(SFX_TR4_HK_STOP, &laraItem.Pose);
				player.Control.Weapon.Timer = 0.0f;
			}
		}
		else if (player.Control.Weapon.Timer != 0.0f)
		{
			SoundEffect(SFX_TR4_EXPLOSION1, &laraItem.Pose, SoundEnvironment::Land, 1.0f, 0.4f);
			SoundEffect(SFX_TR4_HK_FIRE, &laraItem.Pose);
		}
		else if (weaponType == LaraWeaponType::Shotgun && !IsHeld(In::Action) && !player.LeftArm.Locked)
		{
			item.Animation.TargetState = WEAPON_STATE_UNAIM;
		}

		if (item.Animation.FrameNumber == 12 &&
			weaponType == LaraWeaponType::Shotgun)
		{
			TriggerGunShell(1, ID_SHOTGUNSHELL, LaraWeaponType::Shotgun);
		}

		break;

	case WEAPON_STATE_UNDERWATER_RECOIL:
		if (item.Animation.FrameNumber == 0)
		{
			item.Animation.TargetState = WEAPON_STATE_UNDERWATER_UNAIM;

			if ((player.Control.WaterStatus == WaterStatus::Underwater || isRunning) && !reloadHarpoonGun)
			{
				if (IsHeld(In::Action) && (!player.TargetEntity || player.LeftArm.Locked))
				{
					switch (weaponType)
					{
					default:
						item.Animation.TargetState = WEAPON_STATE_UNDERWATER_AIM;
						break;

					case LaraWeaponType::HarpoonGun:
						FireHarpoon(laraItem);

						if (!(player.Weapons[(int)LaraWeaponType::HarpoonGun].Ammo->GetCount() % 4) &&
							!player.Weapons[(int)weaponType].Ammo->HasInfinite())
						{
							reloadHarpoonGun = true;
						}

						break;

					case LaraWeaponType::HK:
						if ((weapon.WeaponMode == LaraWeaponTypeCarried::WTYPE_AMMO_2 ||
							weapon.WeaponMode == LaraWeaponTypeCarried::WTYPE_AMMO_3) &&
							player.Control.Weapon.Interval != 0.0f)
						{
							item.Animation.TargetState = WEAPON_STATE_UNDERWATER_AIM;
						}
						else
						{
							FireHK(laraItem, true);
							player.Control.Weapon.Timer = 1.0f;
							item.Animation.TargetState = WEAPON_STATE_UNDERWATER_RECOIL;

							SoundEffect(SFX_TR4_EXPLOSION1, &laraItem.Pose, SoundEnvironment::Land, 1.0f, 0.4f);
							SoundEffect(SFX_TR4_HK_FIRE, &laraItem.Pose);
						}

						break;
					}

					if (weaponType != LaraWeaponType::HK)
						item.Animation.TargetState = WEAPON_STATE_UNDERWATER_RECOIL;
				}
				else if (player.LeftArm.Locked)
				{
					item.Animation.TargetState = WEAPON_STATE_UNDERWATER_AIM;
				}
			}

			if (item.Animation.TargetState != WEAPON_STATE_UNDERWATER_RECOIL &&
				player.Control.Weapon.Timer)
			{
				StopSoundEffect(SFX_TR4_HK_FIRE);
				SoundEffect(SFX_TR4_HK_STOP, &laraItem.Pose);
				player.Control.Weapon.Timer = 0.0f;
			}
		}
		else if (player.Control.Weapon.Timer != 0.0f)
		{
			SoundEffect(SFX_TR4_EXPLOSION1, &laraItem.Pose, SoundEnvironment::Land, 1.0f, 0.4f);
			SoundEffect(SFX_TR4_HK_FIRE, &laraItem.Pose);
		}

		break;

	default:
		break;
	}

	item.Pose.Position = laraItem.Pose.Position;
	item.RoomNumber = laraItem.RoomNumber;

	AnimateItem(item);

	player.LeftArm.AnimObjectID =
	player.RightArm.AnimObjectID = GetWeaponObjectID(weaponType);
	player.LeftArm.AnimNumber =
	player.RightArm.AnimNumber = item.Animation.AnimNumber;
	player.LeftArm.FrameNumber =
	player.RightArm.FrameNumber = item.Animation.FrameNumber;
}

void ReadyShotgun(ItemInfo& laraItem, LaraWeaponType weaponType)
{
	auto& player = GetLaraInfo(laraItem);
	auto animObjectID = GetWeaponObjectID(weaponType);
	auto& animObject = Objects[animObjectID];

	player.Control.HandStatus = HandStatus::WeaponReady;
	player.LeftArm.Orientation =
	player.RightArm.Orientation = EulerAngles::Identity;
	player.LeftArm.AnimObjectID =
	player.RightArm.AnimObjectID = GetWeaponObjectID(weaponType);
	player.LeftArm.FrameNumber =
	player.RightArm.FrameNumber = 0;
	player.LeftArm.Locked =
	player.RightArm.Locked = false;
	player.TargetEntity = nullptr;
}

void FireShotgun(ItemInfo& laraItem)
{
	auto& player = *GetLaraInfo(&laraItem);

	auto& ammo = GetAmmo(player, LaraWeaponType::Shotgun);
	if (!ammo.HasInfinite() && !ammo.GetCount())
		return;

	auto armOrient = EulerAngles(
		player.LeftArm.Orientation.x,
		player.LeftArm.Orientation.y + laraItem.Pose.Orientation.y,
		0);

	if (!player.LeftArm.Locked)
		armOrient += EulerAngles(player.ExtraTorsoRot.x, player.ExtraTorsoRot.y, 0);

	bool hasFired = false;
	int scatter = Weapons[(int)LaraWeaponType::Shotgun].ShotAccuracy * 
		(player.Weapons[(int)LaraWeaponType::Shotgun].SelectedAmmo == WeaponAmmoType::Ammo1) ? 1 : 3;

	for (int i = 0; i < SHOTGUN_PELLET_COUNT; i++)
	{
		auto wobbledArmOrient = EulerAngles(
			armOrient.x + scatter * (GetRandomControl() - ANGLE(90.0f)) / 65536,
			armOrient.y + scatter * (GetRandomControl() - ANGLE(90.0f)) / 65536,
			0);

		if (FireWeapon(LaraWeaponType::Shotgun, player.TargetEntity, laraItem, wobbledArmOrient) != FireWeaponType::NoAmmo)
			hasFired = true;

		// HACK: Compensate for spending 6 units of shotgun ammo. -- Lwmte, 18.11.22
		if (!ammo.HasInfinite())
			ammo++;
	}

	if (hasFired)
	{
		if (!ammo.HasInfinite())
			ammo--;

		auto pos = GetJointPosition(&laraItem, LM_RHAND, Vector3i(0, 1508, 32));
		auto pos2 = GetJointPosition(&laraItem, LM_RHAND, Vector3i(0, 228, 32));

		player.LeftArm.GunSmoke = 32;

		if (laraItem.MeshBits != 0)
		{
			for (int i = 0; i < 7; i++)
			{
				TriggerGunSmoke(
					pos2.x, pos2.y, pos2.z,
					pos.x - pos2.x, pos.y - pos2.y, pos.z - pos2.z,
					1, LaraWeaponType::Shotgun, player.LeftArm.GunSmoke);
			}
		}

		player.RightArm.GunFlash = Weapons[(int)LaraWeaponType::Shotgun].FlashTime;

		SoundEffect(SFX_TR4_EXPLOSION1, &laraItem.Pose, TestEnvironment(ENV_FLAG_WATER, &laraItem) ? SoundEnvironment::Underwater : SoundEnvironment::Land);
		SoundEffect(Weapons[(int)LaraWeaponType::Shotgun].SampleNum, &laraItem.Pose);

		Rumble(0.5f, 0.2f);

		SaveGame::Statistics.Level.AmmoUsed++;
		SaveGame::Statistics.Game.AmmoUsed++;
	}
}

void DrawShotgun(ItemInfo& laraItem, LaraWeaponType weaponType)
{
	auto& player = *GetLaraInfo(&laraItem);

	ItemInfo* weaponItemPtr = nullptr;

	if (player.Control.Weapon.WeaponItem == NO_VALUE)
	{
		player.Control.Weapon.WeaponItem = CreateItem();
		weaponItemPtr = &g_Level.Items[player.Control.Weapon.WeaponItem];
		weaponItemPtr->ObjectNumber =
		weaponItemPtr->Animation.AnimObjectID = GetWeaponObjectID(weaponType);

		if (weaponType == LaraWeaponType::RocketLauncher)
		{
			weaponItemPtr->Animation.AnimNumber = 1;
		}
		else if (weaponType == LaraWeaponType::GrenadeLauncher)
		{
			weaponItemPtr->Animation.AnimNumber = 0;
		}
		else
		{
			weaponItemPtr->Animation.AnimNumber = 1;
		}

		weaponItemPtr->Animation.AnimObjectID = weaponItemPtr->ObjectNumber;
		weaponItemPtr->Animation.FrameNumber = 0;
		weaponItemPtr->Animation.ActiveState =
		weaponItemPtr->Animation.TargetState = WEAPON_STATE_DRAW;
		weaponItemPtr->Status = ITEM_ACTIVE;
		weaponItemPtr->RoomNumber = NO_VALUE;
		weaponItemPtr->Pose = laraItem.Pose;

		player.LeftArm.AnimObjectID =
		player.RightArm.AnimObjectID = weaponItemPtr->ObjectNumber;
	}
	else
	{
		weaponItemPtr = &g_Level.Items[player.Control.Weapon.WeaponItem];
	}

	AnimateItem(*weaponItemPtr);

	if (weaponItemPtr->Animation.ActiveState != WEAPON_STATE_AIM &&
		weaponItemPtr->Animation.ActiveState != WEAPON_STATE_UNDERWATER_AIM)
	{
		if (weaponItemPtr->Animation.FrameNumber == Weapons[(int)weaponType].DrawFrame)
		{
			DrawShotgunMeshes(laraItem, weaponType);
		}
		else if (player.Control.WaterStatus == WaterStatus::Underwater)
		{
			weaponItemPtr->Animation.TargetState = WEAPON_STATE_UNDERWATER_AIM;
		}
	}
	else
	{
		ReadyShotgun(laraItem, weaponType);
	}

	player.LeftArm.AnimObjectID =
	player.RightArm.AnimObjectID = GetWeaponObjectID(weaponType);
	player.LeftArm.AnimNumber =
	player.RightArm.AnimNumber = weaponItemPtr->Animation.AnimNumber;
	player.LeftArm.FrameNumber =
	player.RightArm.FrameNumber = weaponItemPtr->Animation.FrameNumber;
}

void UndrawShotgun(ItemInfo& laraItem, LaraWeaponType weaponType)
{
	auto& player = GetLaraInfo(laraItem);
	auto& item = g_Level.Items[player.Control.Weapon.WeaponItem];

	// HACK
	if (item.Animation.TargetState != WEAPON_STATE_UNDRAW && item.Animation.ActiveState != WEAPON_STATE_UNDRAW)
		SetAnimation(item, (weaponType == LaraWeaponType::GrenadeLauncher) ? 5 : 3);

	item.Animation.TargetState = WEAPON_STATE_UNDRAW;
	item.Pose = laraItem.Pose;
	AnimateItem(item);

	if (item.Status == ITEM_DEACTIVATED)
	{
		KillItem(player.Control.Weapon.WeaponItem);
		player.Control.Weapon.WeaponItem = NO_VALUE;
		player.Control.HandStatus = HandStatus::Free;
		player.TargetEntity = nullptr;
		player.LeftArm.Locked =
		player.RightArm.Locked = false;
		player.LeftArm.FrameNumber =
		player.RightArm.FrameNumber = 0;
	}
	else if (item.Animation.ActiveState == WEAPON_STATE_UNDRAW)
	{
		if (item.Animation.FrameNumber == 21 ||
			(weaponType == LaraWeaponType::GrenadeLauncher && item.Animation.FrameNumber == 15))
		{
			UndrawShotgunMeshes(laraItem, weaponType);
		}
	}

	// HACK: Get reholstering animation number.
	int animNumber = 0;
	switch (weaponType)
	{
	case LaraWeaponType::GrenadeLauncher:
		animNumber = 5;
		break;

	default:
		animNumber = 3;
		break;
	}
	
	player.LeftArm.AnimObjectID =
	player.RightArm.AnimObjectID = GetWeaponObjectID(weaponType);
	player.RightArm.AnimNumber =
	player.LeftArm.AnimNumber = animNumber;
	player.RightArm.FrameNumber =
	player.LeftArm.FrameNumber = item.Animation.FrameNumber;
}

void DrawShotgunMeshes(ItemInfo& laraItem, LaraWeaponType weaponType)
{
	auto& player = *GetLaraInfo(&laraItem);

	player.Control.Weapon.HolsterInfo.BackHolster = HolsterSlot::Empty;
	laraItem.Model.MeshIndex[LM_RHAND] = Objects[GetWeaponObjectMeshID(laraItem, weaponType)].meshIndex + LM_RHAND;
}

void UndrawShotgunMeshes(ItemInfo& laraItem, LaraWeaponType weaponType)
{
	auto& player = *GetLaraInfo(&laraItem);

	laraItem.Model.MeshIndex[LM_RHAND] = laraItem.Model.BaseMesh + LM_RHAND;

	if (player.Weapons[(int)weaponType].Present)
	{
		player.Control.Weapon.HolsterInfo.BackHolster = GetWeaponHolsterSlot(weaponType);
	}
	else
	{
		player.Control.Weapon.HolsterInfo.BackHolster = HolsterSlot::Empty;
	}
}

bool FireHarpoon(ItemInfo& laraItem, const std::optional<Pose>& pose)
{
	auto& player = *GetLaraInfo(&laraItem);
	auto& ammo = GetAmmo(player, LaraWeaponType::HarpoonGun);

	if (!ammo)
		return false;

	player.Control.Weapon.HasFired = true;

	int itemNumber = CreateItem();
	if (itemNumber == NO_VALUE)
		return false;

	auto& harpoonItem = g_Level.Items[itemNumber];

	harpoonItem.ObjectNumber = ID_HARPOON;
	harpoonItem.Model.Color = Vector4(0.5f, 0.5f, 0.5f, 1.0f);

	if (!ammo.HasInfinite())
		ammo--;

	if (pose.has_value())
	{
		harpoonItem.Pose.Position = pose->Position;
		harpoonItem.RoomNumber = laraItem.RoomNumber;

		InitializeItem(itemNumber);

		harpoonItem.Pose.Orientation = EulerAngles(
			player.LeftArm.Orientation.x + laraItem.Pose.Orientation.x,
			player.LeftArm.Orientation.y + laraItem.Pose.Orientation.y,
			0);
	}
	else
	{
		auto jointPos = GetJointPosition(&laraItem, LM_RHAND, Vector3i(-2, 373, 77));
		harpoonItem.RoomNumber = laraItem.RoomNumber;

		int floorHeight = GetPointCollision(jointPos, harpoonItem.RoomNumber).GetFloorHeight();
		if (floorHeight >= jointPos.y)
		{
			harpoonItem.Pose.Position = jointPos;
		}
		else
		{
			harpoonItem.Pose.Position = Vector3i(laraItem.Pose.Position.x, jointPos.y, laraItem.Pose.Position.z);
			harpoonItem.RoomNumber = laraItem.RoomNumber;
		}

		InitializeItem(itemNumber);

		harpoonItem.Pose.Orientation.x = player.LeftArm.Orientation.x + laraItem.Pose.Orientation.x;
		harpoonItem.Pose.Orientation.z = 0;
		harpoonItem.Pose.Orientation.y = player.LeftArm.Orientation.y + laraItem.Pose.Orientation.y;

		if (!player.LeftArm.Locked)
			harpoonItem.Pose.Orientation += player.ExtraTorsoRot;
	}

	harpoonItem.Animation.Velocity.z = HARPOON_VELOCITY;
	harpoonItem.HitPoints = HARPOON_TIME;

	AddActiveItem(itemNumber);

	harpoonItem.ItemFlags[0] = (int)ProjectileType::Harpoon;

	Rumble(0.2f, 0.1f);

	SaveGame::Statistics.Level.AmmoUsed++;
	SaveGame::Statistics.Game.AmmoUsed++;

	return true;
}
	
void HarpoonBoltControl(short itemNumber)
{
	auto& harpoonItem = g_Level.Items[itemNumber];

	if (harpoonItem.HitPoints < HARPOON_TIME)
	{
		harpoonItem.HitPoints--;

		if (harpoonItem.HitPoints == 0)
		{
			ExplodeItemNode(&harpoonItem, 0, 0, BODY_DO_EXPLOSION);
			KillItem(itemNumber);
		}

		return;
	}

	if (TestEnvironment(ENV_FLAG_WATER, &harpoonItem))
	{
		if (harpoonItem.Animation.Velocity.z > 64.0f)
			harpoonItem.Animation.Velocity.z -= harpoonItem.Animation.Velocity.z / 16;

		if (GlobalCounter & 1)
			SpawnBubble(harpoonItem.Pose.Position.ToVector3(), harpoonItem.RoomNumber);
	}

	auto prevPos = harpoonItem.Pose.Position;
	harpoonItem.Pose.Translate(harpoonItem.Pose.Orientation, harpoonItem.Animation.Velocity.z);

	int damage = Weapons[(int)LaraWeaponType::HarpoonGun].Damage;
	HandleProjectile(harpoonItem, *LaraItem, prevPos, (ProjectileType)harpoonItem.ItemFlags[0], damage);
}

void FireGrenade(ItemInfo& laraItem)
{
	auto& player = *GetLaraInfo(&laraItem);
	auto& ammo = GetAmmo(player, LaraWeaponType::GrenadeLauncher);

	if (!ammo)
		return;

	player.Control.Weapon.HasFired = true;

	short itemNumber = CreateItem();
	if (itemNumber == NO_VALUE)
		return;

	auto& grenadeItem = g_Level.Items[itemNumber];
		
	grenadeItem.Model.Color = Vector4(0.5f, 0.5f, 0.5f, 1.0f);
	grenadeItem.ObjectNumber = ID_GRENADE;
	grenadeItem.RoomNumber = laraItem.RoomNumber;

	auto jointPos = GetJointPosition(&laraItem, LM_RHAND, Vector3i(0, 276, 80));
	grenadeItem.Pose.Position = jointPos;
	auto smokePos = jointPos;

	int floorHeight = GetPointCollision(jointPos, grenadeItem.RoomNumber).GetFloorHeight();
	if (floorHeight < jointPos.y)
	{
		grenadeItem.Pose.Position.x = laraItem.Pose.Position.x;
		grenadeItem.Pose.Position.y = jointPos.y;
		grenadeItem.Pose.Position.z = laraItem.Pose.Position.z;
		grenadeItem.RoomNumber = laraItem.RoomNumber;
	}

	jointPos = GetJointPosition(&laraItem, LM_RHAND, Vector3i(0, 1204, 5));

	player.LeftArm.GunSmoke = 32;

	if (laraItem.MeshBits.TestAny())
	{
		for (int i = 0; i < 5; i++)
		{
			TriggerGunSmoke(
				smokePos.x, smokePos.y, smokePos.z,
				jointPos.x - smokePos.x, jointPos.y - smokePos.y, jointPos.z - smokePos.z,
				1, LaraWeaponType::GrenadeLauncher, player.LeftArm.GunSmoke);
		}
	}

	InitializeItem(itemNumber);

	grenadeItem.Pose.Orientation = EulerAngles(
		laraItem.Pose.Orientation.x + player.LeftArm.Orientation.x + ANGLE(180.0f),
		laraItem.Pose.Orientation.y + player.LeftArm.Orientation.y,
		0);

	if (!player.LeftArm.Locked)
		grenadeItem.Pose.Orientation += player.ExtraTorsoRot;

	grenadeItem.Animation.Velocity.y = CLICK(1) * phd_sin(grenadeItem.Pose.Orientation.x);
	grenadeItem.Animation.Velocity.z = GRENADE_VELOCITY;
	grenadeItem.Animation.ActiveState = grenadeItem.Pose.Orientation.x;
	grenadeItem.Animation.TargetState = grenadeItem.Pose.Orientation.y;
	grenadeItem.Animation.RequiredState = NO_VALUE;
	grenadeItem.HitPoints = GRENADE_TIME;
	grenadeItem.ItemFlags[0] = (int)WeaponAmmoType::Ammo2;

	Rumble(0.4f, 0.25f);

	AddActiveItem(itemNumber);

	if (!ammo.HasInfinite())
		ammo--;

	switch (player.Weapons[(int)LaraWeaponType::GrenadeLauncher].SelectedAmmo)
	{
	case WeaponAmmoType::Ammo1:
		grenadeItem.ItemFlags[0] = (int)ProjectileType::Grenade;
		break;

	case WeaponAmmoType::Ammo2:
		grenadeItem.ItemFlags[0] = (int)ProjectileType::FragGrenade;
		break;

	case WeaponAmmoType::Ammo3:
		grenadeItem.ItemFlags[0] = (int)ProjectileType::FlashGrenade;
		break;

	default:
		break;
	}

	SaveGame::Statistics.Level.AmmoUsed++;
	SaveGame::Statistics.Game.AmmoUsed++;
}

void GrenadeControl(short itemNumber)
{
	auto& grenadeItem = g_Level.Items[itemNumber];

	grenadeItem.Model.Color = Vector4(0.5f, 0.5f, 0.5f, 1.0f);

	// Check if above water and update Y and Z velocities.
	bool aboveWater = false;
	if (TestEnvironment(ENV_FLAG_WATER, grenadeItem.RoomNumber) ||
		TestEnvironment(ENV_FLAG_SWAMP, grenadeItem.RoomNumber))
	{
		grenadeItem.Animation.Velocity.y += (5.0f - grenadeItem.Animation.Velocity.y) / 2;
		grenadeItem.Animation.Velocity.z -= grenadeItem.Animation.Velocity.z / 4;

		if (grenadeItem.Animation.Velocity.z)
		{
			grenadeItem.Pose.Orientation.z += (short((grenadeItem.Animation.Velocity.z / 16) + 3.0f) * ANGLE(1.0f));
			if (grenadeItem.Animation.RequiredState != NO_VALUE)
			{
				grenadeItem.Pose.Orientation.y += (short((grenadeItem.Animation.Velocity.z / 4) + 3.0f) * ANGLE(1.0f));
			}
			else
			{
				grenadeItem.Pose.Orientation.x += (short((grenadeItem.Animation.Velocity.z / 4) + 3.0f) * ANGLE(1.0f));
			}
		}
	}
	else
	{
		aboveWater = true;
		grenadeItem.Animation.Velocity.y += 3.0f;

		if (grenadeItem.Animation.Velocity.z)
		{
			grenadeItem.Pose.Orientation.z += (short((grenadeItem.Animation.Velocity.z / 4) + 7.0f) * ANGLE(1.0f));
			if (grenadeItem.Animation.RequiredState != NO_VALUE)
			{
				grenadeItem.Pose.Orientation.y += (short((grenadeItem.Animation.Velocity.z / 2) + 7.0f) * ANGLE(1.0f));
			}
			else
			{
				grenadeItem.Pose.Orientation.x += (short((grenadeItem.Animation.Velocity.z / 2) + 7.0f) * ANGLE(1.0f));
			}
		}
	}

	// Trigger fire and smoke sparks in direction of motion.
	if (grenadeItem.Animation.Velocity.z && aboveWater)
	{
		auto world = Matrix::CreateTranslation(0, 0, -64) * 
					 Matrix::CreateFromYawPitchRoll(
						TO_RAD(grenadeItem.Pose.Orientation.y - ANGLE(180.0f)),
						TO_RAD(grenadeItem.Pose.Orientation.x),
						TO_RAD(grenadeItem.Pose.Orientation.z));

		int wx = world.Translation().x;
		int wy = world.Translation().y;
		int wz = world.Translation().z;

		TriggerRocketSmoke(wx + grenadeItem.Pose.Position.x, wy + grenadeItem.Pose.Position.y, wz + grenadeItem.Pose.Position.z);
		TriggerRocketFire(wx + grenadeItem.Pose.Position.x, wy + grenadeItem.Pose.Position.y, wz + grenadeItem.Pose.Position.z);
	}

	auto vel = Vector3i(
		grenadeItem.Animation.Velocity.z * phd_sin(grenadeItem.Animation.TargetState),
		grenadeItem.Animation.Velocity.y,
		grenadeItem.Animation.Velocity.z * phd_cos(grenadeItem.Animation.TargetState));

	// Update grenade position.
	auto prevPos = grenadeItem.Pose.Position;
	grenadeItem.Pose.Position += vel;

	// Do dynamics only for first-order grenades (not fragments, which have ProjectileType set to Explosive).
	if (grenadeItem.ItemFlags[0] != (int)ProjectileType::Explosive)
	{
		// Do grenade physics.
		short sYOrient = grenadeItem.Pose.Orientation.y;
		grenadeItem.Pose.Orientation.y = grenadeItem.Animation.TargetState;

		DoProjectileDynamics(itemNumber, prevPos.x, prevPos.y, prevPos.z, vel.x, vel.y, vel.z);

		grenadeItem.Animation.TargetState = grenadeItem.Pose.Orientation.y;
		grenadeItem.Pose.Orientation.y = sYOrient;
	}

	HandleProjectile(grenadeItem, *LaraItem, prevPos, (ProjectileType)grenadeItem.ItemFlags[0], Weapons[(int)LaraWeaponType::GrenadeLauncher].Damage);
}

void FireRocket(ItemInfo& laraItem)
{
	auto& player = *GetLaraInfo(&laraItem);
	auto& ammo = GetAmmo(player, LaraWeaponType::RocketLauncher);

	if (!ammo)
		return;

	player.Control.Weapon.HasFired = true;

	short itemNumber = CreateItem();
	if (itemNumber == NO_VALUE)
		return;

	auto& rocketItem = g_Level.Items[itemNumber];
	rocketItem.ObjectNumber = ID_ROCKET;
	rocketItem.RoomNumber = laraItem.RoomNumber;

	if (!ammo.HasInfinite())
		ammo--;

	auto jointPos = GetJointPosition(&laraItem, LM_RHAND, Vector3i(0, 180, 72));

	auto pos =
	rocketItem.Pose.Position = jointPos;

	jointPos = GetJointPosition(&laraItem, LM_RHAND, Vector3i(0, 2004, 72));

	player.LeftArm.GunSmoke = 32;

	for (int i = 0; i < 5; i++)
	{
		TriggerGunSmoke(
			pos.x, pos.y, pos.z,
			jointPos.x - pos.x, jointPos.y - pos.y, jointPos.z - pos.z,
			1, LaraWeaponType::RocketLauncher, player.LeftArm.GunSmoke);
	}

	jointPos = GetJointPosition(&laraItem, LM_RHAND, Vector3i(0, -CLICK(1), 0));

	for (int i = 0; i < 10; i++)
	{
		TriggerGunSmoke(
			jointPos.x, jointPos.y, jointPos.z,
			jointPos.x - pos.x, jointPos.y - pos.y, jointPos.z - pos.z,
			2, LaraWeaponType::RocketLauncher, 32);
	}

	InitializeItem(itemNumber);

	rocketItem.Pose.Orientation = EulerAngles(
		laraItem.Pose.Orientation.x + player.LeftArm.Orientation.x,
		laraItem.Pose.Orientation.y + player.LeftArm.Orientation.y,
		0);
	rocketItem.HitPoints = ROCKET_TIME;

	if (!player.LeftArm.Locked)
	{
		rocketItem.Pose.Orientation.x += player.ExtraTorsoRot.x;
		rocketItem.Pose.Orientation.y += player.ExtraTorsoRot.y;
	}

	rocketItem.Animation.Velocity.z = 512.0f / 32.0f;
	rocketItem.ItemFlags[0] = 0;

	AddActiveItem(itemNumber);

	Rumble(0.4f, 0.3f);
	SoundEffect(SFX_TR4_EXPLOSION1, &laraItem.Pose);

	SaveGame::Statistics.Level.AmmoUsed++;
	SaveGame::Statistics.Game.AmmoUsed++;
}

void RocketControl(short itemNumber)
{
	auto& rocketItem = g_Level.Items[itemNumber];

	// Update velocity and orientation.
	if (TestEnvironment(ENV_FLAG_WATER, rocketItem.RoomNumber))
	{
		if (rocketItem.Animation.Velocity.z > (ROCKET_VELOCITY / 4))
		{
			rocketItem.Animation.Velocity.z -= rocketItem.Animation.Velocity.z / 4;
		}
		else
		{
			rocketItem.Animation.Velocity.z += (rocketItem.Animation.Velocity.z / 4) + 4.0f;

			if (rocketItem.Animation.Velocity.z > (ROCKET_VELOCITY / 4))
				rocketItem.Animation.Velocity.z = ROCKET_VELOCITY / 4;
		}

		rocketItem.Pose.Orientation.z += short((rocketItem.Animation.Velocity.z / 8) + 3.0f) * ANGLE(1.0f);
	}
	else
	{
		if (rocketItem.Animation.Velocity.z < ROCKET_VELOCITY)
			rocketItem.Animation.Velocity.z += (rocketItem.Animation.Velocity.z / 4) + 4.0f;

		rocketItem.Pose.Orientation.z += short((rocketItem.Animation.Velocity.z / 4) + 7.0f) * ANGLE(1.0f);
	}

	rocketItem.Model.Color = Vector4(0.5f, 0.5f, 0.5f, 1.0f);

	// Calculate offset in rocket direction for fire and smoke sparks.
	auto world = Matrix::CreateTranslation(0, 0, -64) *
				 Matrix::CreateFromYawPitchRoll(
					TO_RAD(rocketItem.Pose.Orientation.y - ANGLE(180.0f)),
					TO_RAD(rocketItem.Pose.Orientation.x),
					TO_RAD(rocketItem.Pose.Orientation.z));

	int wx = world.Translation().x;
	int wy = world.Translation().y;
	int wz = world.Translation().z;

	// Trigger fire, smoke, and light.
	TriggerRocketSmoke(wx + rocketItem.Pose.Position.x, wy + rocketItem.Pose.Position.y, wz + rocketItem.Pose.Position.z);
	TriggerRocketFire(wx + rocketItem.Pose.Position.x, wy + rocketItem.Pose.Position.y, wz + rocketItem.Pose.Position.z);
	SpawnDynamicLight(
		wx + rocketItem.Pose.Position.x + (GetRandomControl() & 15) - 8, 
		wy + rocketItem.Pose.Position.y + (GetRandomControl() & 15) - 8, 
		wz + rocketItem.Pose.Position.z + (GetRandomControl() & 15) - 8, 
		14, 28 + (GetRandomControl() & 3), 16 + (GetRandomControl() & 7), (GetRandomControl() & 7));

	// Spawn bubbles underwater.
	if (TestEnvironment(ENV_FLAG_WATER, rocketItem.RoomNumber))
	{
		auto pos = rocketItem.Pose.Position.ToVector3() + Vector3(wx, wy, wz);
		SpawnBubble(pos, rocketItem.RoomNumber);
	}

	// Update rocket position.
	auto prevPos = rocketItem.Pose.Position;
	rocketItem.Pose.Translate(rocketItem.Pose.Orientation, rocketItem.Animation.Velocity.z);

	HandleProjectile(rocketItem, *LaraItem, prevPos, ProjectileType::Explosive, Weapons[(int)LaraWeaponType::RocketLauncher].Damage);
}

void FireCrossbow(ItemInfo& laraItem, const std::optional<Pose>& pose)
{
	auto& player = *GetLaraInfo(&laraItem);
	auto& ammo = GetAmmo(player, LaraWeaponType::Crossbow);

	if (!ammo)
		return;

	player.Control.Weapon.HasFired = true;

	short itemNumber = CreateItem();
	if (itemNumber == NO_VALUE)
		return;

	auto& boltItem = g_Level.Items[itemNumber];
	boltItem.ObjectNumber = ID_CROSSBOW_BOLT;
	boltItem.Model.Color = Vector4(0.5f, 0.5f, 0.5f, 1.0f);

	if (!ammo.HasInfinite())
		ammo--;

	if (pose.has_value())
	{
		boltItem.Pose.Position = pose->Position;
		boltItem.RoomNumber = laraItem.RoomNumber;

		InitializeItem(itemNumber);

		boltItem.Pose.Orientation = pose->Orientation;
	}
	else
	{
		auto jointPos = GetJointPosition(&laraItem, LM_RHAND, Vector3i(0, 228, 32));

		boltItem.RoomNumber = laraItem.RoomNumber;

		int floorHeight = GetPointCollision(jointPos, boltItem.RoomNumber).GetFloorHeight();
		if (floorHeight >= jointPos.y)
		{
			boltItem.Pose.Position = jointPos;
		}
		else
		{
			boltItem.Pose.Position = Vector3i(laraItem.Pose.Position.x, jointPos.y, laraItem.Pose.Position.z);
			boltItem.RoomNumber = laraItem.RoomNumber;
		}

		InitializeItem(itemNumber);

		boltItem.Pose.Orientation.x = player.LeftArm.Orientation.x + laraItem.Pose.Orientation.x;
		boltItem.Pose.Orientation.z = 0;
		boltItem.Pose.Orientation.y = player.LeftArm.Orientation.y + laraItem.Pose.Orientation.y;

		if (!player.LeftArm.Locked)
		{
			boltItem.Pose.Orientation += player.ExtraTorsoRot;
		}
	}

	boltItem.Animation.Velocity.z = BOLT_VELOCITY;
	boltItem.HitPoints = BOLT_TIME;

	AddActiveItem(itemNumber);

	switch (player.Weapons[(int)LaraWeaponType::Crossbow].SelectedAmmo)
	{
	case WeaponAmmoType::Ammo1:
		boltItem.ItemFlags[0] = (int)ProjectileType::Normal;
		break;

	case WeaponAmmoType::Ammo2:
		boltItem.ItemFlags[0] = (int)ProjectileType::Poison;
		break;

	case WeaponAmmoType::Ammo3:
		boltItem.ItemFlags[0] = (int)ProjectileType::Explosive;
		break;

	default:
		break;
	}

	Rumble(0.2f, 0.1f);
	SoundEffect(SFX_TR4_CROSSBOW_FIRE, &laraItem.Pose);

	SaveGame::Statistics.Level.AmmoUsed++;
	SaveGame::Statistics.Game.AmmoUsed++;
}

void FireCrossBowFromLaserSight(ItemInfo& laraItem, GameVector* origin, GameVector* target)
{
	auto orient = Geometry::GetOrientToPoint(origin->ToVector3(), target->ToVector3());
	auto boltPose = Pose(origin->ToVector3i(), orient);
	FireCrossbow(laraItem, boltPose);
}

void CrossbowBoltControl(short itemNumber)
{
	auto& boltItem = g_Level.Items[itemNumber];

	if (TestEnvironment(ENV_FLAG_WATER, &boltItem))
	{
		if (boltItem.Animation.Velocity.z > 64.0f)
			boltItem.Animation.Velocity.z -= boltItem.Animation.Velocity.z / 16;

		if (GlobalCounter & 1)
			SpawnBubble(boltItem.Pose.Position.ToVector3(), boltItem.RoomNumber);
	}

	auto prevPos = boltItem.Pose.Position;
	boltItem.Pose.Translate(boltItem.Pose.Orientation, boltItem.Animation.Velocity.z);

	int damage = (boltItem.ItemFlags[0] == (int)ProjectileType::Explosive) ?
		Weapons[(int)LaraWeaponType::Crossbow].AlternateDamage : Weapons[(int)LaraWeaponType::Crossbow].Damage;

	HandleProjectile(boltItem, *LaraItem, prevPos, (ProjectileType)boltItem.ItemFlags[0], damage);
}

void FireHK(ItemInfo& laraItem, bool inaccurateMode)
{
	auto& player = *GetLaraInfo(&laraItem);
	const auto& weapon = player.Weapons[(int)LaraWeaponType::HK];

	auto angles = EulerAngles(
		player.LeftArm.Orientation.x,
		player.LeftArm.Orientation.y + laraItem.Pose.Orientation.y,
		0);

	if (weapon.WeaponMode == LaraWeaponTypeCarried::WTYPE_AMMO_3)
	{
		player.Control.Weapon.Interval = HK_BURST_AND_SNIPER_MODE_SHOT_INTERVAL;
	}
	else if (weapon.WeaponMode == LaraWeaponTypeCarried::WTYPE_AMMO_2)
	{
		player.Control.Weapon.NumShotsFired++;
		if (player.Control.Weapon.NumShotsFired == HK_BURST_MODE_SHOT_COUNT)
		{
			player.Control.Weapon.NumShotsFired = 0;
			player.Control.Weapon.Interval = HK_BURST_AND_SNIPER_MODE_SHOT_INTERVAL;
		}
	}

	if (!player.LeftArm.Locked)
	{
		angles = EulerAngles(
			player.ExtraTorsoRot.x + player.LeftArm.Orientation.x,
			player.ExtraTorsoRot.y + player.LeftArm.Orientation.y + laraItem.Pose.Orientation.y,
			0);
	}
	
	// HACK: Backup unmodified accuracy/damage values.
	short accuracy = Weapons[(int)LaraWeaponType::HK].ShotAccuracy;
	int damage = Weapons[(int)LaraWeaponType::HK].Damage;

	if (inaccurateMode)
	{
		Weapons[(int)LaraWeaponType::HK].ShotAccuracy = accuracy * 3;
		Weapons[(int)LaraWeaponType::HK].Damage = damage / 3;
	}

	if (FireWeapon(LaraWeaponType::HK, player.TargetEntity, laraItem, angles) != FireWeaponType::NoAmmo)
	{
		player.LeftArm.GunSmoke = 12;

		TriggerGunShell(1, ID_GUNSHELL, LaraWeaponType::HK);
		player.RightArm.GunFlash = Weapons[(int)LaraWeaponType::HK].FlashTime;

		Rumble(0.2f, 0.1f);
	}

	// HACK: Restore accuracy/damage values.
	Weapons[(int)LaraWeaponType::HK].ShotAccuracy = accuracy;
	Weapons[(int)LaraWeaponType::HK].Damage = damage;
}

void LasersightWeaponHandler(ItemInfo& item, LaraWeaponType weaponType)
{
	auto& player = *GetLaraInfo(&item);
	auto& ammo = GetAmmo(player, player.Control.Weapon.GunType);
	const auto& weapon = player.Weapons[(int)weaponType];

	if (!player.Control.Look.IsUsingLasersight || !weapon.HasLasersight)
		return;

	bool isFiring = false;

	if (player.Control.Weapon.Interval != 0.0f)
		player.Control.Weapon.Interval -= 1.0f;

	if (player.Control.Weapon.Timer != 0.0f)
		player.Control.Weapon.Timer -= 1.0f;

	if (!IsHeld(In::Action) || player.Control.Weapon.Interval != 0.0f || !ammo)
	{
		if (!IsHeld(In::Action))
		{
			player.Control.Weapon.NumShotsFired = 0;
			Camera.bounce = 0;
		}
	}
	else
	{
		if (player.Control.Weapon.GunType == LaraWeaponType::Revolver)
		{
			player.Control.Weapon.Interval = 16.0f;
			SaveGame::Statistics.Level.AmmoUsed++;
			SaveGame::Statistics.Game.AmmoUsed++;
			isFiring = true;

			if (!ammo.HasInfinite())
				ammo--;

			Camera.bounce = -16 - (GetRandomControl() & 0x1F);
		}
		else if (player.Control.Weapon.GunType == LaraWeaponType::Crossbow)
		{
			player.Control.Weapon.Interval = 32.0f;
			isFiring = true;
		}
		else if (player.Control.Weapon.GunType == LaraWeaponType::HK)
		{
			bool playSound = false;

			if (weapon.WeaponMode == LaraWeaponTypeCarried::WTYPE_AMMO_3)
			{
				player.Control.Weapon.Interval = HK_BURST_AND_SNIPER_MODE_SHOT_INTERVAL;
				playSound = isFiring = true;
			}
			else if (weapon.WeaponMode == LaraWeaponTypeCarried::WTYPE_AMMO_2)
			{
				if (!player.Control.Weapon.Timer)
				{
					if (++player.Control.Weapon.NumShotsFired == HK_BURST_MODE_SHOT_COUNT)
					{
						player.Control.Weapon.NumShotsFired = 0;
						player.Control.Weapon.Interval = HK_BURST_AND_SNIPER_MODE_SHOT_INTERVAL;
					}

					player.Control.Weapon.Timer = HK_RAPID_MODE_SHOT_INTERVAL;
					isFiring = true;
				}

				playSound = true;
			}
			else
			{
				if (!player.Control.Weapon.Timer)
				{
					player.Control.Weapon.Timer = HK_RAPID_MODE_SHOT_INTERVAL;
					isFiring = true;
				}

				playSound = true;
			}

			if (playSound)
			{
				SoundEffect(SFX_TR4_EXPLOSION1, nullptr, SoundEnvironment::Land, 1.0f, 0.4f);
				SoundEffect(SFX_TR4_HK_FIRE, nullptr);
				Camera.bounce = -16 - (GetRandomControl() & 0x1F);
			}

			if (!ammo.HasInfinite() && isFiring)
				ammo--;
		}
	}

	GetTargetOnLOS(&Camera.pos, &Camera.target, true, isFiring);
}

void RifleHandler(ItemInfo& laraItem, LaraWeaponType weaponType)
{
	auto& player = *GetLaraInfo(&laraItem);
	const auto& weapon = Weapons[(int)weaponType];

	// Never handle weapons in binocular mode.
	if (player.Control.Look.OpticRange || player.Control.Look.IsUsingLasersight)
		return;

	FindNewTarget(laraItem, weapon);

	if (IsHeld(In::Action))
		LaraTargetInfo(laraItem, weapon);

	AimWeapon(laraItem, player.LeftArm, weapon);

	if (player.LeftArm.Locked)
	{
		player.ExtraTorsoRot = player.LeftArm.Orientation;

		if (Camera.oldType != CameraType::Look && player.Control.Look.OpticRange == 0)
			player.ExtraHeadRot = EulerAngles::Identity;
	}

	if (weaponType == LaraWeaponType::Revolver)
	{
		AnimatePistols(laraItem, LaraWeaponType::Revolver);
	}
	else
	{
		AnimateShotgun(laraItem, weaponType);
	}

	if (player.RightArm.GunFlash)
	{
		auto& settings = g_GameFlow->GetSettings()->Weapons[(int)weaponType - 1];
		
		auto color = Color(settings.FlashColor);
		color += Color(Random::GenerateFloat(-0.2f, 0.2f));

		if (weaponType == LaraWeaponType::Shotgun || weaponType == LaraWeaponType::HK)
		{
			auto pos = GetJointPosition(&laraItem, LM_RHAND, Vector3i(0, -64, 0));
			SpawnDynamicPointLight(pos.ToVector3(), color, CLICK(settings.FlashRange));
		}
		else if (weaponType == LaraWeaponType::Revolver)
		{
			auto pos = GetJointPosition(&laraItem, LM_RHAND, Vector3i(0, -32, 0));
			SpawnDynamicPointLight(pos.ToVector3(), color, CLICK(settings.FlashRange));
		}
	}
}

void DoExplosiveDamage(ItemInfo& emitter, ItemInfo& target, ItemInfo& projectile, int damage)
{
	if (target.Flags & IFLAG_KILLED)
		return;

	if (target.HitPoints == NOT_TARGETABLE)
		return;

	if (&target != &emitter && !target.IsLara() && target.HitPoints > 0)
	{
		if (emitter.ItemFlags[2])
			return;

		target.HitStatus = true;
		HitTarget(&emitter, &target, nullptr, damage, true);

		if (&target != &emitter)
		{
			SaveGame::Statistics.Game.AmmoHits++;
			SaveGame::Statistics.Level.AmmoHits++;
			if (target.HitPoints <= 0)
			{
				SaveGame::Statistics.Game.Kills++;
				SaveGame::Statistics.Level.Kills++;
				CreatureDie(target.Index, true);
			}
		}
	}
	else if (target.IsLara())
	{
		DoDamage(&target, damage * 5);
		if (!TestEnvironment(ENV_FLAG_WATER, target.RoomNumber) && target.HitPoints <= 0)
			ItemBurn(&target);
	}
}

bool EmitFromProjectile(ItemInfo& projectile, ProjectileType type)
{
	if (!projectile.ItemFlags[1])
		return false;

	projectile.ItemFlags[1]--;

	if (!projectile.ItemFlags[1])
	{
		KillItem(projectile.Index);
		return true;
	}

	if (type == ProjectileType::FlashGrenade)
	{
		// Flash grenades.
		int r, g, b;
		if (projectile.ItemFlags[1] == 1)
		{
			FlashGrenadeAftershockTimer = 120;
			r = 255;
			g = 255;
			b = 255;
		}
		else
		{
			r = (GetRandomControl() & 0x1F) + 224;
			g = b = r - GetRandomControl() & 0x1F;
		}

		Weather.Flash(r, g, b, 0.03f);
		TriggerFlashSmoke(projectile.Pose.Position.x, projectile.Pose.Position.y, projectile.Pose.Position.z, projectile.RoomNumber);
	}
	else if (type == ProjectileType::FragGrenade)
	{
		// Trigger a new fragment in the case of GRENADE_SUPER until itemFlags[1] is > 0.
		int grenadeItemNumber = CreateItem();
		if (grenadeItemNumber == NO_VALUE)
			return true;

		auto& grenadeItem = g_Level.Items[grenadeItemNumber];

		grenadeItem.Model.Color = Vector4(0.5f, 0.5f, 0.5f, 1.0f);
		grenadeItem.ObjectNumber = ID_GRENADE;
		grenadeItem.RoomNumber = projectile.RoomNumber;
		grenadeItem.Pose.Position = Vector3i(
			Random::GenerateInt(0, BLOCK(0.5f)) + projectile.Pose.Position.x - CLICK(1),
			projectile.Pose.Position.y - CLICK(1),
			Random::GenerateInt(0, BLOCK(0.5f)) + projectile.Pose.Position.z - CLICK(1));

		InitializeItem(grenadeItemNumber);

		grenadeItem.Pose.Orientation = EulerAngles(
			Random::GenerateAngle(0, ANGLE(90.0f)) + ANGLE(45.0f),
			Random::GenerateAngle(0, ANGLE(359.0f)),
			0);
		grenadeItem.Animation.Velocity.y = -64.0f * phd_sin(grenadeItem.Pose.Orientation.x);
		grenadeItem.Animation.Velocity.z = 64.0f;
		grenadeItem.Animation.ActiveState = grenadeItem.Pose.Orientation.x;
		grenadeItem.Animation.TargetState = grenadeItem.Pose.Orientation.y;
		grenadeItem.Animation.RequiredState = NO_VALUE;

		AddActiveItem(grenadeItemNumber);

		grenadeItem.Status = ITEM_INVISIBLE;
		grenadeItem.ItemFlags[3] = 1;
		grenadeItem.HitPoints = 3000;
		grenadeItem.ItemFlags[0] = (short)ProjectileType::Explosive;

		if (TestEnvironment(ENV_FLAG_WATER, grenadeItem.RoomNumber))
			grenadeItem.HitPoints = Random::GenerateInt(5, 15);
	}

	return true;
}

bool TestProjectileNewRoom(ItemInfo& item, PointCollisionData& pointColl)
{
	// Check if projectile changed room.
	if (item.RoomNumber == pointColl.GetRoomNumber())
		return false;

	// If currently in water and previously on land, spawn ripple.
	if (TestEnvironment(ENV_FLAG_WATER, item.RoomNumber) != TestEnvironment(ENV_FLAG_WATER, pointColl.GetRoomNumber()))
	{
		const auto& player = GetLaraInfo(item);

		int floorDiff = abs(pointColl.GetFloorHeight() - item.Pose.Position.y);
		int ceilingDiff = abs(pointColl.GetCeilingHeight() - item.Pose.Position.y);
		int yPoint = (floorDiff > ceilingDiff) ? pointColl.GetCeilingHeight() : pointColl.GetFloorHeight();

		if (player.Control.Weapon.GunType != LaraWeaponType::GrenadeLauncher && player.Control.Weapon.GunType != LaraWeaponType::RocketLauncher)
		{
			SpawnSplashDrips(
				Vector3(item.Pose.Position.x, yPoint, item.Pose.Position.z),
				item.RoomNumber, 3, false);

			SpawnRipple(Vector3(item.Pose.Position.x, yPoint, item.Pose.Position.z), item.RoomNumber, Random::GenerateInt(8, 16));
		}
		else		
		{
			Splash(&item);
		}
	}

	ItemNewRoom(item.Index, pointColl.GetRoomNumber());
	return true;
}

void ExplodeProjectile(ItemInfo& item, const Vector3i& prevPos)
{
	if (TestEnvironment(ENV_FLAG_WATER, item.RoomNumber))
	{
		TriggerUnderwaterExplosion(&item, 0);
	}
	else
	{
		TriggerShockwave(&item.Pose, 48, 304, 96, 128, 96, 0, 24, EulerAngles::Identity, 0, true, false, false, (int)ShockwaveStyle::Normal);
		item.Pose.Position.y += CLICK(1.0f / 2);
		TriggerExplosionSparks(prevPos.x, prevPos.y, prevPos.z, 3, -2, 0, item.RoomNumber);

		for (int j = 0; j < 2; j++)
			TriggerExplosionSparks(prevPos.x, prevPos.y, prevPos.z, 3, -1, 0, item.RoomNumber);
	}

	AlertNearbyGuards(&item);

	SoundEffect(SFX_TR4_EXPLOSION1, &item.Pose, SoundEnvironment::Land, 0.7f, 0.5f);
	SoundEffect(SFX_TR4_EXPLOSION2, &item.Pose);
}

void HandleProjectile(ItemInfo& projectile, ItemInfo& emitter, const Vector3i& prevPos, ProjectileType type, int damage)
{
	auto pointColl = GetPointCollision(projectile);

	bool hasHit = false;
	bool hasHitNotByEmitter = false;
	bool isExplosive = (type >= ProjectileType::Explosive);
	bool isShatterable = (type != ProjectileType::Harpoon);

	// For non-grenade projectiles, check for room collision.
	if (type < ProjectileType::Grenade)
	{
		if (pointColl.GetFloorHeight() < projectile.Pose.Position.y ||
			pointColl.GetCeilingHeight() > projectile.Pose.Position.y)
		{
			hasHit = hasHitNotByEmitter = true;
		}
	}
	// If projectile is timed grenade, try to emit from it according to flags.
	else if (EmitFromProjectile(projectile, type))
	{
		return;
	}

	// Fire trail and water collision for grenade fragments.
	if (type == ProjectileType::Explosive && projectile.ItemFlags[3])
	{
		TriggerFireFlame(projectile.Pose.Position.x, projectile.Pose.Position.y, projectile.Pose.Position.z, FlameType::Medium);
		if (TestEnvironment(ENV_FLAG_WATER, projectile.RoomNumber))
			hasHit = true;
	}

	TestProjectileNewRoom(projectile, pointColl);

	// Decrease launch timer (only if projectile is not harpoon or harpoon has hit wall).
	if (type != ProjectileType::Harpoon || hasHit)
		projectile.HitPoints--;

	// Increase trigger timeout (to prevent emitter from exploding).
	projectile.ItemFlags[2]++;

	bool doExplosion = false;
	bool doShatter = false;

	// Item has reached EOL; mark it to destroy.
	if (projectile.HitPoints <= 0)
	{
		doExplosion = isExplosive;
		doShatter = hasHit = true;
	}

	// Store list of objects which were already affected by this explosion.
	auto affectedObjects = std::vector<int>();

	// Step 0: Check for specific collision in small radius.
	// Step 1: If exploding, try smashing all objects in blast radius.

	for (int n = 0; n < 2; n++)
	{
		bool isFirstPass = (n == 0);

		// Projectile is already hit because of room collision, bypass 1st pass.
		if (isFirstPass && hasHit)
		{
			doExplosion = isExplosive;
			doShatter = isShatterable;
			isFirstPass = false;
		}

		// Use bigger radius for second pass, or break if projectile is not explosive.
		int radius = PROJECTILE_HIT_RADIUS;
		if (!isExplosive && !isFirstPass)
		{
			break;
		}
		else if (!isFirstPass)
		{
			radius = PROJECTILE_EXPLODE_RADIUS;
		}

		// Found possible collided items and statics.
		auto collObjects = GetCollidedObjects(projectile, true, false, radius);
		if (collObjects.IsEmpty())
			break;

		// Run through statics.
		for (auto* staticPtr : collObjects.Statics)
		{
			hasHit = hasHitNotByEmitter = doShatter = true;
			doExplosion = isExplosive;

			if (Statics[staticPtr->staticNumber].shatterType == ShatterType::None)
				continue;

			staticPtr->HitPoints -= damage;
			if (staticPtr->HitPoints <= 0)
				ShatterObject(nullptr, staticPtr, -128, projectile.RoomNumber, 0);

			if (!isExplosive)
				continue;

			TriggerExplosionSparks(staticPtr->pos.Position.x, staticPtr->pos.Position.y, staticPtr->pos.Position.z, 3, -2, 0, projectile.RoomNumber);
			auto pose = Pose(Vector3i(staticPtr->pos.Position.x, staticPtr->pos.Position.y - 128, staticPtr->pos.Position.z), EulerAngles(0, staticPtr->pos.Orientation.y, 0));
			TriggerShockwave(&pose, 40, 176, 64, 0, 96, 128, 16, EulerAngles::Identity, 0, true, false, false, (int)ShockwaveStyle::Normal);
		}

		// Run through items.
		for (auto* itemPtr : collObjects.Items)
		{
			// Object was already affected by collision, skip it.
			if (std::find(affectedObjects.begin(), affectedObjects.end(), itemPtr->Index) != affectedObjects.end())
				continue;

			const auto& currentObject = Objects[itemPtr->ObjectNumber];

			if ((currentObject.intelligent && currentObject.collision && itemPtr->Status == ITEM_ACTIVE) ||
				itemPtr->IsLara() ||
				(itemPtr->Flags & 0x40 && Objects[itemPtr->ObjectNumber].explodableMeshbits))
			{
				// If we collide with emitter, and there are no other objects around, 
				// don't process further in early launch stages.
				if (!hasHitNotByEmitter && itemPtr == &emitter && affectedObjects.empty())
				{
					// Non-grenade projectiles require larger timeout
					int timeout = type >= ProjectileType::Grenade ? TRIGGER_TIMEOUT : TRIGGER_TIMEOUT * 2;
					if (projectile.ItemFlags[2] < timeout)
						continue;
				}
				else
				{
					hasHitNotByEmitter = true;
				}

				hasHit = true;
				doShatter = !currentObject.intelligent && !itemPtr->IsLara();

				affectedObjects.push_back(itemPtr->Index);

				if (isExplosive)
				{
					doExplosion = isExplosive;
					if (type != ProjectileType::FlashGrenade && currentObject.damageType != DamageMode::None)
						DoExplosiveDamage(emitter, *itemPtr, projectile, damage);
				}
				else if (currentObject.damageType == DamageMode::Any)
				{
					if (type == ProjectileType::Poison)
					{
						if (itemPtr->IsCreature())
							GetCreatureInfo(itemPtr)->Poisoned = true;

						if (itemPtr->IsLara())
							GetLaraInfo(itemPtr)->Status.Poison += 5;
					}
					else
					{
						DoDamage(itemPtr, damage);
					}
				}
			}
			else if (itemPtr->ObjectNumber >= ID_SMASH_OBJECT1 &&
					 itemPtr->ObjectNumber <= ID_SMASH_OBJECT8)
			{
				doShatter = hasHit = true;
				doExplosion = isExplosive;

				// Smash objects are legacy objects from TRC. Make them explode in legacy way.
				ExplodeItemNode(itemPtr, 0, 0, 128);
				SmashObject(itemPtr->Index);
				KillItem(itemPtr->Index);
			}
			else if (currentObject.collision && itemPtr->Status != ITEM_INVISIBLE)
			{
				doShatter = hasHit = true;
				doExplosion = isExplosive;

				affectedObjects.push_back(itemPtr->Index);
			}
		}

		if (isFirstPass && !hasHit)
			return;

		if (!isFirstPass)
			break;
	}

	if (!doShatter && !doExplosion && !hasHit)
		return;

	if (doExplosion && isExplosive)
	{
		ExplodeProjectile(projectile, prevPos);
	}
	else if (doShatter)
	{
		ExplodeItemNode(&projectile, 0, 0, BODY_DO_EXPLOSION);
	}

	switch (type)
	{
	case ProjectileType::FlashGrenade:
		hasHit = false;
		projectile.ItemFlags[1] = GRENADE_FLASH_TIMEOUT;
		break;

	case ProjectileType::FragGrenade:
		hasHit = false;
		projectile.ItemFlags[1] = GRENADE_FRAG_TIMEOUT;
		return;

	case ProjectileType::Harpoon:
		if (affectedObjects.empty())
			return;
		break;

	default:
		break;
	}

	if (hasHit)
		KillItem(projectile.Index);
}
