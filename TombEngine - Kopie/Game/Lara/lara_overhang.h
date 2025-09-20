#pragma once
#include "Math/Math.h"

struct ItemInfo;
struct CollisionInfo;

void lara_col_slopeclimb(ItemInfo* item, CollisionInfo* coll);
void lara_as_slopeclimb(ItemInfo* item, CollisionInfo* coll);
void lara_as_slopefall(ItemInfo* item, CollisionInfo* coll);
void lara_col_slopehang(ItemInfo* item, CollisionInfo* coll);
void lara_as_slopehang(ItemInfo* item, CollisionInfo* coll);
void lara_col_slopeshimmy(ItemInfo* item, CollisionInfo* coll);
void lara_as_slopeshimmy(ItemInfo* item, CollisionInfo* coll);
void lara_as_slopeclimbup(ItemInfo* item, CollisionInfo* coll);
void lara_as_slopeclimbdown(ItemInfo* item, CollisionInfo* coll);
void lara_as_sclimbstart(ItemInfo* item, CollisionInfo* coll);
void lara_as_sclimbstop(ItemInfo* item, CollisionInfo* coll);
void lara_as_sclimbend(ItemInfo* item, CollisionInfo* coll);

void SlopeHangExtra(ItemInfo* item, CollisionInfo* coll);
void SlopeReachExtra(ItemInfo* item, CollisionInfo* coll);
void SlopeClimbExtra(ItemInfo* item, CollisionInfo* coll);
void SlopeClimbDownExtra(ItemInfo* item, CollisionInfo* coll);
void SlopeMonkeyExtra(ItemInfo* item, CollisionInfo* coll);
bool LadderMonkeyExtra(ItemInfo* item, CollisionInfo* coll);
