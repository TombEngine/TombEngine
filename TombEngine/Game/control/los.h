#pragma once
#include "Game/room.h"
#include "Objects/objectslist.h"
#include "Math/Math.h"

constexpr auto NO_LOS_ITEM = INT_MAX;

bool LOS(const GameVector* origin, GameVector* target);
bool GetTargetOnLOS(GameVector* origin, GameVector* target, bool drawTarget, bool isFiring);
int	 ObjectOnLOS2(GameVector* origin, GameVector* target, Vector3i* vec, StaticMesh** mesh, GAME_OBJECT_ID priorityObjectID = GAME_OBJECT_ID::ID_NO_OBJECT);
bool LOSAndReturnTarget(GameVector* origin, GameVector* target, int push);

std::optional<Vector3> GetStaticObjectLos(const Vector3& origin, int roomNumber, const Vector3& dir, float dist, bool onlySolid);
std::pair<GameVector, GameVector> GetRayFrom2DPosition(const Vector2& screenPos);
