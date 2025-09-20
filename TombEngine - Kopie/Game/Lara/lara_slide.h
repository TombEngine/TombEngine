#pragma once

struct ItemInfo;
struct CollisionInfo;

// -----------------------------
// SLIDE
// Control & Collision Functions
// -----------------------------

void lara_as_slide_forward(ItemInfo* item, CollisionInfo* coll);
void lara_col_slide_forward(ItemInfo* item, CollisionInfo* coll);
void lara_as_slide_back(ItemInfo* item, CollisionInfo* coll);
void lara_col_slide_back(ItemInfo* item, CollisionInfo* coll);
