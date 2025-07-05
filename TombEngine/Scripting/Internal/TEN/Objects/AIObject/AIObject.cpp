#pragma once

#include "framework.h"
#include "Scripting/Internal/TEN/Objects/AIObject/AIObject.h"

#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/ScriptAssert.h"
#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/TEN/Types/Vec3/Vec3.h"
/***
AI object

@tenclass Objects.AIObject
@pragma nostrip
*/

static auto IndexError = IndexErrorMaker(AIObject, ScriptReserved_AIObject);
static auto NewIndexError = NewIndexErrorMaker(AIObject, ScriptReserved_AIObject);

AIObject::AIObject(AI_OBJECT & ref) : m_aiObject{ref}
{};

void AIObject::Register(sol::table& parent)
{
	parent.new_usertype<AIObject>(ScriptReserved_AIObject,
		sol::no_constructor, // ability to spawn new ones could be added later
		sol::meta_function::index, IndexError,
		sol::meta_function::new_index, NewIndexError,

		/// Get the object's position.
		// @function AIObject:GetPosition
		// @treturn Vec3 A copy of the object's position.
		ScriptReserved_GetPosition, &AIObject::GetPos,

		/// Set the object's position.
		// @function AIObject:SetPosition
		// @tparam Vec3 position The new position of the object.
		ScriptReserved_SetPosition, &AIObject::SetPos,

		/// Get the object's Y-axis rotation.
		// @function AIObject:GetRotationY
		// @treturn int The object's Y-axis rotation.
		ScriptReserved_GetRotationY, &AIObject::GetYRot,

		/// Set the object's Y-axis rotation.
		// @function AIObject:SetRotationY
		// @tparam int rotation The object's new Y-axis rotation.
		ScriptReserved_SetRotationY, &AIObject::SetYRot,

		/// Get the object's unique string identifier.
		// @function AIObject:GetName
		// @treturn string The object's name.
		ScriptReserved_GetName, &AIObject::GetName,

		/// Set the object's unique string identifier.
		// @function AIObject:SetName
		// @tparam string name The object's new name.
		ScriptReserved_SetName, &AIObject::SetName,

		/// Get the current room of the object.
		// @function AIObject:GetRoom
		// @treturn Objects.Room current room of the object.
		ScriptReserved_GetRoom, &AIObject::GetRoom,

		/// Get the current room number of the object.
		// @function AIObject:GetRoomNumber
		// @treturn int Number representing the current room of the object.
		ScriptReserved_GetRoomNumber, &AIObject::GetRoomNumber,

		/// Set room number of the object.
		// This is used in conjunction with SetPosition to teleport the object to a new room.
		// @function AIObject:SetRoomNumber
		// @tparam int ID The ID of the new room.
		ScriptReserved_SetRoomNumber, &AIObject::SetRoomNumber,

		/// Retrieve the object ID.
		// @function AIObject:GetObjectID
		// @treturn int A number representing the ID of the object.
		ScriptReserved_GetObjectID, &AIObject::GetObjectID,

		/// Change the object ID. This will change the type of AI object it is.
		// Note that a baddy will gain the behaviour of the tile it's on _before_ said baddy is triggered.
		// This means that changing the type of an AI object beneath a moveable will have no effect.
		// Instead, this function can be used to change an object that the baddy isn't standing on.
		// For example, you could have a pair of AI_GUARD objects, and change one or the other two
		// AI_PATROL_1 based on whether the player has a certain item or not.
		// @function AIObject:SetObjectID
		// @tparam Objects.ObjID ID the new ID.
		// @usage
		// aiObj = TEN.Objects.GetMoveableByName("ai_guard_sphinx_room")
		// aiObj:SetObjectID(TEN.Objects.ObjID.AI_PATROL1)
		ScriptReserved_SetObjectID, &AIObject::SetObjectID
		);
}

Vec3 AIObject::GetPos() const
{
	return Vec3(m_aiObject.pos.Position.x, m_aiObject.pos.Position.y, m_aiObject.pos.Position.z);
}

void AIObject::SetPos(Vec3 const& pos)
{
	m_aiObject.pos.Position.x = pos.x;
	m_aiObject.pos.Position.y = pos.y;
	m_aiObject.pos.Position.z = pos.z;
}

GAME_OBJECT_ID AIObject::GetObjectID() const
{
	return m_aiObject.objectNumber;
}

void AIObject::SetObjectID(GAME_OBJECT_ID objNum)
{
	m_aiObject.objectNumber = objNum;
}

float AIObject::GetYRot() const
{
	return TO_DEGREES(m_aiObject.pos.Orientation.y);
}

void AIObject::SetYRot(float yRot)
{
	m_aiObject.pos.Orientation.y = ANGLE(yRot);
}

std::string AIObject::GetName() const
{
	return m_aiObject.Name;
}

void AIObject::SetName(const std::string& id)
{
	if (!ScriptAssert(!id.empty(), "Name cannot be blank. Not setting name."))
		return;

	if (_callbackSetName(id, m_aiObject))
	{
		// Remove old name if it exists.
		_callbackRemoveName(m_aiObject.Name);
		m_aiObject.Name = id;
	}
	else
	{
		ScriptAssertF(false, "Could not add name {} - does an object with this name already exist?", id);
		TENLog("Name will not be set", LogLevel::Warning, LogConfig::All);
	}
}

std::unique_ptr<Room> AIObject::GetRoom() const
{
	return std::make_unique<Room>(g_Level.Rooms[m_aiObject.roomNumber]);
}

int AIObject::GetRoomNumber() const
{
	return m_aiObject.roomNumber;
}

void AIObject::SetRoomNumber(short room)
{
	const size_t nRooms = g_Level.Rooms.size();
	if (room < 0 || static_cast<size_t>(room) >= nRooms)
	{
		ScriptAssertF(false, "Invalid room number: {}. Value must be in range [0, {})", room, nRooms);
		TENLog("Room number will not be set", LogLevel::Warning, LogConfig::All);
		return;
	}

	m_aiObject.roomNumber = room;
}
