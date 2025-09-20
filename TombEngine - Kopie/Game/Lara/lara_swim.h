#pragma once
#include "Game/collision/collide_room.h"

// -----------------------------
// UNDERWATER SWIM
// Control & Collision Functions
// -----------------------------

void lara_as_underwater_idle(ItemInfo* item, CollisionInfo* coll);
void lara_col_underwater_idle(ItemInfo* item, CollisionInfo* coll);
void lara_as_underwater_swim_forward(ItemInfo* item, CollisionInfo* coll);
void lara_col_underwater_swim_forward(ItemInfo* item, CollisionInfo* coll);
void lara_as_underwater_inertia(ItemInfo* item, CollisionInfo* coll);
void lara_col_underwater_inertia(ItemInfo* item, CollisionInfo* coll);
void lara_as_underwater_death(ItemInfo* item, CollisionInfo* coll);
void lara_col_underwater_death(ItemInfo* item, CollisionInfo* coll);
void lara_as_underwater_roll_180(ItemInfo* item, CollisionInfo* coll);
void lara_col_underwater_roll_180(ItemInfo* item, CollisionInfo* coll);

