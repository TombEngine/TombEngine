#pragma once
#include "Game/collision/collide_room.h"
#include "Game/Lara/lara_struct.h"
#include "Game/Lara/Context/Structs.h"

struct CollisionInfo;
struct ItemInfo;

using namespace TEN::Player;

// -----------------------------
// TEST FUNCTIONS
// For State Control & Collision
// -----------------------------

bool TestPlayerInteractAngle(const ItemInfo& item, short testAngle);
bool TestPlayerInteractAngle(short playerHeadingAngle, short testAngle);

bool TestLaraWall(const ItemInfo* item, float dist, float height);
bool TestLaraFacingCorner(const ItemInfo* item, short headingAngle, float dist);

void TestLaraWaterDepth(ItemInfo* item, CollisionInfo* coll);

bool TestLaraWeaponType(LaraWeaponType refWeaponType, const std::vector<LaraWeaponType>& weaponTypeList);
bool TestLaraTorchFlame(ItemInfo* item, ItemInfo* flameItem);
bool IsStandingWeapon(const ItemInfo* item, LaraWeaponType weaponType);
bool IsCrouching(const ItemInfo* item);
bool IsVaultState(int state);
bool IsJumpState(int state);
bool IsSideJumpState(int state);
bool IsRunJumpQueueableState(int state);
bool IsRunJumpCountableState(int state);

bool TestLaraPoleCollision(ItemInfo* item, CollisionInfo* coll, bool goingUp, float offset = 0.0f);
bool TestLaraPoleUp(ItemInfo* item, CollisionInfo* coll);
bool TestLaraPoleDown(ItemInfo* item, CollisionInfo* coll);
