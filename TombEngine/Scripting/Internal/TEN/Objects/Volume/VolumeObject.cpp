#pragma once
#include "framework.h"
#include "Scripting/Internal/TEN/Objects/Volume/VolumeObject.h"

#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/ScriptAssert.h"
#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/TEN/Types/Rotation/Rotation.h"
#include "Scripting/Internal/TEN/Types/Vec3/Vec3.h"
#include "Specific/level.h"

/***
Activator volume.

@tenclass Objects.Volume
@pragma nostrip
*/

static auto IndexError = IndexErrorMaker(Volume, ScriptReserved_Volume);
static auto NewIndexError = NewIndexErrorMaker(Volume, ScriptReserved_Volume);

Volume::Volume(TriggerVolume& volume) :
	_volume(volume)
{};

void Volume::Register(sol::table& parent)
{
	parent.new_usertype<Volume>(ScriptReserved_Volume,
		sol::no_constructor, // TODO: Ability to spawn new volumes could be added later.
		sol::meta_function::index, IndexError,
		sol::meta_function::new_index, NewIndexError,

		ScriptReserved_GetName, &Volume::GetName,
		ScriptReserved_GetPosition, &Volume::GetPos,
		ScriptReserved_GetRotation, &Volume::GetRot,
		ScriptReserved_GetScale, &Volume::GetScale,

		ScriptReserved_SetName, &Volume::SetName,
		ScriptReserved_SetPosition, &Volume::SetPos,
		ScriptReserved_SetRotation, &Volume::SetRot,
		ScriptReserved_SetScale, &Volume::SetScale,

		ScriptReserved_Enable, &Volume::Enable,
		ScriptReserved_Disable, &Volume::Disable,
		ScriptReserved_ClearActivators, &Volume::ClearActivators,

		ScriptReserved_GetActive, &Volume::GetActive,
		ScriptReserved_IsMoveableInside, &Volume::IsMoveableInside);
}

/// Get the unique string identifier of this volume.
// @function Volume:GetName
// @treturn string Name.
std::string Volume::GetName() const
{
	return _volume.Name;
}

/// Get the position of this volume.
// @function Volume:GetPosition
// @treturn Vec3 Position.
Vec3 Volume::GetPos() const
{
	return Vec3(_volume.Box.Center.x, _volume.Box.Center.y, _volume.Box.Center.z);
}

/// Get the rotation of this volume.
// @function Volume:GetRotation
// @treturn Rotation Rotation.
Rotation Volume::GetRot() const
{
	auto eulers = EulerAngles(_volume.Box.Orientation);
	return Rotation(TO_DEGREES(eulers.x), TO_DEGREES(eulers.y), TO_DEGREES(eulers.z));
}

/// Get this scale of this volume.
// @function Volume:GetScale
// @treturn Vec3 Scale.
Vec3 Volume::GetScale() const
{
	return Vec3((Vector3)_volume.Box.Extents);
}

/// Set the unique string identifier of this volume.
// @function Volume:SetName
// @tparam string name New name.
void Volume::SetName(const std::string& name)
{
	if (!ScriptAssert(!name.empty(), "Attempted to set name to blank string."))
		return;

	// Remove previous name if it exists.
	if (_callbackSetName(name, _volume))
	{
		_callbackRemoveName(_volume.Name);
		_volume.Name = name;
	}
	else
	{
		ScriptAssertF(false, "Could not add name {}. Object with this name may already exist.", name);
		TENLog("Name not set.", LogLevel::Warning, LogConfig::All);
	}
}

/// Set the position of this volume.
// @function Volume:SetPosition
// @tparam Vec3 pos New position.
void Volume::SetPos(const Vec3& pos)
{
	_volume.Box.Center =
	_volume.Sphere.Center = pos.ToVector3();
}

/// Set the rotation of this volume.
// @function Volume:SetRotation
// @tparam Rotation rot New rotation.
void Volume::SetRot(const Rotation& rot)
{
	auto eulers = EulerAngles(ANGLE(rot.x), ANGLE(rot.y), ANGLE(rot.z));
	_volume.Box.Orientation = eulers.ToQuaternion();
}

/// Set the scale of the volume.
// @function Volume:SetScale
// @tparam Vec3 scale New scale.
void Volume::SetScale(const Vec3& scale)
{
	_volume.Box.Extents = scale.ToVector3();
	_volume.Sphere.Radius = _volume.Box.Extents.x;
}

/// Determine if this volume is active.
// @function Volume:GetActive
// @treturn bool Boolean representing active status.
bool Volume::GetActive() const
{
	return _volume.Enabled;
}

/// Determine if a moveable is inside this volume.
// @function Volume:IsMoveableInside
// @tparam Objects.Moveable moveable Moveable to be checked for containment.
// @treturn bool Boolean representing containment status.
bool Volume::IsMoveableInside(const Moveable& mov)
{
	for (const auto& entry : _volume.StateQueue)
	{
		if (std::holds_alternative<int>(entry.Activator))
		{
			int id = std::get<int>(entry.Activator);
			auto& mov2 = std::make_unique<Moveable>(id);

			if (mov2.get()->GetName() == mov.GetName())
				return true;
		}
	}

	return false;
}

/// Enable this volume.
// @function Volume:Enable
void Volume::Enable()
{
	_volume.Enabled = true;
}

/// Disable this volume.
// @function Volume:Disable
void Volume::Disable()
{
	ClearActivators();
	_volume.Enabled = false;
}

/// Clear the activators for this volume, allowing it to trigger again.
// @function Volume:ClearActivators
void Volume::ClearActivators()
{
	_volume.StateQueue.clear();
}
