#pragma once

struct ItemInfo;
struct CollisionInfo;

// -----------------------------
// MONKEY SWING
// Control & Collision Functions
// -----------------------------

void lara_as_monkey_idle(ItemInfo* item, CollisionInfo* coll);
void lara_col_monkey_idle(ItemInfo* item, CollisionInfo* coll);
void lara_as_monkey_forward(ItemInfo* item, CollisionInfo* coll);
void lara_col_monkey_forward(ItemInfo* item, CollisionInfo* coll);
void lara_as_monkey_back(ItemInfo* item, CollisionInfo* coll);
void lara_col_monkey_back(ItemInfo* item, CollisionInfo* coll);
void lara_as_monkey_shimmy_left(ItemInfo* item, CollisionInfo* coll);
void lara_col_monkey_shimmy_left(ItemInfo* item, CollisionInfo* coll);
void lara_as_monkey_shimmy_right(ItemInfo* item, CollisionInfo* coll);
void lara_col_monkey_shimmy_right(ItemInfo* item, CollisionInfo* coll);
void lara_as_monkey_turn_180(ItemInfo* item, CollisionInfo* coll);
void lara_col_monkey_turn_180(ItemInfo* item, CollisionInfo* coll);
void lara_as_monkey_turn_left(ItemInfo* item, CollisionInfo* coll);
void lara_col_monkey_turn_left(ItemInfo* item, CollisionInfo* coll);
void lara_as_monkey_turn_right(ItemInfo* item, CollisionInfo* coll);
void lara_col_monkey_turn_right(ItemInfo* item, CollisionInfo* coll);
