#pragma once

#include "Scripting/Internal/ScriptAssert.h"
#include "Scripting/Internal/TEN/Properties/PropertyValue.h"

// Conversion helpers between PropertyValue and Lua sol::object.
// Used by Moveable, Static, and ObjectsHandler property API implementations.

namespace TEN::Scripting::Properties
{
	// Validate that a property name is not empty. Returns false and logs error if blank.
	inline bool ValidatePropertyName(const std::string& name)
	{
		return ScriptAssert(!name.empty(), "Property name cannot be blank.");
	}

	// Convert a Lua value to a PropertyValue. Returns std::nullopt on unsupported types.
	inline std::optional<PropertyValue> PropertyValueFromLua(const sol::object& obj)
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
			return std::nullopt;
		}

		default:
			ScriptAssert(false, "Unsupported Lua type for property value. Supported: bool, number, string, Vec2, Vec3, Color, Rotation, Time.");
			return std::nullopt;
		}
	}

	// Convert a PropertyValue to a Lua sol::object.
	inline sol::object PropertyValueToLua(sol::state_view state, const PropertyValue& value)
	{
		return std::visit([&state](const auto& val) -> sol::object
		{
			return sol::make_object(state, val);
		}, value);
	}
}
