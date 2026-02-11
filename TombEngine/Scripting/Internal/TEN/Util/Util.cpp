#include "framework.h"
#include "Scripting/Internal/TEN/Util/Util.h"

#include "Game/collision/collide_room.h"
#include "Game/collision/Los.h"
#include "Game/control/los.h"
#include "Game/Gui.h"
#include "Game/items.h"
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

#include "Scripting/Include/ScriptInterfaceGame.h"
#include "Scripting/Include/ScriptInterfaceLevel.h"
#include "Game/control/volume.h"

using namespace TEN::Collision::Los;
using TEN::Renderer::g_Renderer;

namespace TEN::Scripting::Util
{
	/// Utility functions for various calculations.
	// @tentable Util
	// @pragma nostrip

	/// Determine if there is a clear line of sight between two positions. Limited to room geometry. Objects are ignored.
	// @function HasLineOfSight
	// @tparam float roomID Room ID of the first position's room.
	// @tparam Vec3 posA First position.
	// @tparam Vec3 posB Second position.
	// @treturn bool true if there is a line of sight, false if not.
	// @usage
	// local flamePlinthPos = flamePlinth:GetPosition() + Vec3(0, flamePlinthHeight, 0);
	// local test = TEN.Util.HasLineOfSight(enemyHead:GetRoomNumber(), enemyHead:GetPosition(), flamePlinthPos);
	// print(tostring(test))
	static bool HasLineOfSight(int roomID, const Vec3& posA, const Vec3& posB)
	{
		auto vector0 = posA.ToGameVector();
		auto vector1 = posB.ToGameVector();

		StaticMesh* mesh = nullptr;
		auto vector = Vector3i::Zero;
		return (LOS(&vector0, &vector1) && ObjectOnLOS2(&vector0, &vector1, &vector, &mesh) == NO_LOS_ITEM);
	}

	/// Calculate the horizontal distance between two positions.
	// @function CalculateHorizontalDistance
	// @tparam Vec3 posA First position.
	// @tparam Vec3 posB Second position.
	// @treturn float Horizontal distance between the two positions.
	// @usage
	// local dist = TEN.Util.CalculateHorizontalDistance(Lara:GetPosition(), enemyHead:GetPosition())
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
	// -- Example: Display a string at the player's position.
	// local displayPos = TEN.Util.GetDisplayPosition(Lara:GetPosition())
	// local str = TEN.Strings.DisplayString('You are here!', displayPos)
	// TEN.Strings.ShowString(str, 4)
	static sol::optional<Vec2> GetDisplayPosition(const Vec3& worldPos)
	{
		auto displayPos = g_Renderer.Get2DPosition(worldPos.ToVector3());
		if (!displayPos.has_value())
			return sol::nullopt;

		return Vec2(
			(displayPos->x / DISPLAY_SPACE_RES.x) * 100,
			(displayPos->y / DISPLAY_SPACE_RES.y) * 100);
	}

	/// Translate a pair display position coordinates to pixel coordinates.<br>
	// To be used with `Strings.DisplayString` and `Strings.DisplayString:SetPosition`.
	// @function PercentToScreen
	// @tparam float x X component of the display position.
	// @tparam float y Y component of the display position.
	// @treturn int x X coordinate in pixels.
	// @treturn int y Y coordinate in pixels.
	// @usage
	// -- Example 1: Simple usage of PercentToScreen.
	// local screenX, screenY = TEN.Util.PercentToScreen(10, 10)
	// local pos = TEN.Vec2(screenX, screenY)
	// local str1 = TEN.Strings.DisplayString("Position at 10% X and 10% Y", pos)
	//
	// -- Example 2: Using PercentToScreen with DisplayString:SetPosition.
	// str1:SetPosition(TEN.Util.PercentToScreen(50, 50))
	static std::tuple<int, int> PercentToScreen(float x, float y)
	{
		float fWidth = g_Configuration.ScreenWidth;
		float fHeight = g_Configuration.ScreenHeight;
		int resX = (int)std::round(fWidth / 100.0f * x);
		int resY = (int)std::round(fHeight / 100.0f * y);

		return std::make_tuple(resX, resY);
	}

	/// Translate a Vec2 of display position coordinates to Vec2 pixel coordinates.<br>
	// To be used with `Strings.DisplayString` and `Strings.DisplayString:SetPosition`.
	// @function PercentToScreen
	// @tparam Vec2 percentPos Display position to translate to pixel coordinates.
	// @treturn Vec2 Pixel coordinates.
	// @usage
	// -- Example: Translate a display position to pixel coordinates.
	// local percentPos = TEN.Vec2(25, 75)
	// local screenPos = TEN.Util.PercentToScreen(percentPos)
	// local str1 = TEN.Strings.DisplayString("Position at 25% X and 75% Y", screenPos)
	//
	// -- Example 2: Using PercentToScreen with DisplayString:SetPosition.
	// str1:SetPosition(TEN.Util.PercentToScreen(TEN.Vec2(50, 50)))
	static Vec2 PercentToScreen(const Vec2& percentPos)
	{
		auto [screenX, screenY] = PercentToScreen(percentPos.x, percentPos.y);
		return Vec2(screenX, screenY);
	}

	/// Translate a pair of pixel coordinates to display position coordinates.
	// To be used with `Strings.DisplayString:GetPosition`.
	// @function ScreenToPercent
	// @tparam int x X pixel coordinate to translate to display position.
	// @tparam int y Y pixel coordinate to translate to display position.
	// @treturn float x X component of display position.
	// @treturn float y Y component of display position.
	// @usage
	// -- Example: Translate pixel coordinates to display position.
	// local percentX, percentY = TEN.Util.ScreenToPercent(800, 600)
	//
	// -- Example 2: Using ScreenToPercent with DisplayString:GetPosition.
	// local screenPos = str1:GetPosition()
	// local percentPos = TEN.Util.ScreenToPercent(screenPos.x, screenPos.y)
	static std::tuple<float, float> ScreenToPercent(int x, int y)
	{
		float fWidth = g_Configuration.ScreenWidth;
		float fHeight = g_Configuration.ScreenHeight;
		float resX = x / fWidth * 100.0f;
		float resY = y / fHeight * 100.0f;
		return std::make_tuple(resX, resY);
	}

	/// Translate a Vec2 of pixel coordinates to Vec2 display position coordinates.
	// To be used with `Strings.DisplayString:GetPosition`.
	// @function ScreenToPercent
	// @tparam Vec2 screenPos Pixel coordinates to translate to display position.
	// @treturn Vec2 Display position.
	// @usage
	// -- Example: Translate pixel coordinates to display position.
	// local screenPos = TEN.Vec2(400, 300)
	// local percentPos = TEN.Util.ScreenToPercent(screenPos)
	//
	// -- Example 2: Using ScreenToPercent with DisplayString:GetPosition.
	// local textPos = str1:GetPosition()
	// local percentPos = TEN.Util.ScreenToPercent(textPos)
	static Vec2 ScreenToPercent(const Vec2& screenPos)
	{
		auto [percentX, percentY] = ScreenToPercent((int)screenPos.x, (int)screenPos.y);
		return Vec2(percentX, percentY);
	}

	/// Pick a moveable by the given display position.
	// @function PickMoveableByDisplayPosition
	// @tparam Vec2 position Display space position in percent.
	// @treturn Objects.Moveable Picked moveable (nil if no moveable was found under the cursor).
	// @usage
	// -- Example: Pick a moveable at the center of the screen.
	// local screenCenter = TEN.Vec2(50, 50)
	// local pickedMoveable = TEN.Util.PickMoveableByDisplayPosition(screenCenter)
	// if pickedMoveable then
	//     print("Picked moveable: " .. pickedMoveable:GetName())
	// else
	//     print("No moveable found at the center of the screen.")
	// end
	static sol::optional <std::unique_ptr<Moveable>> PickMoveable(const Vec2& screenPos)
	{
		auto realScreenPos = PercentToScreen(screenPos.x, screenPos.y);
		auto ray = GetRayFrom2DPosition(Vector2(int(std::get<0>(realScreenPos)), int(std::get<1>(realScreenPos))));

		auto vector = Vector3i::Zero;
		int itemNumber = ObjectOnLOS2(&ray.first, &ray.second, &vector, nullptr);

		if (itemNumber == NO_LOS_ITEM || itemNumber < 0)
			return sol::nullopt;

		return std::make_unique<Moveable>(itemNumber);
	}

	/// Pick a static mesh by the given display position.
	// @function PickStaticByDisplayPosition
	// @tparam Vec2 position Display space position in percent.
	// @treturn Objects.Static Picked static mesh (nil if no static mesh was found under the cursor).
	// @usage
	// -- Example: Pick a static mesh at the center of the screen.
	// local screenCenter = TEN.Vec2(50, 50)
	// local pickedStatic = TEN.Util.PickStaticByDisplayPosition(screenCenter)
	// if pickedStatic then
	//     print("Picked static mesh.")
	// else
	//     print("No static mesh found at the center of the screen.")
	// end
	static sol::optional <std::unique_ptr<Static>> PickStatic(const Vec2& screenPos)
	{
		auto realScreenPos = PercentToScreen(screenPos.x, screenPos.y);
		auto ray = GetRayFrom2DPosition(Vector2((int)std::get<0>(realScreenPos), (int)std::get<1>(realScreenPos)));

		StaticMesh* mesh = nullptr;
		auto vector = Vector3i::Zero;
		int itemNumber = ObjectOnLOS2(&ray.first, &ray.second, &vector, &mesh, GAME_OBJECT_ID::ID_LARA);

		if (itemNumber == NO_LOS_ITEM || itemNumber >= 0)
			return sol::nullopt;

		return std::make_unique<Static>(*mesh);
	}

	/// Write messages within the Log file.<br>
	// For native Lua handling of errors, see <a href="https://www.lua.org/pil/8.3.html">Error management</a> and <a href="https://www.lua.org/manual/5.4/manual.html#pdf-debug.traceback">debug.traceback</a>
	// @function PrintLog
	// @tparam string Message to be displayed within the log.
	// @tparam Util.LogLevel logLevel Log level to be displayed.
	// @tparam[opt] bool allowSpam If true, allows continuous spamming of the message.
	// @usage
	// TEN.Util.PrintLog('test info log', TEN.Util.LogLevel.INFO)
	// TEN.Util.PrintLog('test warning log', TEN.Util.LogLevel.WARNING)
	// TEN.Util.PrintLog('test error log', TEN.Util.LogLevel.ERROR)
	// -- spammed message
	// TEN.Util.PrintLog('test spam log', TEN.Util.LogLevel.INFO, true)
	static void PrintLog(const std::string& message, const LogLevel& level, TypeOrNil<bool> allowSpam)
	{
		TENLog(message, level, LogConfig::All, ValueOr<bool>(allowSpam, false));
	}

	static float CalculateDistance(const Vec3& posA, const Vec3& posB)
	{
		return posA.Distance(posB);
	}

	/// Converts ObjectID to a string. Used by Custom Inventory module to retrieve examine texts.
	// @function GetObjectIDString
	// @tparam Objects.ObjID objectID ID of the object.
	// @treturn string ObjectID converted to string.
	static std::string GetObjectIDString(GAME_OBJECT_ID objectID)
	{
		return GetObjectName(objectID);
	}

	/// Runs the OnUseItem callback. Used by Custom Inventory module to enable OnItemUse callbacks.
	// @function OnUseItemCallBack
	static void OnUseItemCallBack()
	{
		g_GameScript->OnUseItem((GAME_OBJECT_ID)g_Gui.GetInventoryItemChosen());
		HandleAllGlobalEvents(EventType::UseItem, (Activator)short(LaraItem->Index));

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
		tableUtil.set_function(ScriptReserved_PercentToScreen,
			sol::overload(
				static_cast<std::tuple<int, int>(*)(float, float)>(&PercentToScreen),
				static_cast<Vec2(*)(const Vec2&)>(&PercentToScreen)
			)
		);
		tableUtil.set_function(ScriptReserved_ScreenToPercent,
			sol::overload(
				static_cast<std::tuple<float, float>(*)(int, int)>(&ScreenToPercent),
				static_cast<Vec2(*)(const Vec2&)>(&ScreenToPercent)
			)
		);
		tableUtil.set_function(ScriptReserved_PrintLog, &PrintLog);
		tableUtil.set_function(ScriptReserved_GetObjectIDString, &GetObjectIDString);
		tableUtil.set_function(ScriptReserved_OnUseItemCallBack, &OnUseItemCallBack);

		// COMPATIBILITY
		tableUtil.set_function("CalculateDistance", &CalculateDistance);

		auto handler = LuaHandler(state);
		handler.MakeReadOnlyTable(tableUtil, ScriptReserved_LogLevel, LOG_LEVEL_IDS);
	}
}
