#pragma once

struct ItemInfo;
struct CollisionInfo;

// -----------------------------
// WATER SURFACE TREAD
// Control & Collision Functions
// -----------------------------

void lara_as_surface_dive(ItemInfo* item, CollisionInfo* coll);
void lara_col_surface_dive(ItemInfo* item, CollisionInfo* coll);
void lara_as_surface_idle(ItemInfo* item, CollisionInfo* coll);
void lara_col_surface_idle(ItemInfo* item, CollisionInfo* coll);
void lara_as_surface_swim_forward(ItemInfo* item, CollisionInfo* coll);
void lara_col_surface_swim_forward(ItemInfo* item, CollisionInfo* coll);
void lara_as_surface_swim_left(ItemInfo* item, CollisionInfo* coll);
void lara_col_surface_swim_left(ItemInfo* item, CollisionInfo* coll);
void lara_as_surface_swim_right(ItemInfo* item, CollisionInfo* coll);
void lara_col_surface_swim_right(ItemInfo* item, CollisionInfo* coll);
void lara_as_surface_swim_back(ItemInfo* item, CollisionInfo* coll);
void lara_col_surface_swim_back(ItemInfo* item, CollisionInfo* coll);
void lara_as_surface_climb_out(ItemInfo* item, CollisionInfo* coll);
