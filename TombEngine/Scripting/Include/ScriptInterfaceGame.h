#pragma once
#include "framework.h"
#include "Game/control/event.h"
#include "Game/room.h"
#include "Scripting/Internal/TEN/Logic/CallbackPoint.h"
#include "Specific/level.h"

typedef unsigned int D3DCOLOR;

using VarSaveType = std::variant<bool, double, std::string>;
using IndexTable = std::vector<std::pair<unsigned int, unsigned int>>;

struct FuncName
{
	std::string name;
};

enum class SavedVarType
{
	Bool,
	String,
	Number,
	IndexTable,
	Vec2,
	Vec3,
	Rotation,
	Time,
	Color,
	FuncName,

	NumTypes
};

using SavedVar = std::variant<
	bool,
	std::string,
	double,
	IndexTable,
	Vector2,  // Vec2
	Vector3,  // Vec3
	Vector3,  // Rotation
	int,	  // Time
	D3DCOLOR, // Color
	FuncName>;

// Make sure SavedVarType and SavedVar have same number of types.
static_assert((int)SavedVarType::NumTypes == std::variant_size_v<SavedVar>);

using CallbackStringLists = std::array<std::vector<std::string>, (int)TEN::Scripting::CallbackPoint::Count>;

class ScriptInterfaceGame
{
public:
	virtual ~ScriptInterfaceGame() = default;
	
	virtual void InitCallbacks() = 0;

	virtual void OnStart() = 0;
	virtual void OnLoad() = 0;
	virtual void OnLoop(float deltaTime, bool postLoop) = 0;
	virtual void OnSave() = 0;
	virtual void OnEnd(GameStatus reason) = 0;
	virtual void OnUseItem(short itemNumber, GAME_OBJECT_ID objectNumber) = 0;
	virtual void OnPickup(short itemNumber, bool postLoop) = 0;
	virtual void OnVehicleEnter(short itemNumber, bool postLoop) = 0;
	virtual void OnVehicleLeave(short itemNumber, bool postLoop) = 0;
	virtual void OnFreeze() = 0;

	virtual void AddConsoleInput(const std::string& input) = 0;
	virtual void ShortenTENCalls() = 0;
	virtual void FreeLevelScripts() = 0;
	virtual void ResetScripts(bool clearGameVars) = 0;
	virtual void ExecuteScriptFile(const std::string& luaFileName) = 0;
	virtual void ExecuteString(const std::string& command) = 0;
	virtual void ExecuteFunction(const std::string& luaFuncName, TEN::Control::Volumes::Activator, const std::string& arguments) = 0;
	virtual void ExecuteFunction(const std::string& luaFuncName, short idOne, short idTwo = 0) = 0;

	virtual unsigned int GetFunctionCallCount() = 0;

	virtual void GetVariables(std::vector<SavedVar>& vars) = 0;
	virtual void SetVariables(const std::vector<SavedVar>& vars, bool onlyLevelVars) = 0;
	virtual void GetGlobalVariables(std::vector<SavedVar>& vars) = 0;
	virtual void SetGlobalVariables(const std::vector<SavedVar>& vars) = 0;

	virtual void GetCallbackStrings(CallbackStringLists& callbackLists) const = 0;
	virtual void SetCallbackStrings(const CallbackStringLists& callbackLists) = 0;
};

extern ScriptInterfaceGame* g_GameScript;
