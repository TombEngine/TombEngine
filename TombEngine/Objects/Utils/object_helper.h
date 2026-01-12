#pragma once
#include "Game/Setup.h"

#define InitFunction void(short itemNumber)
#define ControlFunction void(short itemNumber)

void AssignObjectMeshSwap(ObjectInfo& object, int requiredMeshSwap, const std::string& baseName, const std::string& requiredName);
bool AssignObjectAnimations(ObjectInfo& object, int requiredObjectID, const std::string& baseName = "NOT_SET", const std::string& requiredName = "NOT_SET");
bool CheckIfSlotExists(GAME_OBJECT_ID requiredObj, const std::string& baseName);

void InitSmashObject(ObjectInfo* object, int objectNumber);
void InitKeyHole(ObjectInfo* object, int objectNumber);
void InitPuzzleHole(ObjectInfo* object, int objectNumber);
void InitPuzzleDone(ObjectInfo* object, int objectNumber);
void InitAnimating(ObjectInfo* object, int objectNumber);
void InitPickup(ObjectInfo* object, int objectNumber);
void InitPickup(ObjectInfo* object, int objectNumber, std::function<ControlFunction> func);
void InitFlare(ObjectInfo* object, int objectNumber);
void InitProjectile(ObjectInfo* object, std::function<InitFunction> func, int objectNumber, bool noLoad = false);
void InitSearchObject(ObjectInfo* object, int objectNumber);
void InitPushableObject(ObjectInfo* object, int objectNumber);
