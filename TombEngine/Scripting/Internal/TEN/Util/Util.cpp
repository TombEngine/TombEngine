#include "framework.h"
#include "Scripting/Internal/TEN/Util/Util.h"

#include "Game/collision/collide_room.h"
#include "Game/collision/Los.h"
#include "Game/control/los.h"
#include "Game/Lara/lara.h"
#include "Game/room.h"
#include "Renderer/Renderer.h"
#include "Scripting/Internal/LuaHandler.h"
#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/ScriptAssert.h"
#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/TEN/Objects/Moveable/MoveableObject.h"
#include "Scripting/Internal/TEN/Objects/Static/StaticObject.h"
#include "Scripting/Internal/TEN/Types/Vec2/Vec2.h"
#include "Scripting/Internal/TEN/Types/Vec3/Vec3.h"
#include "Scripting/Internal/TEN/Util/LevelLog.h"
#include "Specific/configuration.h"
#include "Specific/level.h"

using namespace TEN::Collision::Los;
using TEN::Renderer::g_Renderer;

namespace TEN::Scripting::Util
{
	/// Utility functions for various calculations.
	// @tentable Util
	// @pragma nostrip

	/// Determine if there is a clear line of sight between two positions.
	// NOTE: Limited to room geometry. Objects are ignored.
	// @function HasLineOfSight()
	// @tparam float roomID Room ID of the first position's room.
	// @tparam Vec3 posA First position.
	// @tparam Vec3 posB Second position.
	// @treturn bool __true__ if there is a line of sight, __false__ if not.
	// @usage
	// local flamePlinthPos = flamePlinth:GetPosition() + Vec3(0, flamePlinthHeight, 0);
	// print(Misc.HasLineOfSight(enemyHead:GetRoomNumber(), enemyHead:GetPosition(), flamePlinthPos))
	static bool HasLineOfSight(int roomID, const Vec3& posA, const Vec3& posB)
	{
		auto vector0 = posA.ToGameVector();
		auto vector1 = posB.ToGameVector();

		MESH_INFO* mesh = nullptr;
		auto vector = Vector3i::Zero;
		return (LOS(&vector0, &vector1) && ObjectOnLOS2(&vector0, &vector1, &vector, &mesh) == NO_LOS_ITEM);
	}

	/// Calculate the horizontal distance between two positions.
	// @function CalculateHorizontalDistance
	// @tparam Vec3 posA First position.
	// @tparam Vec3 posB Second position.
	// @treturn float Horizontal distance between the two positions.
	static float CalculateHorizontalDistance(const Vec3& posA, const Vec3& posB)
	{
		auto pos0 = Vector2(posA.x, posA.z);
		auto pos1 = Vector2(posB.x, posB.z);
		return round(Vector2::Distance(pos0, pos1));
	}

	/// Get the projected display space position of a 3D world position. Returns nil if the world position is behind the camera view.
	// @function GetDisplayPosition
	// @tparam Vec3 worldPos 3D world position.
	// @treturn Vec2 Projected display space position in percent.
	// @usage 
	// Example: Display a string at the player's position.
	// local string = DisplayString('Example', 0, 0, Color(255, 255, 255), false)
	// local displayPos = GetDisplayPosition(Lara:GetPosition())
	// string:SetPosition(PercentToScreen(displayPos.x, displayPos.y))
	static sol::optional<Vec2> GetDisplayPosition(const Vec3& worldPos)
	{
		auto displayPos = g_Renderer.Get2DPosition(worldPos.ToVector3());
		if (!displayPos.has_value())
			return sol::nullopt;

		return Vec2(
			(displayPos->x / DISPLAY_SPACE_RES.x) * 100,
			(displayPos->y / DISPLAY_SPACE_RES.y) * 100);
	}

	/// Translate a pair display position coordinates to pixel coordinates.
	//To be used with @{Strings.DisplayString:SetPosition} and @{Strings.DisplayString}.
	//@function PercentToScreen
	//@tparam float x X component of the display position.
	//@tparam float y Y component of the display position.
	//@treturn int x X coordinate in pixels.
	//@treturn int y Y coordinate in pixels.
	//@usage	
	//local halfwayX, halfwayY = PercentToScreen(50, 50)
	//local baddy
	//local spawnLocationNullmesh = GetMoveableByName("position_behind_left_pillar")
	//local str1 = DisplayString("You spawned an enemy!", halfwayX, halfwayY, Color(255, 100, 100), false, { DisplayStringOption.SHADOW, DisplayStringOption.CENTER })
	//
	//LevelFuncs.triggerOne = function(obj) 
	//	ShowString(str1, 4)
	//end
	static std::tuple<int, int> PercentToScreen(float x, float y)
	{
		float fWidth = g_Configuration.ScreenWidth;
		float fHeight = g_Configuration.ScreenHeight;
		int resX = (int)std::round(fWidth / 100.0f * x);
		int resY = (int)std::round(fHeight / 100.0f * y);

		return std::make_tuple(resX, resY);
	}

	/// Translate a pair of pixel coordinates to display position coordinates.
	//To be used with @{Strings.DisplayString:GetPosition}.
	//@function ScreenToPercent
	//@tparam int x X pixel coordinate to translate to display position.
	//@tparam int y Y pixel coordinate to translate to display position.
	//@treturn float x X component of display position.
	//@treturn float y Y component of display position.
	static std::tuple<float, float> ScreenToPercent(int x, int y)
	{
		float fWidth = g_Configuration.ScreenWidth;
		float fHeight = g_Configuration.ScreenHeight;
		float resX = x / fWidth * 100.0f;
		float resY = y / fHeight * 100.0f;
		return std::make_tuple(resX, resY);
	}

	/// Pick a moveable by the given display position.
	// @function PickMoveableByDisplayPosition
	// @tparam Vec2 Display space position in percent.
	// @treturn Objects.Moveable Picked moveable (nil if no moveable was found under the cursor).
	static sol::optional <std::unique_ptr<Moveable>> PickMoveable(const Vec2& screenPos)
	{
		auto realScreenPos = PercentToScreen(screenPos.x, screenPos.y);
		auto ray = GetRayFrom2DPosition(Vector2(int(std::get<0>(realScreenPos)), int(std::get<1>(realScreenPos))));

		auto vector = Vector3i::Zero;
		int itemNumber = ObjectOnLOS2(&ray.first, &ray.second, &vector, nullptr, GAME_OBJECT_ID::ID_LARA);

		if (itemNumber == NO_LOS_ITEM || itemNumber < 0)
			return sol::nullopt;

		return std::make_unique<Moveable>(itemNumber);
	}

	/// Pick a static mesh by the given display position.
	// @function PickStaticByDisplayPosition
	// @tparam Vec2 Display space position in percent.
	// @treturn Objects.Static Picked static mesh (nil if no static mesh was found under the cursor).
	static sol::optional <std::unique_ptr<Static>> PickStatic(const Vec2& screenPos)
	{
		auto realScreenPos = PercentToScreen(screenPos.x, screenPos.y);
		auto ray = GetRayFrom2DPosition(Vector2((int)std::get<0>(realScreenPos), (int)std::get<1>(realScreenPos)));

		MESH_INFO* mesh = nullptr;
		auto vector = Vector3i::Zero;
		int itemNumber = ObjectOnLOS2(&ray.first, &ray.second, &vector, &mesh, GAME_OBJECT_ID::ID_LARA);

		if (itemNumber == NO_LOS_ITEM || itemNumber >= 0)
			return sol::nullopt;

		return std::make_unique<Static>(*mesh);
	}

	/// Write messages within the Log file
	//@advancedDesc
	//For native Lua handling of errors, see the official Lua website:
	//
	//<a href="https://www.lua.org/pil/8.3.html">Error management</a>
	//
	//<a href="https://www.lua.org/manual/5.4/manual.html#pdf-debug.traceback">debug.traceback</a>
	//@function PrintLog
	//@tparam string message to be displayed within the Log
	//@tparam Util.LogLevel logLevel log level to be displayed
	//@tparam[opt] bool allowSpam true allows spamming of the message
	// 
	//@usage
	//PrintLog('test info log', LogLevel.INFO)
	//PrintLog('test warning log', LogLevel.WARNING)
	//PrintLog('test error log', LogLevel.ERROR)
	//-- spammed message
	//PrintLog('test spam log', LogLevel.INFO, true)
	// 
	static void PrintLog(const std::string& message, const LogLevel& level, TypeOrNil<bool> allowSpam)
	{
		TENLog(message, level, LogConfig::All, ValueOr<bool>(allowSpam, false));
	}

	static float CalculateDistance(const Vec3& posA, const Vec3& posB)
	{
		return posA.Distance(posB);
	}

	void Register(sol::state* state, sol::table& parent)
	{
		auto tableUtil = sol::table(state->lua_state(), sol::create);
		parent.set(ScriptReserved_Util, tableUtil);

		tableUtil.set_function(ScriptReserved_HasLineOfSight, &HasLineOfSight);
		tableUtil.set_function(ScriptReserved_CalculateHorizontalDistance, &CalculateHorizontalDistance);
		tableUtil.set_function(ScriptReserved_GetDisplayPosition, &GetDisplayPosition);
		tableUtil.set_function(ScriptReserved_PickMoveable, &PickMoveable);
		tableUtil.set_function(ScriptReserved_PickStatic, &PickStatic);
		tableUtil.set_function(ScriptReserved_PercentToScreen, &PercentToScreen);
		tableUtil.set_function(ScriptReserved_ScreenToPercent, &ScreenToPercent);
		tableUtil.set_function(ScriptReserved_PrintLog, &PrintLog);

		// COMPATIBILITY
		tableUtil.set_function("CalculateDistance", &CalculateDistance);

		auto handler = LuaHandler(state);
		handler.MakeReadOnlyTable(tableUtil, ScriptReserved_LogLevel, LOG_LEVEL_IDS);
	}
}
