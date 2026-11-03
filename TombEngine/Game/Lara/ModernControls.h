#pragma once

enum class LaraWeaponType;
enum class ScriptControlMode;
class EulerAngles;
struct CollisionInfo;
struct ItemInfo;
struct LaraInfo;

namespace TEN::Controls::Modern
{
	constexpr auto SPRINT_LEAN_CLAMP = ANGLE(16.1f);

	bool  IsGrabHeld();
	bool  IsVehicleFireHeld();
	bool  IsVehicleDiveHeld(const LaraInfo& player);
	bool  IsLookHeld(const LaraInfo& player);
	bool  IsLookAroundHeld(const LaraInfo& player);
	void  ResetLookToggle(LaraInfo& player);
	void  ApplyLookAxisCoeff(const ItemInfo& item, Vector2& axisCoeff);
	int   GetOpticsWheelZoom();
	int   GetWeaponWheelScroll(const LaraInfo& player);
	bool  DoesSolverOwnHeading(const ItemInfo& item);
	bool  DoesCameraAssistApply(const ItemInfo& item);
	short GetPlayerHeadingAngleY(const ItemInfo& item);
	Vector2 GetHeldMoveAxis();
	bool  HandleDiagonalStep(const ItemInfo& item, CollisionInfo& coll);
	void  ApplyInput(ItemInfo& item);
	void  HandleHeading(ItemInfo& item);
	bool  DoesSolverOwnWaterHeading(const ItemInfo& item);
	void  HandleWaterHeading(ItemInfo& item);
	void  HandleUnderwaterVehicleHeading(const ItemInfo& vehicleItem, LaraInfo& player, EulerAngles& turnRate, const EulerAngles& turnRateMax, bool isAtSurface);
	void  InitializeControlMode(bool clearScriptMode);
	void  SetScriptControlMode(ScriptControlMode controlMode);
	int   GetScriptControlModeID();
	void  SetScriptControlModeID(int controlModeID);
	bool  TestSafetyGrab(ItemInfo& item, CollisionInfo& coll);
	bool  HandleTightropeIdle(ItemInfo& item);

	bool  GetUnscaledTurnAxisCoeff(const ItemInfo& item, float& axisCoeff);
	void  ClampArmedTurnRate(ItemInfo& item);
	bool  HandleCrawlSafetyGrabHang(ItemInfo& item, CollisionInfo& coll);
	bool  HandleCrouchStageUp(ItemInfo& item);
	bool  HandleCrawlSafetyGrabTurn(ItemInfo& item, CollisionInfo& coll);

	bool  NeedsSecondWeaponPass(const ItemInfo& laraItem, const LaraInfo& player);
	bool  IsDrawHeld(const LaraInfo& player);
	bool  IsHolsterRequested(const LaraInfo& player);
	void  UpdateDrawToggle(LaraInfo& player);
	void  SetDrawToggle(ItemInfo& laraItem);
	void  SetHolsters(LaraInfo& player, LaraWeaponType weaponType);
}
