#pragma once

struct ItemInfo;
struct CollisionInfo;

// -----------------------------------
// MISCELLANEOUS INTERACTABLE OBJECT
// Control & Collision Functions
// -----------------------------------

// ------
// PICKUP
// ------

void lara_as_pickup(ItemInfo* item, CollisionInfo* coll);
void lara_as_pickup_flare(ItemInfo* item, CollisionInfo* coll);
void lara_col_pickup(ItemInfo* item, CollisionInfo* coll);

// ------
// SWITCH
// ------

void lara_as_switch_on(ItemInfo* item, CollisionInfo* coll);
void lara_as_switch_off(ItemInfo* item, CollisionInfo* coll);
void lara_col_turn_switch(ItemInfo* item, CollisionInfo* coll);

// ----------
// RECEPTACLE
// ----------

void lara_as_use_key(ItemInfo* item, CollisionInfo* coll);
void lara_as_use_puzzle(ItemInfo* item, CollisionInfo* coll);

// --------
// PUSHABLE
// --------

void lara_as_pushable_push(ItemInfo* item, CollisionInfo* coll);
void lara_as_pushable_pull(ItemInfo* item, CollisionInfo* coll);
void lara_as_pushable_grab(ItemInfo* item, CollisionInfo* coll);
void lara_as_pushable_edge_slip(ItemInfo* item, CollisionInfo* coll);

// ------
// PULLEY
// ------

void lara_as_pulley(ItemInfo* item, CollisionInfo* coll);

// --------------
// HORIZONTAL BAR
// --------------

void lara_as_horizontal_bar_swing(ItemInfo* item, CollisionInfo* coll);
void lara_as_horizontal_bar_leap(ItemInfo* item, CollisionInfo* coll);

// ---------
// TIGHTROPE
// ---------

void lara_as_tightrope_dismount(ItemInfo* item, CollisionInfo* coll);
void lara_as_tightrope_idle(ItemInfo* item, CollisionInfo* coll);
void lara_as_tightrope_walk(ItemInfo* item, CollisionInfo* coll);
void lara_as_tightrope_fall(ItemInfo* item, CollisionInfo* coll);

// ----
// ROPE
// ----

void lara_as_rope_turn_clockwise(ItemInfo* item, CollisionInfo* coll);
void lara_as_rope_turn_counter_clockwise(ItemInfo* item, CollisionInfo* coll);
void lara_as_rope_idle(ItemInfo* item, CollisionInfo* coll);
void lara_col_rope_idle(ItemInfo* item, CollisionInfo* coll);
void lara_col_rope_swing(ItemInfo* item, CollisionInfo* coll);
void lara_as_rope_up(ItemInfo* item, CollisionInfo* coll);
void lara_as_rope_down(ItemInfo* item, CollisionInfo* coll);

// -------------
// VERTICAL POLE
// -------------

void lara_as_pole_idle(ItemInfo* item, CollisionInfo* coll);
void lara_col_pole_idle(ItemInfo* item, CollisionInfo* coll);
void lara_as_pole_up(ItemInfo* item, CollisionInfo* coll);
void lara_col_pole_up(ItemInfo* item, CollisionInfo* coll);
void lara_as_pole_down(ItemInfo* item, CollisionInfo* coll);
void lara_col_pole_down(ItemInfo* item, CollisionInfo* coll);
void lara_as_pole_turn_clockwise(ItemInfo* item, CollisionInfo* coll);
void lara_col_pole_turn_clockwise(ItemInfo* item, CollisionInfo* coll);
void lara_as_pole_turn_counter_clockwise(ItemInfo* item, CollisionInfo* coll);
void lara_col_pole_turn_counter_clockwise(ItemInfo* item, CollisionInfo* coll);

// --------
// ZIP-LINE
// --------

void lara_as_zip_line(ItemInfo* item, CollisionInfo* coll);
