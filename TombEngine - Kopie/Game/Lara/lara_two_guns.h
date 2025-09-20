#pragma once

enum class LaraWeaponType;
struct ItemInfo;

void HandlePistols(ItemInfo& laraItem, LaraWeaponType weaponType);
void AnimatePistols(ItemInfo& laraItem, LaraWeaponType weaponType);

void DrawPistols(ItemInfo& laraItem, LaraWeaponType weaponType);
void DrawPistolMeshes(ItemInfo& laraItem, LaraWeaponType weaponType);

void UndrawPistols(ItemInfo& laraItem, LaraWeaponType weaponType);
void UndrawPistolMesh(ItemInfo& laraItem, LaraWeaponType weaponType, bool isRightWeapon);
