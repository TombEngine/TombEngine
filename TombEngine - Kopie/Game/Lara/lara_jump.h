#pragma once

struct ItemInfo;
struct CollisionInfo;

// -----------------------------
// JUMP
// Control & Collision Functions
// -----------------------------

void lara_as_jump_forward(ItemInfo* item, CollisionInfo* coll);
void lara_col_jump_forward(ItemInfo* item, CollisionInfo* coll);
void lara_as_freefall(ItemInfo* item, CollisionInfo* coll);
void lara_col_freefall(ItemInfo* item, CollisionInfo* coll);
void lara_as_reach(ItemInfo* item, CollisionInfo* coll);
void lara_col_reach(ItemInfo* item, CollisionInfo* coll);
void lara_col_land(ItemInfo* item, CollisionInfo* coll);
void lara_as_jump_prepare(ItemInfo* item, CollisionInfo* coll);
void lara_col_jump_prepare(ItemInfo* item, CollisionInfo* coll);
void lara_as_jump_back(ItemInfo* item, CollisionInfo* coll);
void lara_col_jump_back(ItemInfo* item, CollisionInfo* coll);
void lara_as_jump_right(ItemInfo* item, CollisionInfo* coll);
void lara_col_jump_right(ItemInfo* item, CollisionInfo* coll);
void lara_as_jump_left(ItemInfo* item, CollisionInfo* coll);
void lara_col_jump_left(ItemInfo* item, CollisionInfo* coll);
void lara_as_jump_up(ItemInfo* item, CollisionInfo* coll);
void lara_col_jump_up(ItemInfo* item, CollisionInfo* coll);
void lara_as_fall_back(ItemInfo* item, CollisionInfo* coll);
void lara_col_fall_back(ItemInfo* item, CollisionInfo* coll);
void lara_as_swan_dive(ItemInfo* item, CollisionInfo* coll);
void lara_col_swan_dive(ItemInfo* item, CollisionInfo* coll);
void lara_as_freefall_dive(ItemInfo* item, CollisionInfo* coll);
void lara_col_freefall_dive(ItemInfo* item, CollisionInfo* coll);
