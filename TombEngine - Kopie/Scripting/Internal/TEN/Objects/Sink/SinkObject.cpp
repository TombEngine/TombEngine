#pragma once

#include "framework.h"
#include "Scripting/Internal/TEN/Objects/Sink/SinkObject.h"

#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/ScriptAssert.h"
#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/TEN/Types/Vec3/Vec3.h"

/// Represents a sink object.
//
// @tenclass Objects.Sink
// @pragma nostrip

namespace TEN::Scripting::Objects
{
	static auto IndexError = IndexErrorMaker(Sink, ScriptReserved_Sink);
	static auto NewIndexError = NewIndexErrorMaker(Sink, ScriptReserved_Sink);

	Sink::Sink(SinkInfo& sink) :
		_sink(sink)
	{
	};

	void Sink::Register(sol::table& parent)
	{
		parent.new_usertype<Sink>(
			ScriptReserved_Sink,
			sol::no_constructor, // TODO: Add feature to spawn sinks.
			sol::meta_function::index, IndexError,
			sol::meta_function::new_index, NewIndexError,

			ScriptReserved_SinkGetName, &Sink::GetName,
			ScriptReserved_SinkGetPosition, &Sink::GetPosition,
			ScriptReserved_SinkGetStrength, &Sink::GetStrength,

			ScriptReserved_SinkSetName, &Sink::SetName,
			ScriptReserved_SinkSetPosition, &Sink::SetPosition,
			ScriptReserved_SinkSetStrength, &Sink::SetStrength);
	}

	/// Get this sink's unique string identifier.
	// @function Sink:GetName
	// @treturn string Name.
	std::string Sink::GetName() const
	{
		return _sink.Name;
	}

	/// Get this sink's world position.
	// @function Sink:GetPosition
	// @treturn Vec3 World position.
	Vec3 Sink::GetPosition() const
	{
		return Vec3(_sink.Position);
	}

	/// Get this sink's strength.
	// @function Sink:GetStrength
	// @treturn int Strength.
	int Sink::GetStrength() const
	{
		return _sink.Strength;
	}

	/// Set this sink's unique string identifier.
	// @function Sink:SetName
	// @tparam string name New name.
	void Sink::SetName(const std::string& name)
	{
		if (!ScriptAssert(!name.empty(), "Name cannot be blank. Not setting name."))
			return;

		if (_callbackSetName(name, _sink))
		{
			_callbackRemoveName(_sink.Name);
			_sink.Name = name;
		}
		else
		{
			ScriptAssertF(false, "Could not add name {} - object with this name may already exist.", name);
			TENLog("Name will not be set.", LogLevel::Warning, LogConfig::All);
		}
	}

	/// Set this sink's world position.
	// @function Sink:SetPosition
	// @tparam Vec3 pos New world position.
	void Sink::SetPosition(const Vec3& pos)
	{
		_sink.Position = pos.ToVector3();
	}

	/// Set this sink's strength.
	// Higher numbers provide stronger currents. Clamped to the range [1, 32].
	// @function Sink:SetStrength
	// @tparam int strength New strength.
	void Sink::SetStrength(int strength)
	{
		constexpr auto STRENGTH_MAX = 32;
		constexpr auto STRENGTH_MIN = 1;

		_sink.Strength = std::clamp(strength, STRENGTH_MIN, STRENGTH_MAX);
	}
}
