#include "framework.h"
#include "Scripting/Internal/TEN/Collision/CollisionHandler.h"

#include "Scripting/Internal/LuaHandler.h"
#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/TEN/Collision/IntersectionTypes.h"
#include "Scripting/Internal/TEN/Collision/Los.h"
#include "Scripting/Internal/TEN/Collision/MaterialTypes.h"
#include "Scripting/Internal/TEN/Collision/Probe.h"

namespace TEN::Scripting::Collision
{
	void Register(sol::state* state, sol::table& parent)
	{
		auto collTable = sol::table(state->lua_state(), sol::create);
		parent.set(ScriptReserved_Collision, collTable);

		Probe::Register(collTable);
		Ray::Register(collTable);

		auto handler = LuaHandler(state);
		handler.MakeReadOnlyTable(collTable, ScriptReserved_MaterialType, MATERIAL_TYPES);
		handler.MakeReadOnlyTable(collTable, ScriptReserved_IntersectionType, INTERSECTION_TYPE);
	}
}