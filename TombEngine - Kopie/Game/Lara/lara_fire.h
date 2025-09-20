#pragma once
#include "Game/Lara/lara.h"

class EulerAngles;
struct CollisionInfo;
struct ItemInfo;
namespace TEN::Scripting { struct Settings; };

using namespace TEN::Scripting;

enum class FireWeaponType
{
	Miss = -1,
	NoAmmo,
	PossibleHit
};

enum WeaponState
{
	WEAPON_STATE_AIM = 0,
	WEAPON_STATE_DRAW = 1,
	WEAPON_STATE_RECOIL = 2,
	WEAPON_STATE_UNDRAW = 3,
	WEAPON_STATE_UNAIM = 4,
	WEAPON_STATE_RELOAD = 5,
	WEAPON_STATE_UNDERWATER_AIM = 6,
	WEAPON_STATE_UNDERWATER_UNAIM = 7,
	WEAPON_STATE_UNDERWATER_RECOIL = 8
};

struct WeaponInfo
{
	std::pair<EulerAngles, EulerAngles> LockOrientConstraint  = {};
	std::pair<EulerAngles, EulerAngles> LeftOrientConstraint  = {};
	std::pair<EulerAngles, EulerAngles> RightOrientConstraint = {};

	short AimSpeed		  = 0;
	short ShotAccuracy	  = 0;
	int	  GunHeight		  = 0;
	float TargetDist	  = 0.0f;
	int	  Damage		  = 0;
	int	  RecoilFrame	  = 0;
	int	  FlashTime		  = 0;
	int	  DrawFrame		  = 0;
	int	  SampleNum		  = 0;
	int	  AlternateDamage = 0;
};

extern int FlashGrenadeAftershockTimer;
extern WeaponInfo Weapons[(int)LaraWeaponType::NumWeapons];

void InitializeNewWeapon(ItemInfo& laraItem);
void InitializeWeaponInfo(const Settings& settings);

const WeaponInfo& GetWeaponInfo(LaraWeaponType weaponType);
Ammo&			  GetAmmo(LaraInfo& lara, LaraWeaponType weaponType);
GameVector		  GetTargetPoint(ItemInfo& targetEntity);
HolsterSlot		  GetWeaponHolsterSlot(LaraWeaponType weaponType);
GAME_OBJECT_ID	  GetWeaponObjectID(LaraWeaponType weaponType);
GAME_OBJECT_ID	  GetWeaponObjectMeshID(ItemInfo& laraItem, LaraWeaponType weaponType);

void HandleWeapon(ItemInfo& laraItem);
void AimWeapon(ItemInfo& laraItem, ArmInfo& arm, const WeaponInfo& weaponInfo);
FireWeaponType FireWeapon(LaraWeaponType weaponType, ItemInfo* targetEntity, ItemInfo& laraItem, const EulerAngles& armOrient);

void FindNewTarget(ItemInfo& laraItem, const WeaponInfo& weaponInfo);
void LaraTargetInfo(ItemInfo& laraItem, const WeaponInfo& weaponInfo);
void HitTarget(ItemInfo* laraItem, ItemInfo* targetEntity, GameVector* hitPos, int damage, bool isExplosive, int bestJointIndex = NO_VALUE);

void SmashItem(short itemNumber);
