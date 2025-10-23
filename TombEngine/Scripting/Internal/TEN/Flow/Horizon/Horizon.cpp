#include "framework.h"
#include "Scripting/Internal/TEN/Flow/Horizon/Horizon.h"

#include "Objects/game_object_ids.h"
#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/TEN/Types/Color/Color.h"
#include "Scripting/Internal/TEN/Types/Rotation/Rotation.h"

namespace TEN::Scripting
{
	/// Represents a horizon. To be used with @{Flow.Level.horizon1} and @{Flow.Level.horizon2} properties.
	//
	// @tenprimitive Flow.Horizon
	// @pragma nostrip

	void Horizon::Register(sol::table& parent)
	{
		using ctors = sol::constructors<
			Horizon(GAME_OBJECT_ID),
			Horizon(bool)>;

		// Register type.
		parent.new_usertype<Horizon>(
			"Horizon",
			ctors(), sol::call_constructor, ctors(),

			/// (bool) Horizon enabled state.
			// If set to true, horizon will be visible.
			// @mem enabled
			"enabled", sol::property(&Horizon::GetEnabled, &Horizon::SetEnabled),

			/// (@{Objects.ObjID}) Horizon object ID.
			// @mem objectID
			"objectID", sol::property(&Horizon::GetObjectID, &Horizon::SetObjectID),

			/// (@{Vec3}) Horizon position.
			// Specifies an offset from the camera origin.
			// @mem position
			"position", sol::property(&Horizon::GetPosition, &Horizon::SetPosition),

			/// (@{Rotation}) Horizon rotation.
			// Specifies horizon rotation.
			// @mem rotation
			"rotation", sol::property(&Horizon::GetRotation, &Horizon::SetRotation),

			/// (float) Horizon transparency.
			// Specifies horizon transparency on a range from 0 to 1.
			// @mem transparency
			"transparency", sol::property(&Horizon::GetTransparency, &Horizon::SetTransparency));
	}

	/// Create a horizon object.
	// @function Horizon
	// @tparam Objects.ObjID objectID Object ID for the horizon to use.
	// @treturn Horizon A new Horizon object.
	Horizon::Horizon(GAME_OBJECT_ID objectID)
	{
		_enabled = true;
		_objectID = objectID;
	}

	// Legacy constructor to maintain backwards compatibility.
	Horizon::Horizon(bool enabled)
	{
		_enabled = enabled;
	}

	bool Horizon::GetEnabled() const
	{
		return _enabled;
	}

	GAME_OBJECT_ID Horizon::GetObjectID() const
	{
		return _objectID;
	}

	const Vec3 Horizon::GetPosition() const
	{
		return _position;
	}

	const Rotation Horizon::GetRotation() const
	{
		return _rotation;
	}

	const float Horizon::GetTransparency() const
	{
		return _transparency;
	}

	void Horizon::SetEnabled(bool value)
	{
		_enabled = value;
	}

	void Horizon::SetObjectID(GAME_OBJECT_ID objectID)
	{
		_objectID = objectID;
	}

	void Horizon::SetPosition(const Vec3& pos, TypeOrNil<bool> noInterpolation)
	{
		_prevPosition = ValueOr<bool>(noInterpolation, false) ? pos : _position;
		_position = pos;
	}

	void Horizon::SetRotation(const Rotation& rot, TypeOrNil<bool> noInterpolation)
	{
		_prevRotation = ValueOr<bool>(noInterpolation, false) ? rot : _rotation;
		_rotation = rot;
	}

	void Horizon::SetTransparency(float value)
	{
		_transparency = value;
	}

	const Vec3 Horizon::GetPrevPosition() const
	{
		return _prevPosition;
	}

	const Rotation Horizon::GetPrevRotation() const
	{
		return _prevRotation;
	}
}
