#pragma once

struct ItemInfo;
struct CollisionInfo;

void InitializeGameSticks(short itemNumber);
void GameSticksControl(short itemNumber);
void _0x0040FAE0(ItemInfo* item);
void SenetPieceExplosionEffect(ItemInfo* item, int color, int speed);
void TriggerItemInRoom(short roomNumber, int object);
bool CheckSenetWinner(short number);
void MakeMove(int piece, int displacement);
void GameSticksCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
void ControlGodHead(short itemNumber);
void InitializeGamePiece(short itemNumber);
void SenetControl(short itemNumber);
