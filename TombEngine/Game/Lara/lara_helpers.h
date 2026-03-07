#pragma once
#include "Game/collision/collide_room.h"

namespace TEN::Player{ struct ClimbContextData; };
namespace TEN::Player{ struct WaterTreadStepOutContextData; };
namespace TEN::Player{ using JumpCatchClimbContextData = std::variant<struct ClimbContextData, struct MonkeySwingJumpCatchClimbContextData>; };
enum class JumpDirection;
enum class WaterStatus;
struct ItemInfo;
struct CollisionInfo;
struct LaraInfo;

using namespace TEN::Player;

struct PlayerWaterData
{
	bool IsWater = false;
	bool IsSwamp = false;
	bool IsCold	 = false;

	int WaterDepth		= 0;
	int WaterHeight		= 0;
	int HeightFromWater = 0;
};

// -----------------------------
// HELPER FUNCTIONS
// For State Control & Collision
// -----------------------------

// Utilities
void HandleLaraMovementParameters(ItemInfo* item, CollisionInfo* coll);
void HandlePlayerStatusEffects(ItemInfo& item, WaterStatus waterStatus, PlayerWaterData& water);
void HandlePlayerAttractorParent(ItemInfo& item);
void HandlePlayerQuickActions(ItemInfo& item);
bool CanPlayerLookAround(const ItemInfo& item); // TODO: Move to context file. -- Sezz 2023.08.22
void HandlePlayerLookAround(ItemInfo& item, bool invertXAxis = true);
bool HandleLaraVehicle(ItemInfo* item, CollisionInfo* coll);
void HandlePlayerLean(ItemInfo* item, CollisionInfo* coll, short baseRate, short maxAngle);
void HandlePlayerCrawlFlex(ItemInfo& item);
void HandlePlayerFlyCheat(ItemInfo& item);
void HandlePlayerExtraAnim(ItemInfo& item);
void HandlePlayerWetnessDrips(ItemInfo& item);
void HandlePlayerDiveBubbles(ItemInfo& item);
void HandlePlayerAirBubbles(ItemInfo* item);

void EasePlayerElevation(ItemInfo* item, int relHeight);
void HandlePlayerElevationChange(ItemInfo* item, CollisionInfo* coll);
void DoLaraMonkeyStep(ItemInfo* item, CollisionInfo* coll);
void DoLaraTightropeBalance(ItemInfo* item);
void DoLaraTightropeLean(ItemInfo* item);
void DoLaraTightropeBalanceRegen(ItemInfo* item);
void DoLaraFallDamage(ItemInfo* item);

// Getters
LaraInfo& GetLaraInfo(ItemInfo& item);
const LaraInfo& GetLaraInfo(const ItemInfo& item);
LaraInfo*& GetLaraInfo(ItemInfo* item);
JumpDirection GetPlayerJumpDirection(const ItemInfo& item, const CollisionInfo& coll);

PlayerWaterData GetPlayerWaterData(ItemInfo& item);
short GetPlayerSlideHeadingAngle(ItemInfo* item, CollisionInfo* coll);

// Modulators
short ModulateLaraTurnRate(short turnRate, short accelRate, short minTurnRate, short maxTurnRate, float axisCoeff, bool invert);
void ModulateLaraTurnRateX(ItemInfo* item, short accelRate, short minTurnRate, short maxTurnRate, bool invert = true);
void ModulateLaraTurnRateY(ItemInfo* item, short accelRate, short minTurnRate, short maxTurnRate, bool invert = false);
void ResetPlayerTurnRateX(ItemInfo& item, short decelRate = SHRT_MAX);
void ResetPlayerTurnRateY(ItemInfo& item, short decelRate = SHRT_MAX);
void ModulateLaraSwimTurnRates(ItemInfo* item, CollisionInfo* coll);
void ModulateLaraSubsuitSwimTurnRates(ItemInfo* item);
void UpdateLaraSubsuitAngles(ItemInfo* item);
void ModulateLaraSlideVelocity(ItemInfo* item, CollisionInfo* coll);
void AlignLaraToSurface(ItemInfo* item, float alpha = 0.15f);

// Setters
void SetLaraJumpDirection(ItemInfo* item, CollisionInfo* coll);
void SetLaraRunJumpQueue(ItemInfo* item, CollisionInfo* coll);
void SetPlayerClimb(ItemInfo& item, const ClimbContextData& climbContext);
void SetPlayerTreadWaterStepOut(ItemInfo& item, const WaterTreadStepOutContextData& stepOutContext);
void SetLaraLand(ItemInfo* item, CollisionInfo* coll);
void SetLaraFallAnimation(ItemInfo* item);
void SetLaraFallBackAnimation(ItemInfo* item);
void SetLaraMonkeyFallAnimation(ItemInfo* item);
void SetLaraMonkeyRelease(ItemInfo* item);
void SetLaraSlideAnimation(ItemInfo* item, CollisionInfo* coll);
void SetPlayerEdgeHangRelease(ItemInfo& item);
void SetLaraSwimDiveAnimation(ItemInfo* item);
void SetLaraVehicle(ItemInfo* item, ItemInfo* vehicle = nullptr);

void ResetPlayerLean(ItemInfo* item, float alpha = 1.0f, bool resetRoll = true, bool resetPitch = true);
void ResetPlayerFlex(ItemInfo* item, float alpha = 1.0f);
void ResetPlayerLookAround(ItemInfo& item, float alpha = 0.1f);

void RumbleLaraHealthCondition(ItemInfo* item);
