#pragma once

#include "Game/room.h"
#include "Objects/objectslist.h"
#include "Math/Math.h"

struct StaticMesh;

constexpr auto NO_LOS_ITEM = INT_MAX;

// Legacy LOS functions

bool LOS(const GameVector* origin, GameVector* target);
bool LOSAndReturnTarget(GameVector* origin, GameVector* target, int push);
bool GetTargetOnLOS(GameVector* origin, GameVector* target);
int	 ObjectOnLOS2(GameVector* origin, GameVector* target, Vector3i* vec, StaticMesh** mesh, GAME_OBJECT_ID priorityObjectID = GAME_OBJECT_ID::ID_NO_OBJECT, int excludeItemIndex = NO_VALUE);

// Returns number of hits (0 = none).
// outItems: positive = item index, negative = static (-1 - Slot).
// multiHit=false: nearest hit only. multiHit=true: all hits sorted by distance (up to maxResults).
// excludeSelf: item index to skip, -1 = none. No object-type filter — hits any collidable object or static.
// Prerequisite: LOS() must have been called first (fills LosRoomNumbers).
// Designed also for future multi-target effects (e.g. spawnable laser beam hitting all enemies in line).
int ObjectOnLOS3(GameVector* origin, GameVector* target, Vector3i* hitPos, int* outItems, int maxResults, bool multiHit, int excludeSelf);
