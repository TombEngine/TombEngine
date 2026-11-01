#pragma once

#include "Scripting/Internal/TEN/Types/Vec2/Vec2.h"
#include "Scripting/Internal/TEN/Types/Vec3/Vec3.h"
#include "Scripting/Internal/TEN/Types/Color/Color.h"
#include "Scripting/Internal/TEN/Types/Rotation/Rotation.h"
#include "Scripting/Internal/TEN/Types/Time/Time.h"
 
namespace TEN::Scripting::Properties
{
	using TEN::Scripting::Types::ScriptColor;
	using TEN::Scripting::Rotation;
	using TEN::Scripting::Time;

	// Variant holding any supported property value.
	using PropertyValue = std::variant<bool, float, std::string, Vec2, Vec3, ScriptColor, Rotation, Time>;

	// Extract a typed value from a PropertyValue.
	// Integral types (except bool) are read from the stored float and cast automatically.
	template <typename T>
	std::optional<T> ExtractValue(const PropertyValue& value)
	{
		if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>)
		{
			auto* ptr = std::get_if<float>(&value);
			return ptr ? std::optional<T>(static_cast<T>(*ptr)) : std::nullopt;
		}
		else
		{
			auto* ptr = std::get_if<T>(&value);
			return ptr ? std::optional<T>(*ptr) : std::nullopt;
		}
	}
}
