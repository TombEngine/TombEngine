#pragma once

#include "Scripting/Include/VarMapVal.h"
#include "Specific/level.h"

typedef DWORD D3DCOLOR;

class ScriptInterfaceObjectsHandler
{
public:
	virtual ~ScriptInterfaceObjectsHandler() = default;

	virtual int GetIndexByName(const std::string& name) const = 0;
	virtual bool AddName(const std::string& key, VarMapVal val) = 0;
	virtual bool NotifyKilled(ItemInfo* item) = 0;
	virtual void FreeEntities() = 0;
	virtual void AssignPlayer() = 0;

	virtual bool TryAddColliding(int id) = 0;
	virtual bool TryRemoveColliding(int id, bool force = false) = 0;
	virtual void TestCollidingObjects() = 0;
};

extern ScriptInterfaceObjectsHandler* g_GameScriptEntities;
