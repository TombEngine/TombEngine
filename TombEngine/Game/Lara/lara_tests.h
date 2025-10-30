#pragma once
#include "Game/collision/collide_room.h"
#include "Game/Lara/lara_struct.h"
#include "Game/Lara/lara_test_structs.h"

struct CollisionInfo;
struct ItemInfo;

// -----------------------------
// TEST FUNCTIONS
// For State Control & Collision
// -----------------------------

bool TestValidLedge(ItemInfo* item, CollisionInfo* coll, bool ignoreHeadroom = false, bool heightLimit = false);
bool TestValidLedgeAngle(ItemInfo* item, CollisionInfo* coll);

bool TestLaraHang(ItemInfo* item, CollisionInfo* coll);
bool TestLaraHangJump(ItemInfo* item, CollisionInfo* coll);
bool TestLaraHangJumpUp(ItemInfo* item, CollisionInfo* coll);
int  TestLaraEdgeCatch(ItemInfo* item, CollisionInfo* coll, int* edge);

bool TestLaraClimbIdle(ItemInfo* item, CollisionInfo* coll);
bool TestLaraHangOnClimbableWall(ItemInfo* item, CollisionInfo* coll);
bool TestLaraNearClimbableWall(ItemInfo* item, FloorInfo* floor = nullptr);

bool TestLaraValidHangPosition(ItemInfo* item, CollisionInfo* coll);
CornerType TestLaraHangCorner(ItemInfo* item, CollisionInfo* coll, float testAngle);
CornerTestResult TestItemAtNextCornerPosition(ItemInfo* item, CollisionInfo* coll, float angle, bool outer);
bool TestHangSwingIn(ItemInfo* item, CollisionInfo* coll);
bool TestLaraHangSideways(ItemInfo* item, CollisionInfo* coll, short angle);

bool TestLaraWall(const ItemInfo* item, float dist, float height);
bool TestLaraFacingCorner(const ItemInfo* item, short headingAngle, float dist);
bool LaraPositionOnLOS(ItemInfo* item, short angle, int distance);
int LaraFloorFront(ItemInfo* item, short angle, int distance);
int LaraCeilingFront(ItemInfo* item, short angle, int distance, int height);

bool TestPlayerWaterStepOut(ItemInfo* item, CollisionInfo* coll);
bool TestLaraWaterClimbOut(ItemInfo* item, CollisionInfo* coll);
bool TestLaraLadderClimbOut(ItemInfo* item, CollisionInfo* coll);
void TestLaraWaterDepth(ItemInfo* item, CollisionInfo* coll);

bool TestLaraWeaponType(LaraWeaponType refWeaponType, const std::vector<LaraWeaponType>& weaponTypeList);
bool TestLaraTorchFlame(ItemInfo* item, ItemInfo* flameItem);
bool IsStandingWeapon(const ItemInfo* item, LaraWeaponType weaponType);
bool IsCrouching(const ItemInfo* item);
bool IsVaultState(int state);
bool IsJumpState(int state);
bool IsRunJumpQueueableState(int state);
bool IsRunJumpCountableState(int state);

std::optional<VaultTestResult> TestLaraVaultTolerance(ItemInfo* item, CollisionInfo* coll, VaultTestSetup testSetup);
std::optional<VaultTestResult> TestLaraVault2Steps(ItemInfo* item, CollisionInfo* coll);
std::optional<VaultTestResult> TestLaraVault3Steps(ItemInfo* item, CollisionInfo* coll);
std::optional<VaultTestResult> TestLaraVault1StepToCrouch(ItemInfo* item, CollisionInfo* coll);
std::optional<VaultTestResult> TestLaraVault2StepsToCrouch(ItemInfo* item, CollisionInfo* coll);
std::optional<VaultTestResult> TestLaraVault3StepsToCrouch(ItemInfo* item, CollisionInfo* coll);
std::optional<VaultTestResult> TestLaraLedgeAutoJump(ItemInfo* item, CollisionInfo* coll);
std::optional<VaultTestResult> TestLaraLadderAutoJump(ItemInfo* item, CollisionInfo* coll);
std::optional<VaultTestResult> TestLaraLadderMount(ItemInfo* item, CollisionInfo* coll);
std::optional<VaultTestResult> TestLaraAutoMonkeySwingJump(ItemInfo* item, CollisionInfo* coll);
std::optional<VaultTestResult> TestLaraVault(ItemInfo* item, CollisionInfo* coll);
bool TestAndDoLaraLadderClimb(ItemInfo* item, CollisionInfo* coll);

CrawlVaultTestResult TestLaraCrawlVaultTolerance(ItemInfo* item, CollisionInfo* coll, CrawlVaultTestSetup testSetup);
CrawlVaultTestResult TestLaraCrawlUpStep(ItemInfo* item, CollisionInfo* coll);
CrawlVaultTestResult TestLaraCrawlDownStep(ItemInfo* item, CollisionInfo* coll);
CrawlVaultTestResult TestLaraCrawlExitDownStep(ItemInfo* item, CollisionInfo* coll);
CrawlVaultTestResult TestLaraCrawlExitJump(ItemInfo* item, CollisionInfo* coll);
CrawlVaultTestResult TestLaraCrawlVault(ItemInfo* item, CollisionInfo* coll);
bool TestLaraCrawlToHang(ItemInfo* item, CollisionInfo* coll);

bool TestLaraPoleCollision(ItemInfo* item, CollisionInfo* coll, bool goingUp, float offset = 0.0f);
bool TestLaraPoleUp(ItemInfo* item, CollisionInfo* coll);
bool TestLaraPoleDown(ItemInfo* item, CollisionInfo* coll);
