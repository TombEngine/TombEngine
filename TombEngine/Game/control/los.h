#pragma once

#include "Game/room.h"
#include "Objects/objectslist.h"
#include "Math/Math.h"

struct MESH_INFO;

constexpr auto NO_LOS_ITEM = INT_MAX;

// Legacy LOS functions

bool LOS(const GameVector* origin, GameVector* target);
bool LOSAndReturnTarget(GameVector* origin, GameVector* target, int push);
bool GetTargetOnLOS(GameVector* origin, GameVector* target, bool drawTarget, bool isFiring);
int	 ObjectOnLOS2(GameVector* origin, GameVector* target, Vector3i* vec, MESH_INFO** mesh, GAME_OBJECT_ID priorityObjectID = GAME_OBJECT_ID::ID_NO_OBJECT);
