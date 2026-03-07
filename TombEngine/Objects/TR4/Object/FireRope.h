#pragma once

class Vector3i;
struct CollisionInfo;
struct ItemInfo;

void TriggerRopeFlame(Vector3i pos);
void InitializeFireRope(short itemNumber);
void FireRopeControl(short itemNumber);
void FireRopeCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);