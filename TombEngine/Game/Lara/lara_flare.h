#pragma once

enum GAME_OBJECT_ID : short;
class Vector3i;
struct CollisionInfo;
struct ItemInfo;

void FlareControl(short itemNumber);
void ReadyFlare(ItemInfo& laraItem);

void UndrawFlareMeshes(ItemInfo& laraItem);
void DrawFlareMeshes(ItemInfo& laraItem);

void UndrawFlare(ItemInfo& laraItem);
void DrawFlare(ItemInfo& laraItem);

void SetFlareArm(ItemInfo& laraItem, int armFrame);
void CreateFlare(ItemInfo& laraItem, GAME_OBJECT_ID objectID, bool isThrown);

void DoFlareInHand(ItemInfo& laraItem, int flareLife);
bool DoFlareLight(ItemInfo& item, const Vector3i& pos, int flareLife);
