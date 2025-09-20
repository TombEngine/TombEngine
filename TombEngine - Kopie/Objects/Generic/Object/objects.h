#pragma once

struct ItemInfo;
struct CollisionInfo;

void SmashObject(short itemNumber);
void SmashObjectControl(short itemNumber);
void ControlAnimatingSlots(short itemNumber);
void ControlTriggerTriggerer(short itemNumber);
void TightropeCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
void HorizontalBarCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
void CutsceneRopeControl(short itemNumber);
void HybridCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
void InitializeSmashObject(short itemNumber);
void InitializeTightrope(short itemNumber);
void HighObject2Control(short itemNumber);
void InitializeAnimating(short itemNumber);
void AnimatingControl(short itemNumber);
