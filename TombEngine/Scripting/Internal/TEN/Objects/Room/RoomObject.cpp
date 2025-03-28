#pragma once
#include "framework.h"

#include "Renderer/Renderer.h"
#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/ScriptAssert.h"
#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/TEN/Objects/Room/RoomFlags.h"
#include "Scripting/Internal/TEN/Objects/Room/RoomObject.h"
#include "Scripting/Internal/TEN/Types/Color/Color.h"
#include "Scripting/Internal/TEN/Types/Vec3/Vec3.h"
#include "Sound/sound.h"
#include "Specific/level.h"
#include "Specific/trutils.h"

using namespace TEN::Scripting::Types;

/// Room object.
// @tenclass Objects.Room
// @pragma nostrip

//namespace TEN::Scripting
//{
	static auto IndexError = IndexErrorMaker(Room, ScriptReserved_Volume);
	static auto NewIndexError = NewIndexErrorMaker(Room, ScriptReserved_Volume);

	Room::Room(RoomData& room) :
		_room(room)
	{
	};

	void Room::Register(sol::table& parent)
	{
		// Register type.
		parent.new_usertype<Room>(
			ScriptReserved_Room,
			sol::no_constructor,
			sol::meta_function::index, IndexError,
			sol::meta_function::new_index, NewIndexError,

			ScriptReserved_RoomGetRoomNumber, &Room::GetRoomNumber,
			ScriptReserved_RoomGetName, &Room::GetName,
			ScriptReserved_RoomGetColor, &Room::GetColor,
			ScriptReserved_RoomGetReverbType, &Room::GetReverbType,
			ScriptReserved_RoomSetName, &Room::SetName,
			ScriptReserved_RoomSetReverbType, &Room::SetReverbType,
			ScriptReserved_RoomSetFlag, &Room::SetFlag,
			ScriptReserved_RoomIsTagPresent, &Room::IsTagPresent,
			ScriptReserved_RoomGetActive, &Room::GetActive,
			ScriptReserved_RoomGetFlag, &Room::GetFlag);
	}

	/// Get the room's number.
	// @function Room:GetRoomNumber
	// @treturn int Room number.
	int Room::GetRoomNumber() const
	{
		return _room.RoomNumber;
	}

	/// Get the room's unique string identifier.
	// @function Room:GetName
	// @treturn string Room name.
	std::string Room::GetName() const
	{
		return _room.Name;
	}

	/// Get the room's ambient light color.
	// @function Room:GetColor
	// @treturn Color Ambient light color.
	ScriptColor Room::GetColor() const
	{
		return ScriptColor(_room.ambient);
	}

	/// Get the room's reverb type.
	// @function Room:GetReverbType
	// @treturn Objects.RoomReverb Reverb type.
	ReverbType Room::GetReverbType() const
	{
		return _room.reverbType;
	}

	/// Set the room's unique string identifier.
	// @function Room:SetName
	// @tparam string name New name.
	void Room::SetName(const std::string& name)
	{
		if (!ScriptAssert(!name.empty(), "Unable to set name. Name cannot be blank."))
			return;

		// Remove previous name if it already exists.
		if (_callbackSetName(name, _room))
		{
			_callbackRemoveName(_room.Name);
			_room.Name = name;
		}
		else
		{
			ScriptAssertF(false, "Could not add name {} - does an object with this name already exist?", name);
			TENLog("Name will not be set", LogLevel::Warning, LogConfig::All);
		}
	}

	/// Set the room's reverb type.
	// @function Room:SetReverbType
	// @tparam Objects.RoomReverb reverb Reverb type.
	void Room::SetReverbType(ReverbType reverb)
	{
		_room.reverbType = reverb;
	}

	/// Set the room's specified flag.
	// @function Room:SetFlag
	// @tparam Objects.RoomFlagID flagID Room flag ID.
	// @tparam bool value Boolean to set the flag to.
	void Room::SetFlag(RoomEnvFlags flag, bool value)
	{
		if (value)
		{
			_room.flags |= flag;
		}
		else
		{
			_room.flags &= ~flag;
		}
	}

	/// Get the room's specified flag value (true or false).
	// @function Room:GetFlag
	// @tparam Objects.RoomFlagID flagID Room flag ID.
	bool Room::IsTagPresent(const std::string& tag) const
	{
		if (_room.Tags.empty())
			return false;

		return std::any_of(
			_room.Tags.begin(), _room.Tags.end(),
			[&tag](const std::string& value)
			{
				return (value == tag);
			});
	}

	/// Check if the specified tag is set for the room.
	// @function Room:IsTagPresent
	// @tparam string tag Text tag to check (case sensitive).
	// @treturn bool Boolean of the tag's presence.
	bool Room::GetActive() const
	{
		return _room.Active();
	}

	/// Check if the room is active.
	// @function Room:GetActive
	// @treturn bool Boolean of the room's active status.
	bool Room::GetFlag(RoomEnvFlags flag) const
	{
		return ((_room.flags & flag) == flag);
	}
//}
