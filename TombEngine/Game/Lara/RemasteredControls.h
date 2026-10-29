#pragma once

enum class LaraWeaponType;
struct CollisionInfo;
struct ItemInfo;
struct LaraInfo;

constexpr auto PLAYER_REMASTERED_SPRINT_LEAN_CLAMP = ANGLE(16.1f);

bool  IsRemasteredGrabHeld();
bool  IsRemasteredVehicleFireHeld();
bool  DoesPlayerRemasteredSolverOwnHeading(const ItemInfo& item);
bool  DoesPlayerRemasteredCameraAssistApply(const ItemInfo& item);
short GetPlayerHeadingAngleY(const ItemInfo& item);
bool  HandlePlayerRemasteredDiagonalStep(const ItemInfo& item, CollisionInfo& coll);
void  ApplyRemasteredInput(ItemInfo& item);
void  HandlePlayerRemasteredHeading(ItemInfo& item);
bool  DoesRemasteredSolverOwnWaterHeading(const ItemInfo& item);
void  HandleRemasteredWaterHeading(ItemInfo& item);
bool  TestRemasteredSafetyGrab(ItemInfo& item, CollisionInfo& coll);
bool  HandleRemasteredTightropeIdle(ItemInfo& item);

bool  GetRemasteredUnscaledTurnAxisCoeff(const ItemInfo& item, float& axisCoeff);
void  ClampRemasteredArmedTurnRate(ItemInfo& item);
bool  HandleRemasteredCrawlSafetyGrabHang(ItemInfo& item, CollisionInfo& coll);
bool  HandleRemasteredCrouchStageUp(ItemInfo& item);
bool  HandleRemasteredCrawlSafetyGrabTurn(ItemInfo& item, CollisionInfo& coll);

bool  NeedsSecondWeaponPass(const ItemInfo& laraItem, const LaraInfo& player);
bool  IsRemasteredDrawHeld(const LaraInfo& player);
void  UpdateRemasteredDrawToggle(LaraInfo& player);
void  SetRemasteredDrawToggle(ItemInfo& laraItem);
void  SetRemasteredHolsters(LaraInfo& player, LaraWeaponType weaponType);
