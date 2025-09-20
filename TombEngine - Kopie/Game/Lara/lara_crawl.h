#pragma once
#include "Game/Lara/lara_struct.h"

struct ItemInfo;
struct CollisionInfo;

// -----------------------------
// CROUCH & CRAWL
// Control & Collision Functions
// -----------------------------

// -------
// CROUCH:
// -------

void lara_as_crouch_idle(ItemInfo* item, CollisionInfo* coll);
void lara_col_crouch_idle(ItemInfo* item, CollisionInfo* coll);
void lara_as_crouch_roll(ItemInfo* item, CollisionInfo* coll);
void lara_col_crouch_roll(ItemInfo* item, CollisionInfo* coll);
void lara_as_crouch_turn_left(ItemInfo* item, CollisionInfo* coll);
void lara_col_crouch_turn_left(ItemInfo* item, CollisionInfo* coll);
void lara_as_crouch_turn_right(ItemInfo* item, CollisionInfo* coll);
void lara_col_crouch_turn_right(ItemInfo* item, CollisionInfo* coll);
void lara_as_crouch_turn_180(ItemInfo* item, CollisionInfo* coll);
void lara_col_crouch_turn_180(ItemInfo* item, CollisionInfo* coll);

// ------
// CRAWL:
// ------

void lara_as_crawl_idle(ItemInfo* item, CollisionInfo* coll);
void lara_col_crawl_idle(ItemInfo* item, CollisionInfo* coll);
void lara_as_crawl_forward(ItemInfo* item, CollisionInfo* coll);
void lara_col_crawl_forward(ItemInfo* item, CollisionInfo* coll);
void lara_as_crawl_back(ItemInfo* item, CollisionInfo* coll);
void lara_col_crawl_back(ItemInfo* item, CollisionInfo* coll);
void lara_as_crawl_turn_left(ItemInfo* item, CollisionInfo* coll);
void lara_col_crawl_turn_left(ItemInfo* item, CollisionInfo* coll);
void lara_as_crawl_turn_right(ItemInfo* item, CollisionInfo* coll);
void lara_col_crawl_turn_right(ItemInfo* item, CollisionInfo* coll);
void lara_as_crawl_turn_180(ItemInfo* item, CollisionInfo* coll);
void lara_col_crawl_turn_180(ItemInfo* item, CollisionInfo* coll);
void lara_col_crawl_to_hang(ItemInfo* item, CollisionInfo* coll);
