#pragma once

#include "Scripting/Internal/ScriptAssert.h"
#include "Scripting/Internal/TEN/Properties/PropertyValue.h"

// Conversion helpers between PropertyValue and Lua sol::object.
// Used by Moveable, Static, and ObjectsHandler property API implementations.

namespace TEN::Scripting::Properties
{
	// Stored Lua state for property-to-Lua conversion. Set once during handler initialization.
	inline lua_State* s_propertyLuaState = nullptr;

	inline void InitPropertyLua(sol::state& state)
	{
		s_propertyLuaState = state.lua_state();
	}

	// Convert a Lua value to a PropertyValue.
	inline PropertyValue PropertyValueFromLua(const sol::object& obj)
	{
		switch (obj.get_type())
		{
		case sol::type::boolean:
			return obj.as<bool>();

		case sol::type::number:
			return obj.as<float>();

		case sol::type::string:
			return obj.as<std::string>();

		case sol::type::userdata:
		{
			if (obj.is<Vec2>())
				return obj.as<Vec2>();

			if (obj.is<Vec3>())
				return obj.as<Vec3>();

			if (obj.is<ScriptColor>())
				return obj.as<ScriptColor>();

			if (obj.is<Rotation>())
				return obj.as<Rotation>();

			if (obj.is<Time>())
				return obj.as<Time>();

			ScriptAssert(false, "Unsupported userdata type for property value.");
			return false;
		}

		default:
			ScriptAssert(false, "Unsupported Lua type for property value. Supported: bool, number, string, Vec2, Vec3, Color, Rotation, Time.");
			return false;
		}
	}

	// Convert a PropertyValue to a Lua sol::object.
	inline sol::object PropertyValueToLua(const PropertyValue& value)
	{
		return std::visit([](const auto& val) -> sol::object
		{
			return sol::make_object(s_propertyLuaState, val);
		}, value);
	}
}
