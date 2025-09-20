#include "framework.h"

#include "Game/effects/debris.h"
#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/ScriptAssert.h"
#include "Scripting/Internal/TEN/Objects/Static/StaticObject.h"
#include "Scripting/Internal/TEN/Types/Color/Color.h"
#include "Scripting/Internal/TEN/Types/Rotation/Rotation.h"
#include "Scripting/Internal/TEN/Types/Vec3/Vec3.h"

using namespace TEN::Scripting::Types;

/// Represents a static object in the game world.
//
// @tenclass Objects.Static
// @pragma nostrip

namespace TEN::Scripting
{
	static auto IndexError = IndexErrorMaker(Static, ScriptReserved_Static);
	static auto NewIndexError = NewIndexErrorMaker(Static, ScriptReserved_Static);

	void Static::Register(sol::table& parent)
	{
		// Register type.
		parent.new_usertype<Static>(
			ScriptReserved_Static,
			sol::no_constructor, // TODO: Add feature to spawn statics.
			sol::meta_function::index, IndexError,
			sol::meta_function::new_index, NewIndexError,

			// Getters
			ScriptReserved_StaticGetName, &Static::GetName,
			ScriptReserved_StaticGetSlot, &Static::GetSlot,
			ScriptReserved_StaticGetPosition, &Static::GetPosition,
			ScriptReserved_StaticGetRotation, &Static::GetRotation,
			ScriptReserved_StaticGetScale, &Static::GetScale,
			ScriptReserved_StaticGetColor, &Static::GetColor,
			ScriptReserved_StaticGetHP, &Static::GetHP,
			ScriptReserved_StaticGetActive, &Static::GetActiveStatus, // TODO: Deprecate. Rename Lua func to GetActiveStatus.
			ScriptReserved_StaticGetCollidable, &Static::GetCollidable,
			ScriptReserved_StaticGetSolid, &Static::GetSolidStatus, // TODO: Deprecate. Rename Lua func to GetSolidStatus.

			// Setters
			ScriptReserved_StaticSetName, &Static::SetName,
			ScriptReserved_StaticSetSlot, &Static::SetSlot,
			ScriptReserved_StaticSetPosition, &Static::SetPosition,
			ScriptReserved_StaticSetRotation, &Static::SetRotation,
			ScriptReserved_StaticSetScale, sol::overload(
				(void(Static::*)(const Vec3&))(&Static::SetScale),
				(void(Static::*)(float))(&Static::SetScale)), // COMPATIBILITY
			ScriptReserved_StaticSetColor, &Static::SetColor,
			ScriptReserved_StaticSetHP, &Static::SetHP,
			ScriptReserved_StaticSetCollidable, &Static::SetCollidable,
			ScriptReserved_StaticSetSolid, &Static::SetSolidStatus, // TODO: Deprecate. Rename Lua func to SetSolidStatus.
			
			// Utilities
			ScriptReserved_StaticEnable, &Static::Enable,
			ScriptReserved_StaticDisable, &Static::Disable,
			ScriptReserved_StaticShatter, &Static::Shatter);
	}

	Static::Static(StaticMesh& staticObj) :
		_static(staticObj)
	{
	};

	/// Get this static's unique string identifier.
	// @function Static:GetName
	// @treturn string Name string.
	std::string Static::GetName() const
	{
		return _static.Name;
	}

	/// Get this static's slot ID.
	// @function Static:GetSlot
	// @treturn int Slot ID.
	int Static::GetSlot() const
	{
		return _static.Slot;
	}

	/// Get this static's world position.
	// @function Static:GetPosition
	// @treturn Vec3 World position.
	Vec3 Static::GetPosition() const
	{
		return Vec3(_static.Pose.Position);
	}

	/// Get this static's world rotation.
	// @function Static:GetRotation
	// @treturn Rotation World rotation.
	Rotation Static::GetRotation() const
	{
		return Rotation(_static.Pose.Orientation);
	}

	/// Get this static's world scale.
	// @function Static:GetScale
	// @treturn Vec3 World scale.
	Vec3 Static::GetScale() const
	{
		return Vec3(_static.Pose.Scale);
	}

	/// Get this static's color.
	// @function Static:GetColor
	// @treturn Color Color.
	ScriptColor Static::GetColor() const
	{
		return ScriptColor(_static.Color);
	}

	/// Get this static's hit points. Used only with shatterable statics.
	// @function Static:GetHP
	// @treturn int Hit points.
	int Static::GetHP() const
	{
		return _static.HitPoints;
	}

	/// Get this static's visibility status.
	// @function Static:GetActive
	// @treturn bool Status. true means visible, false otherwise.
	bool Static::GetActiveStatus() const
	{
		return ((_static.Flags & StaticMeshFlags::SM_VISIBLE) != 0);
	}

	/// Get this static's collision status.
	// @function Static:GetCollidable
	// @treturn bool Collision status. true if can be collided with, false otherwise.
	bool Static::GetCollidable() const
	{
		return ((_static.Flags & StaticMeshFlags::SM_COLLISION) != 0);
	}

	/// Get this static's solid collision status.
	// @function Static:GetSolid
	// @treturn bool Solid Status. true if solid, false if soft.
	bool Static::GetSolidStatus() const
	{
		return ((_static.Flags & StaticMeshFlags::SM_SOLID) != 0);
	}

	/// Set this static's unique identifier string.
	// @function Static:SetName
	// @tparam string name New name.
	void Static::SetName(const std::string& name)
	{
		if (!ScriptAssert(!name.empty(), "Name cannot be blank. Not setting name."))
			return;

		if (_callbackSetName(name, _static))
		{
			_callbackRemoveName(_static.Name);
			_static.Name = name;
		}
		else
		{
			ScriptAssertF(false, "Could not add name {} - an object with this name may already exist.", name);
			TENLog("Name will not be set.", LogLevel::Warning, LogConfig::All);
		}
	}

	/// Set this static's slot ID.
	// @function Static:SetSlot
	// @tparam int slotID New slot ID.
	void Static::SetSlot(int slotID)
	{
		_static.Slot = slotID;
		_static.Dirty = true;
	}

	/// Set this static's world position.
	// @function Static:SetPosition
	// @tparam Vec3 pos New world position.
	void Static::SetPosition(const Vec3& pos)
	{
		_static.Pose.Position = pos.ToVector3i();
		_static.Dirty = true;
	}

	/// Set this static's rotation.
	// @function Static:SetRotation
	// @tparam Rotation rot New rotation.
	void Static::SetRotation(const Rotation& rot)
	{
		_static.Pose.Orientation = rot.ToEulerAngles();
		_static.Dirty = true;
	}

	/// Set this static's world scale.
	// @function Static:SetScale
	// @tparam Vec3 scale New world scale.
	void Static::SetScale(const Vec3& scale)
	{
		_static.Pose.Scale = scale.ToVector3();
		_static.Dirty = true;
	}

	void Static::SetScale(float scale)
	{
		_static.Pose.Scale = Vector3(scale);
		_static.Dirty = true;
	}

	/// Set this static's color.
	// @function Static:SetColor
	// @tparam Color color New color.
	void Static::SetColor(ScriptColor const& col)
	{
		_static.Color = col;
		_static.Dirty = true;
	}

	/// Set this static's hit points. Used only with shatterable statics.
	// @function Static:SetHP
	// @tparam int hitPoints New hit points.
	void Static::SetHP(int hitPoints)
	{
		_static.HitPoints = hitPoints;
	}

	/// Set this static's solid collision status.
	// @function Static:SetSolid
	// @tparam bool status New status, true is solid, false is soft.
	void Static::SetSolidStatus(bool status)
	{
		if (status)
		{
			_static.Flags |= StaticMeshFlags::SM_SOLID;
		}
		else
		{
			_static.Flags &= ~StaticMeshFlags::SM_SOLID;
		}
	}

	/// Set this static's collision status.
	// @function Static:SetCollidable
	// @tparam bool collidable New collision status. true if can be collided with, false: no collision.
	void Static::SetCollidable(bool collidable)
	{
		if (collidable)
		{
			_static.Flags |= StaticMeshFlags::SM_COLLISION;
		}
		else
		{
			_static.Flags &= ~StaticMeshFlags::SM_COLLISION;
		}
	}

	/// Enable this static. Used when previously shattered disabled manually.
	// @function Static:Enable
	void Static::Enable()
	{
		_static.Flags |= StaticMeshFlags::SM_VISIBLE;
	}

	/// Disable this static.
	// @function Static:Disable
	void Static::Disable()
	{
		_static.Flags &= ~StaticMeshFlags::SM_VISIBLE;
	}

	/// Shatter this static.
	// @function Static:Shatter
	void Static::Shatter()
	{
		ShatterObject(nullptr, &_static, -128, _static.RoomNumber, 0);
	}
}
