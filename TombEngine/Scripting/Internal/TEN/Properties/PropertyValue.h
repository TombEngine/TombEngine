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
}
