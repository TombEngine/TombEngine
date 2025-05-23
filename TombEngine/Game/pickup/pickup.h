#pragma once

enum GAME_OBJECT_ID : short;
class GameBoundingBox;
struct CollisionInfo;
struct ItemInfo;
class Vector3i;

extern int NumRPickups;
extern short RPickups[16];
extern Vector3i OldPickupPos;

void InitializePickup(short itemNumber);
bool SetInventoryCount(GAME_OBJECT_ID objectID, int count);
void PickedUpObject(GAME_OBJECT_ID objectID, std::optional<int> count = std::nullopt);
void PickedUpObject(ItemInfo& item);
void RemoveObjectFromInventory(GAME_OBJECT_ID objectID, std::optional<int> count = std::nullopt);
int GetInventoryCount(GAME_OBJECT_ID objectID);
void CollectCarriedItems(ItemInfo* item);
void PickupCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
void RegeneratePickups();
void DropPickups(ItemInfo* item);
void CollectMultiplePickups(int itemNumber);
const GameBoundingBox* FindPlinth(ItemInfo* item);

void PickupControl(short itemNumber);

void InitializeSearchObject(short itemNumber);
void SearchObjectCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
void SearchObjectControl(short itemNumber);
void DoPickup(ItemInfo* laraItem);
bool UseSpecialItem(ItemInfo* laraItem);
