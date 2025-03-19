#include "framework.h"
#include "Scripting/Internal/TEN/Objects/Moveable/MoveableObject.h"

#include "Game/collision/floordata.h"
#include "Game/control/lot.h"
#include "Game/effects/debris.h"
#include "Game/effects/item_fx.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Objects/objectslist.h"
#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/ScriptAssert.h"
#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/TEN/Logic/LevelFunc.h"
#include "Scripting/Internal/TEN/Objects/ObjectsHandler.h"
#include "Scripting/Internal/TEN/Types/Color/Color.h"
#include "Scripting/Internal/TEN/Types/Rotation/Rotation.h"
#include "Scripting/Internal/TEN/Types/Vec3/Vec3.h"
#include "Specific/level.h"

using namespace TEN::Collision::Floordata;
using namespace TEN::Effects::Items;
using namespace TEN::Math;
using namespace TEN::Scripting::Types;

/// Represents a moveable object in the game world.
// Examples include the player, traps, enemies, doors, and pickups. See also @{Objects.LaraObject} for player-specific features.
//
// @tenclass Objects.Moveable
// @pragma nostrip

static auto IndexError = IndexErrorMaker(Moveable, ScriptReserved_Moveable);
static auto NewIndexError = NewIndexErrorMaker(Moveable, ScriptReserved_Moveable);

/*** Used to generate a new moveable dynamically at runtime.
For more information on each parameter, see the
associated getters and setters. If you do not know what to set for these,
most can just be ignored (see usage).

	@function Moveable
	@tparam Objects.ObjID object ID
	@tparam string name Lua name.
	@tparam Vec3 position position in level
	@tparam Rotation rotation rotation rotation about x, y, and z axes (default Rotation(0, 0, 0))
	@tparam int roomNumber the room number the moveable is in (default: calculated automatically).
	@tparam int animNumber animation number
	@tparam int frameNumber frame number
	@tparam int hp Hit points.
	@tparam int OCB Object code bits.
	@tparam table AIBits table with AI bits (default { 0, 0, 0, 0, 0, 0 })
	@treturn Moveable A new Moveable object (a wrapper around the new object)

	@usage
	local item = Moveable(
		TEN.Objects.ObjID.PISTOLS_ITEM, -- object id
		"test", -- name
		Vec3(18907, 0, 21201)) -- position
	*/
static std::unique_ptr<Moveable> Create(GAME_OBJECT_ID objID, const std::string& name, const Vec3& pos, const TypeOrNil<Rotation>& rot, TypeOrNil<int> room,
										TypeOrNil<int> animNumber, TypeOrNil<int> frameNumber, TypeOrNil<int> hp, TypeOrNil<int> ocb, const TypeOrNil<aiBitsType>& aiBits)
{
	int movID = CreateItem();
	auto scriptMov = std::make_unique<Moveable>(movID, false);

	if (ScriptAssert(scriptMov->SetName(name), "Could not set name for Moveable. Returning an invalid object."))
	{
		auto& mov = g_Level.Items[movID];

		scriptMov->SetObjectID(objID);

		if (std::holds_alternative<int>(room))
		{
			scriptMov->SetPosition(pos, false);
			scriptMov->SetRoomNumber(std::get<int>(room));
		}
		else
		{
			scriptMov->SetPosition(pos, true);
		}

		scriptMov->SetRotation(ValueOr<Rotation>(rot, Rotation()));
		scriptMov->Initialize();

		if (std::holds_alternative<int>(animNumber))
		{
			scriptMov->SetAnimNumber(std::get<int>(animNumber), objID);
			scriptMov->SetFrameNumber(ValueOr<int>(frameNumber, 0));
		}

		if (std::holds_alternative<int>(hp))
		{
			scriptMov->SetHP(std::get<int>(hp));
		}

		scriptMov->SetOcb(ValueOr<int>(ocb, 0));
		scriptMov->SetAIBits(ValueOr<aiBitsType>(aiBits, aiBitsType{}));
		scriptMov->SetColor(ScriptColor(Vector4::One));
		mov.CarriedItem = NO_VALUE;

		// call this when resetting name too?
		dynamic_cast<ObjectsHandler*>(g_GameScriptEntities)->AddMoveableToMap(&mov, scriptMov.get());
		// add to name map too?
	}

	return scriptMov;
}

void Moveable::Register(sol::state& state, sol::table& parent)
{
	// Register type.
	parent.new_usertype<Moveable>(
		ScriptReserved_Moveable,
		sol::call_constructor, Create,
		sol::meta_function::index, IndexError,
		sol::meta_function::new_index, NewIndexError,
		sol::meta_function::equal_to, std::equal_to<const Moveable>(),

		ScriptReserved_GetName, &Moveable::GetName,
		ScriptReserved_GetObjectID, &Moveable::GetObjectID,
		ScriptReserved_GetStatus, &Moveable::GetStatus,
		ScriptReserved_GetPosition, &Moveable::GetPosition,
		ScriptReserved_GetJointPosition, &Moveable::GetJointPos,
		ScriptReserved_GetJointRotation, &Moveable::GetJointRot,
		ScriptReserved_GetRoom, &Moveable::GetRoom,
		ScriptReserved_GetRoomNumber, &Moveable::GetRoomNumber,
		ScriptReserved_GetRotation, &Moveable::GetRotation,
		ScriptReserved_GetScale, &Moveable::GetScale,
		ScriptReserved_GetVelocity, &Moveable::GetVelocity,
		ScriptReserved_GetColor, &Moveable::GetColor,
		ScriptReserved_GetCollidable, &Moveable::GetCollidable,
		ScriptReserved_GetEffect, &Moveable::GetEffect,
		ScriptReserved_GetStateNumber, &Moveable::GetStateNumber,
		ScriptReserved_GetTargetStateNumber, &Moveable::GetTargetStateNumber,
		ScriptReserved_GetAnimNumber, &Moveable::GetAnimNumber,
		ScriptReserved_GetAnimSlot, &Moveable::GetAnimSlot,
		ScriptReserved_GetFrameNumber, &Moveable::GetFrameNumber,
		ScriptReserved_GetEndFrame, &Moveable::GetEndFrame,
		ScriptReserved_GetHP, &Moveable::GetHP,
		ScriptReserved_GetSlotHP, &Moveable::GetSlotHP,
		ScriptReserved_GetOCB, &Moveable::GetOcb,
		ScriptReserved_GetItemFlags, &Moveable::GetItemFlags,
		ScriptReserved_GetLocationAI, &Moveable::GetLocationAI,
		ScriptReserved_GetAIBits, &Moveable::GetAIBits,
		ScriptReserved_GetMeshCount, &Moveable::GetMeshCount,
		ScriptReserved_GetMeshVisible, &Moveable::GetMeshVisible,
		ScriptReserved_GetMeshSwapped, &Moveable::GetMeshSwapped,
		ScriptReserved_GetHitStatus, &Moveable::GetHitStatus,
		ScriptReserved_GetActive, &Moveable::GetActive,
		ScriptReserved_GetValid, &Moveable::GetValid,
		
		ScriptReserved_SetName, &Moveable::SetName,
		ScriptReserved_SetObjectID, &Moveable::SetObjectID,
		ScriptReserved_SetPosition, &Moveable::SetPosition,
		ScriptReserved_SetScale, &Moveable::SetScale,
		ScriptReserved_SetRoomNumber, &Moveable::SetRoomNumber,
		ScriptReserved_SetRotation, &Moveable::SetRotation,
		ScriptReserved_SetColor, &Moveable::SetColor,
		ScriptReserved_SetVisible, &Moveable::SetVisible,
		ScriptReserved_SetCollidable, &Moveable::SetCollidable,
		ScriptReserved_SetEffect, &Moveable::SetEffect,
		ScriptReserved_SetCustomEffect, &Moveable::SetCustomEffect,
		ScriptReserved_SetStatus, &Moveable::SetStatus,
		ScriptReserved_SetStateNumber, &Moveable::SetStateNumber,
		ScriptReserved_SetAnimNumber, &Moveable::SetAnimNumber,
		ScriptReserved_SetFrameNumber, &Moveable::SetFrameNumber,
		ScriptReserved_SetVelocity, &Moveable::SetVelocity,
		ScriptReserved_SetHP, &Moveable::SetHP,
		ScriptReserved_SetOCB, &Moveable::SetOcb,
		ScriptReserved_SetItemFlags, &Moveable::SetItemFlags,
		ScriptReserved_SetLocationAI, &Moveable::SetLocationAI,
		ScriptReserved_SetMeshVisible, &Moveable::SetMeshVisible,
		ScriptReserved_SetAIBits, &Moveable::SetAIBits,
		ScriptReserved_SetOnHit, &Moveable::SetOnHit,
		ScriptReserved_SetOnKilled, &Moveable::SetOnKilled,
		ScriptReserved_SetOnCollidedWithRoom, &Moveable::SetOnCollidedWithRoom,
		ScriptReserved_SetOnCollidedWithObject, &Moveable::SetOnCollidedWithObject,

		ScriptReserved_Enable, &Moveable::EnableItem,
		ScriptReserved_Disable, &Moveable::DisableItem,
		ScriptReserved_Explode, &Moveable::Explode,
		ScriptReserved_Shatter, &Moveable::Shatter,
		ScriptReserved_MakeInvisible, &Moveable::MakeInvisible,
		ScriptReserved_ShatterMesh, &Moveable::ShatterMesh,
		ScriptReserved_SwapMesh, &Moveable::SwapMesh,
		ScriptReserved_UnswapMesh, &Moveable::UnswapMesh,
		ScriptReserved_Destroy, &Moveable::Destroy,
		ScriptReserved_AttachObjCamera, &Moveable::AttachObjCamera,
		ScriptReserved_AnimFromObject, &Moveable::AnimFromObject);
}

Moveable::Moveable(int movID, bool alreadyInitialized)
{
	_moveable = &g_Level.Items[movID];
	_moveableID = movID;
	_initialized = alreadyInitialized;

	if (alreadyInitialized)
		dynamic_cast<ObjectsHandler*>(g_GameScriptEntities)->AddMoveableToMap(_moveable, this);
};

Moveable::Moveable(Moveable&& other) noexcept : 
	_moveable{ std::exchange(other._moveable, nullptr) },
	_moveableID{ std::exchange(other._moveableID, NO_VALUE) },
	_initialized{ std::exchange(other._initialized, false) }
{
	if (GetValid())
	{
		dynamic_cast<ObjectsHandler*>(g_GameScriptEntities)->RemoveMoveableFromMap(_moveable, &other);
		dynamic_cast<ObjectsHandler*>(g_GameScriptEntities)->AddMoveableToMap(_moveable, this);
	}
}

Moveable::~Moveable()
{
	if (_moveable && g_GameScriptEntities) 
		dynamic_cast<ObjectsHandler*>(g_GameScriptEntities)->RemoveMoveableFromMap(_moveable, this);
}

bool operator ==(const Moveable& movA, const Moveable& movB)
{
	return movA._moveable == movB._moveable;
}

void Moveable::Initialize()
{
	bool cond = IsPointInRoom(_moveable->Pose.Position, _moveable->RoomNumber);
	auto err = std::string("Position of item \"{}\" does not match its room ID.");

	if (!ScriptAssertF(cond, err, _moveable->Name))
	{
		ScriptWarn("Resetting to room center.");
		auto center = GetRoomCenter(_moveable->RoomNumber);

		// Reset position, but not orientation.
		_moveable->Pose.Position = center;
	}

	InitializeItem(_moveableID);
	_initialized = true;
}

/// Retrieve the object ID
// @function Moveable:GetObjectID
// @treturn Objects.ObjID a number representing the ID of the object.
GAME_OBJECT_ID Moveable::GetObjectID() const
{
	return _moveable->ObjectNumber;
}

/// Change the object's ID. This will literally change the object.
// @function Moveable:SetObjectID
// @tparam Objects.ObjID objectID the new ID 
// @usage
// shiva = TEN.Objects.GetMoveableByName("shiva_60")
// shiva:SetObjectID(TEN.Objects.ObjID.BIGMEDI_ITEM)
void Moveable::SetObjectID(GAME_OBJECT_ID id) 
{
	_moveable->ObjectNumber = id;
	_moveable->ResetModelToDefault();
}

void SetLevelFuncCallback(const TypeOrNil<LevelFunc>& cb, const std::string& callerName, Moveable& mov, std::string& toModify)
{
	if (std::holds_alternative<LevelFunc>(cb))
	{
		toModify = std::get<LevelFunc>(cb).m_funcName;
		dynamic_cast<ObjectsHandler*>(g_GameScriptEntities)->TryAddColliding(mov._moveableID);
	}
	else if (std::holds_alternative<sol::nil_t>(cb))
	{
		toModify = std::string{};
		dynamic_cast<ObjectsHandler*>(g_GameScriptEntities)->TryRemoveColliding(mov._moveableID);
	}
	else
	{
		ScriptAssert(
			false, "Tried giving " + mov._moveable->Name
			+ " a non-LevelFunc object as an arg to "
			+ callerName);
	}
}

int Moveable::GetIndex() const
{
	return _moveableID;
}

/// Set the name of the function to be called when the moveable is shot by Lara.
// Note that this will be triggered twice when shot with both pistols at once. 
// @function Moveable:SetOnHit
// @tparam function callback function in LevelFuncs hierarchy to call when moveable is shot
void Moveable::SetOnHit(const TypeOrNil<LevelFunc>& cb)
{
	SetLevelFuncCallback(cb, ScriptReserved_SetOnHit, *this, _moveable->Callbacks.OnHit);
}

/// Set the name of the function to be called when the moveable is destroyed/killed
// Note that enemy death often occurs at the end of an animation, and not at the exact moment
// the enemy's HP becomes zero.
// @function Moveable:SetOnKilled
// @tparam function callback function in LevelFuncs hierarchy to call when enemy is killed
// @usage
// LevelFuncs.baddyKilled = function(theBaddy) print("You killed a baddy!") end
// baddy:SetOnKilled(LevelFuncs.baddyKilled)
void Moveable::SetOnKilled(const TypeOrNil<LevelFunc>& cb)
{
	SetLevelFuncCallback(cb, ScriptReserved_SetOnKilled, *this, _moveable->Callbacks.OnKilled);
}

/// Set the function to be called when this moveable collides with another moveable
// @function Moveable:SetOnCollidedWithObject
// @tparam function func callback function to be called (must be in LevelFuncs hierarchy). This function can take two arguments; these will store the two @{Moveable}s taking part in the collision.
// @usage
// -- obj1 is the collision moveable
// -- obj2 is the collider moveable
//
// LevelFuncs.objCollided = function(obj1, obj2)
//     print(obj1:GetName() .. " collided with " .. obj2:GetName())
// end
// baddy:SetOnCollidedWithObject(LevelFuncs.objCollided)
void Moveable::SetOnCollidedWithObject(const TypeOrNil<LevelFunc>& cb)
{
	SetLevelFuncCallback(cb, ScriptReserved_SetOnCollidedWithObject, *this, _moveable->Callbacks.OnObjectCollided);
}

/// Set the function called when this moveable collides with room geometry (e.g. a wall or floor). This function can take an argument that holds the @{Moveable} that collided with geometry.
// @function Moveable:SetOnCollidedWithRoom
// @tparam function func callback function to be called (must be in LevelFuncs hierarchy)
// @usage
// LevelFuncs.roomCollided = function(obj)
//     print(obj:GetName() .. " collided with room geometry")
// end
// baddy:SetOnCollidedWithRoom(LevelFuncs.roomCollided)
void Moveable::SetOnCollidedWithRoom(const TypeOrNil<LevelFunc>& cb)
{
	SetLevelFuncCallback(cb, ScriptReserved_SetOnCollidedWithRoom, *this, _moveable->Callbacks.OnRoomCollided);
}

/// Get the moveable's name (its unique string identifier)
// e.g. "door\_back\_room" or "cracked\_greek\_statue"
// This corresponds with the "Lua Name" field in an object's properties in Tomb Editor.
// @function Moveable:GetName
// @treturn string the moveable's name
std::string Moveable::GetName() const
{
	return _moveable->Name;
}

/// Set the moveable's name (its unique string identifier)
// e.g. "door\_back\_room" or "cracked\_greek\_statue"
// It cannot be blank and cannot share a name with any existing object.
// @function Moveable:SetName
// @tparam string name the new moveable's name
// @treturn bool true if we successfully set the name, false otherwise (e.g. if another object has the name already)
bool Moveable::SetName(const std::string& id) 
{
	if (!ScriptAssert(!id.empty(), "Name cannot be blank. Not setting name."))
		return false;

	if (_callbackSetName(id, _moveableID))
	{
		// Remove old name if it exists.
		if (id != _moveable->Name)
		{
			if (!_moveable->Name.empty())
				_callbackRemoveName(_moveable->Name);

			_moveable->Name = id;
		}
	}
	else
	{
		ScriptAssertF(false, "Could not add name {} - does an object with this name already exist?", id);
		TENLog("Name will not be set", LogLevel::Warning, LogConfig::All);

		return false;
	}

	return true;
}

/// Get the moveable's position
// @function Moveable:GetPosition
// @treturn Vec3 a copy of the moveable's position
Vec3 Moveable::GetPosition() const
{
	return Vec3(_moveable->Pose.Position);
}

/// Set the moveable's position
// If you are moving a moveable whose behaviour involves knowledge of room geometry,
// (e.g. a BADDY1, which uses it for pathfinding), then the second argument should
// be true (or omitted, as true is the default). Otherwise, said moveable will not behave correctly.
// @function Moveable:SetPosition
// @tparam Vec3 position the new position of the moveable 
// @bool[opt] updateRoom Will room changes be automatically detected? Set to false if you are using overlapping rooms (default: true)
void Moveable::SetPosition(const Vec3& pos, sol::optional<bool> updateRoom)
{
	constexpr auto BIG_DISTANCE_THRESHOLD = BLOCK(1);

	auto newPos = pos.ToVector3i();
	bool bigDistance = Vector3i::Distance(newPos, _moveable->Pose.Position) > BIG_DISTANCE_THRESHOLD;
	
	_moveable->Pose.Position = newPos;

	bool willUpdate = !updateRoom.has_value() || updateRoom.value();

	if (_initialized && willUpdate)
	{
		bool isRoomUpdated = _moveable->IsLara() ? UpdateLaraRoom(_moveable, pos.y) : UpdateItemRoom(_moveable->Index);

		// In case direct portal room update didn't happen and distance between old and new points is significant, do predictive room update.
		if (!isRoomUpdated && (willUpdate || bigDistance))
		{
			int potentialNewRoom = FindRoomNumber(_moveable->Pose.Position, _moveable->RoomNumber);
			if (potentialNewRoom != _moveable->RoomNumber)
				SetRoomNumber(potentialNewRoom);
		}
	}

	if (_moveable->IsBridge())
		UpdateBridgeItem(*_moveable);

	if (bigDistance)
		_moveable->DisableInterpolation = true;
}

/// Get the moveable's joint position with an optional relative offset.
// @function Moveable:GetJointPosition
// @tparam int jointID Joint ID.
// @tparam[opt] Vec3 offset Offset relative to the joint.
// @treturn Vec3 World position.
Vec3 Moveable::GetJointPos(int jointID, sol::optional<Vec3> offset) const
{
	auto convertedOffset = offset.has_value() ? offset->ToVector3i() : Vector3i::Zero;
	return Vec3(GetJointPosition(_moveable, jointID, convertedOffset));
}

/// Get the object's joint rotation
// @function Moveable:GetJointRotation
// @tparam int index Index of a joint to get rotation.
// @treturn Rotation a calculated copy of the moveable's joint rotation
Rotation Moveable::GetJointRot(int jointIndex) const
{
	auto point1 = GetJointPosition(_moveable, jointIndex);
	auto point2 = GetJointPosition(_moveable, jointIndex, Vector3::Forward * BLOCK(1));

	auto normal = (point1 - point2).ToVector3();
	normal.Normalize();

	auto eulers = EulerAngles(normal);

	return
	{
		TO_DEGREES(eulers.x),
		TO_DEGREES(eulers.y),
		TO_DEGREES(eulers.z)
	};
}

// This does not guarantee that the returned value will be identical
// to a value written in via SetRot - only that the angle measures
// will be mathematically equal
// (e.g. 90 degrees = -270 degrees = 450 degrees)

/// Get the moveable's rotation.
// @function Moveable:GetRotation
// @treturn Rotation A copy of the moveable's rotation.
Rotation Moveable::GetRotation() const
{
	return 
	{
		TO_DEGREES(_moveable->Pose.Orientation.x),
		TO_DEGREES(_moveable->Pose.Orientation.y),
		TO_DEGREES(_moveable->Pose.Orientation.z)
	};
}

/// Get the moveable's visual scale.
// @function Moveable:GetScale
// @treturn Vec3 A copy of the moveable's visual scale.
Vec3 Moveable::GetScale() const
{
	return Vec3(_moveable->Pose.Scale);
}

/// Set the moveable's rotation.
// @function Moveable:SetRotation
// @tparam Rotation rotation The moveable's new rotation
void Moveable::SetRotation(const Rotation& rot)
{
	constexpr auto BIG_ANGLE_THRESHOLD = ANGLE(30.0f);

	auto newRot = rot.ToEulerAngles();
	bool bigRotation = !EulerAngles::Compare(newRot, _moveable->Pose.Orientation, BIG_ANGLE_THRESHOLD);

	_moveable->Pose.Orientation = newRot;

	if (_moveable->IsBridge())
		UpdateBridgeItem(*_moveable);

	if (bigRotation)
		_moveable->DisableInterpolation = true;
}

/// Set the moveable's visual scale. Does not affect collision.
// @function Moveable:SetScale
// @tparam Vec3 scale New visual scale.
void Moveable::SetScale(const Vec3& scale)
{
	_moveable->Pose.Scale = scale.ToVector3();
}

/// Get current HP (hit points/health points)
// @function Moveable:GetHP
// @treturn int the amount of HP the moveable currently has
short Moveable::GetHP() const
{
	return _moveable->HitPoints;
}

/// Set current HP (hit points/health points)
// Clamped to [0, 32767] for "intelligent" entities (i.e. anything with AI); clamped to [-32767, 32767] otherwise.
// @function Moveable:SetHP
// @tparam int HP the amount of HP to give the moveable
void Moveable::SetHP(short hp)
{
	if (Objects[_moveable->ObjectNumber].intelligent && hp < 0)
	{
		if (hp != NOT_TARGETABLE)
		{
			ScriptAssert(false, "Invalid HP value: " + std::to_string(hp));
			ScriptWarn("Setting HP to 0.");
			hp = 0;
		}
	}

	_moveable->HitPoints = hp;
}

/// Get HP definded for that object type (hit points/health points) (Read Only).
// @function Moveable:GetSlotHP
// @treturn int the moveable's slot default hit points
short Moveable::GetSlotHP() const
{
	return Objects[_moveable->ObjectNumber].HitPoints;
}

/// Get OCB (object code bit) of the moveable
// @function Moveable:GetOCB
// @treturn int the moveable's current OCB value
short Moveable::GetOcb() const
{
	return _moveable->TriggerFlags;
}

/// Set OCB (object code bit) of the moveable
// @function Moveable:SetOCB
// @tparam int OCB the new value for the moveable's OCB
void Moveable::SetOcb(short ocb)
{
	_moveable->TriggerFlags = ocb;
}

/// Set the effect for this moveable.
// @function Moveable:SetEffect
// @tparam Effects.EffectID effect Type of effect to assign.
// @tparam[opt] float timeout time (in seconds) after which effect turns off.
void Moveable::SetEffect(EffectType effectType, sol::optional<float> timeout)
{
	int realTimeout = timeout.has_value() ? int(timeout.value() * FPS) : -1;

	switch (effectType)
	{
	case EffectType::None:
		_moveable->Effect.Type = EffectType::None;
		break;

	case EffectType::Smoke:
		ItemSmoke(_moveable, realTimeout);
		break;

	case EffectType::Fire:
		ItemBurn(_moveable, realTimeout);
		break;

	case EffectType::Sparks:
		ItemElectricBurn(_moveable, realTimeout);
		break;

	case EffectType::ElectricIgnite:
		ItemBlueElectricBurn(_moveable, realTimeout);
		break;

	case EffectType::RedIgnite:
		ItemRedLaserBurn(_moveable, realTimeout);
		break;

	case EffectType::Custom:
		ScriptWarn("CUSTOM effect type requires additional setup. Use SetCustomEffect command instead.");
	}
}

/// Set custom colored burn effect to moveable
// @function Moveable:SetCustomEffect
// @tparam Color color1 The primary color of the effect (also used for lighting).
// @tparam Color color2 The secondary color of the effect.
// @tparam[opt] float timeout Time (in seconds) after which effect turns off.
void Moveable::SetCustomEffect(const ScriptColor& col1, const ScriptColor& col2, sol::optional<float> timeout)
{
	int realTimeout = timeout.has_value() ? int(timeout.value() * FPS) : -1;
	auto color1 = Vector3(col1.GetR() * (1.f / 255.f), col1.GetG() * (1.f / 255.f), col1.GetB() * (1.f / 255.f));
	auto color2 = Vector3(col2.GetR() * (1.f / 255.f), col2.GetG() * (1.f / 255.f), col2.GetB() * (1.f / 255.f));
	ItemCustomBurn(_moveable, color1, color2, realTimeout);
}

/// Get current moveable effect
// @function Moveable:GetEffect
// @treturn Effects.EffectID Effect type currently assigned.
EffectType Moveable::GetEffect() const
{
	return _moveable->Effect.Type;
}

/// Get the value stored in ItemFlags[index]
// @function Moveable:GetItemFlags
// @tparam int index Index of the ItemFlag, can be between 0 and 7.
// @treturn int the value contained in the ItemFlags[index]
short Moveable::GetItemFlags(int index) const
{
	return _moveable->ItemFlags[index];
}

/// Stores a value in ItemFlags[index]
// @function Moveable:SetItemFlags
// @tparam short value Value to store in the moveable's ItemFlags[index].
// @tparam int index Index of the ItemFlag where to store the value.
void Moveable::SetItemFlags(short value, int index)
{
	_moveable->ItemFlags[index] = value;
}

/// Get the location value stored in the Enemy AI
// @function Moveable:GetLocationAI
// @treturn short the value contained in the LocationAI of the creature.
short Moveable::GetLocationAI() const
{
	if (_moveable->IsCreature())
	{
		auto creature = (CreatureInfo*)_moveable->Data;
		return creature->LocationAI;
	}

	TENLog("Trying to get LocationAI value from non-creature moveable. Value does not exist so it's returning 0.", LogLevel::Error);
	return 0;
}

/// Updates the location in the enemy AI with the given value.
// @function Moveable:SetLocationAI
// @tparam short value Value to store.
void Moveable::SetLocationAI(short value)
{
	if (_moveable->IsCreature())
	{
		auto creature = (CreatureInfo*)_moveable->Data;
		creature->LocationAI = value;
	}
	else
	{
		TENLog("Trying to set a value in nonexisting variable. Non creature moveable hasn't got LocationAI.", LogLevel::Error);
	}
}

/// Get the moveable's color
// @function Moveable:GetColor
// @treturn Color a copy of the moveable's color
ScriptColor Moveable::GetColor() const
{
	return ScriptColor{ _moveable->Model.Color };
}

/// Set the moveable's color
// @function Moveable:SetColor
// @tparam Color color the new color of the moveable 
void Moveable::SetColor(const ScriptColor& color)
{
	_moveable->Model.Color = color;
}

/// Get AIBits of object
// This will return a table with six values, each corresponding to
// an active behaviour. If the object is in a certain AI mode, the table will
// have a *1* in the corresponding cell. Otherwise, the cell will hold
// a *0*.
//
//	1 - Guard
//	2 - Ambush
//	3 - Patrol 1
//	4 - Modify
//	5 - Follow
//	6 - Patrol 2
//
// @function Moveable:GetAIBits
// @treturn table a table of AI bits
aiBitsType Moveable::GetAIBits() const
{
	static_assert(63 == ALL_AIOBJ);

	aiBitsArray ret{};
	for (size_t i = 0; i < ret.size(); ++i)
	{
		unsigned char isSet = _moveable->AIBits & (1 << i);
		ret[i] = static_cast<int>( isSet > 0);
	}

	return ret;
}

/// Set AIBits of object
// Use this to force a moveable into a certain AI mode or modes, as if a certain nullmesh
// (or more than one) had suddenly spawned beneath their feet.
// @function Moveable:SetAIBits
// @tparam table bits the table of AI bits
// @usage 
// local sas = TEN.Objects.GetMoveableByName("sas_enemy")
// sas:SetAIBits({1, 0, 0, 0, 0, 0})
void Moveable::SetAIBits(aiBitsType const & bits)
{
	for (size_t i = 0; i < bits.value().size(); ++i)
	{
		_moveable->AIBits &= ~(1 << i);
		unsigned char isSet = bits.value()[i] > 0;
		_moveable->AIBits |= isSet << i;
	}
}

/// Retrieve the index of the current state.
// This corresponds to the number shown in the item's state ID field in WadTool.
// @function Moveable:GetState
// @treturn int the index of the active state
int Moveable::GetStateNumber() const
{
	return _moveable->Animation.ActiveState;
}

/// Retrieve the index of the target state.
// This corresponds to the state the object is trying to get into, which is sometimes different from the active state.
// @function Moveable:GetTargetState
// @treturn int the index of the target state
int Moveable::GetTargetStateNumber() const
{
	return _moveable->Animation.TargetState;
}

/// Set the object's state to the one specified by the given index.
// Performs no bounds checking. *Ensure the number given is correct, else
// object may end up in corrupted animation state.*
// @function Moveable:SetState
// @tparam int index the index of the desired state 
void Moveable::SetStateNumber(int stateNumber)
{
	_moveable->Animation.TargetState = stateNumber;
}

/// Retrieve the slot ID of the animation.
// In certain cases, moveable may play animations from another object slot. Use this
// function when you need to identify such cases.
// @function Moveable:GetAnimSlot
// @treturn int animation slot ID
int Moveable::GetAnimSlot() const
{
	return _moveable->Animation.AnimObjectID;
}

/// Retrieve the index of the current animation.
// This corresponds to the number shown in the item's animation list in WadTool.
// @function Moveable:GetAnim
// @treturn int the index of the active animation
int Moveable::GetAnimNumber() const
{
	return _moveable->Animation.AnimNumber;
}

/// Set the object's animation to the one specified by the given index.
// Performs no bounds checking. *Ensure the number given is correct, else
// object may end up in corrupted animation state.*
// @function Moveable:SetAnim
// @tparam int index the index of the desired anim 
// @tparam[opt] int slot slot ID of the desired anim (if omitted, moveable's own slot ID is used)
void Moveable::SetAnimNumber(int animNumber, sol::optional<int> slotIndex)
{
	SetAnimation(*_moveable, animNumber);
}

/// Retrieve frame number.
// This is the current frame of the object's active animation.
// @function Moveable:GetFrame
// @treturn int the current frame of the active animation
int Moveable::GetFrameNumber() const
{
	return _moveable->Animation.FrameNumber;
}

/// Get the object's velocity.
// In most cases, only Z and Y components are used as forward and vertical velocity.
// In some cases, primarily NPCs, X component is used as side velocity.
// @function Moveable:GetVelocity
// @treturn Vec3 current object velocity
Vec3 Moveable::GetVelocity() const
{
	return Vec3(
		(int)round(_moveable->Animation.Velocity.x),
		(int)round(_moveable->Animation.Velocity.y),
		(int)round(_moveable->Animation.Velocity.z));
}

/// Set the object's velocity to specified value.
// In most cases, only Z and Y components are used as forward and vertical velocity.
// In some cases, primarily NPCs, X component is used as side velocity.
// @function Moveable:SetVelocity
// @tparam Vec3 velocity velocity represented as vector 
void Moveable::SetVelocity(Vec3 velocity)
{
	if (_moveable->IsCreature())
		ScriptWarn("Attempt to set velocity to a creature. In may not work, as velocity is overridden by AI.");

	_moveable->Animation.Velocity = Vector3(velocity.x, velocity.y, velocity.z);
}

/// Set frame number.
// This will move the animation to the given frame.
// The number of frames in an animation can be seen under the heading "End frame" in
// the WadTool animation editor. If the animation has no frames, the only valid argument
// is -1.
// @function Moveable:SetFrame
// @tparam int frame the new frame number
void Moveable::SetFrameNumber(int frameNumber)
{
	const auto& anim = GetAnimData(*_moveable);

	unsigned int endFrameNumber = anim.EndFrameNumber;
	
	bool cond = (frameNumber < endFrameNumber);
	const char* err = "Invalid frame number {}; max frame number for anim {} is {}.";

	if (ScriptAssertF(cond, err, frameNumber, _moveable->Animation.AnimNumber, endFrameNumber - 1))
	{
		_moveable->Animation.FrameNumber = frameNumber;
	}
	else
	{
		ScriptWarn("Not setting frame number.");
	}
}

/// Get the end frame number of the moveable's active animation.
// This is the "End Frame" set in WADTool for the animation.
// @function Moveable:GetEndFrame()
// @treturn int End frame number of the active animation.
int Moveable::GetEndFrame() const
{
	const auto& anim = GetAnimData(*_moveable);
	return anim.EndFrameNumber;
}

/// Determine whether the moveable is active or not 
// @function Moveable:GetActive
// @treturn bool true if the moveable is active
bool Moveable::GetActive() const
{
	return _moveable->Active;
}

void Moveable::SetActive(bool isActive)
{
	_moveable->Active = isActive;
}

/// Get the hit status of the moveable.
// @function Moveable:GetHitStatus
// @treturn bool true if the moveable was hit by something in the last gameplay frame, false otherwise 
bool Moveable::GetHitStatus() const
{
	return _moveable->HitStatus;
}

/// Get the current room of the object
// @function Moveable:GetRoom
// @treturn Objects.Room current room of the object
std::unique_ptr<Room> Moveable::GetRoom() const
{
	return std::make_unique<Room>(g_Level.Rooms[_moveable->RoomNumber]);
}

/// Get the current room number of the object
// @function Moveable:GetRoomNumber
// @treturn int number representing the current room of the object
int Moveable::GetRoomNumber() const
{
	return _moveable->RoomNumber;
}

/// Set the room ID of a moveable.
// Use this if not using SetPosition's automatic room update - for example, when dealing with overlapping rooms.
// @function Moveable:SetRoomNumber
// @tparam int roomID New room's ID.
// @usage 
// local sas = TEN.Objects.GetMoveableByName("sas_enemy")
// sas:SetRoomNumber(newRoomID)
// sas:SetPosition(newPos, false)
void Moveable::SetRoomNumber(int roomNumber)
{	
	int roomCount = (int)g_Level.Rooms.size();
	if (roomNumber < 0 || roomNumber >= roomCount)
	{
		ScriptAssertF(false, "Invalid room ID {}. Value must be in range [0, {})", roomNumber, roomCount);
		TENLog("Room ID will not be set.", LogLevel::Warning, LogConfig::All);
		return;
	}

	if (!_initialized)
	{
		_moveable->RoomNumber = roomNumber;
	}
	else
	{
		ItemNewRoom(_moveableID, roomNumber);

		// HACK: Must manually force new Location.RoomNumber for player, otherwise camera doesn't update properly.
		if (_moveable->IsLara())
			_moveable->Location.RoomNumber = roomNumber;
	}
}

/// Get the moveable's status.
// @function Moveable:GetStatus
// @treturn Objects.MoveableStatus Status.
short Moveable::GetStatus() const
{
	return _moveable->Status;
}

/// Set the moveable's status.
// @function Moveable:SetStatus
// @tparam Objects.MoveableStatus status The new status of the moveable.
void Moveable::SetStatus(ItemStatus status)
{
	_moveable->Status = status;
}

/// Get number of meshes for a particular object
// Returns number of meshes in an object
// @function Moveable:GetMeshCount
// @treturn int number of meshes
short Moveable::GetMeshCount() const
{
	return Objects[_moveable->ObjectNumber].nmeshes;
}

/// Get state of specified mesh visibility of object
// Returns true if specified mesh is visible on an object, and false
// if it is not visible.
// @function Moveable:GetMeshVisible
// @int index index of a mesh
// @treturn bool visibility status
bool Moveable::GetMeshVisible(int meshId) const
{
	if (!MeshExists(meshId))
		return false;

	return _moveable->MeshBits.Test(meshId);
}

/// Makes specified mesh visible or invisible
// Use this to show or hide a specified mesh of an object.
// @function Moveable:SetMeshVisible
// @int index index of a mesh
// @bool isVisible true if you want the mesh to be visible, false otherwise
void Moveable::SetMeshVisible(int meshId, bool isVisible)
{
	if (!MeshExists(meshId))
		return;

	if (isVisible)
	{
		_moveable->MeshBits.Set(meshId);
	}
	else
	{
		_moveable->MeshBits.Clear(meshId);
	}
}

/// Shatters specified mesh and makes it invisible
// Note that you can re-enable mesh later by using SetMeshVisible().
// @function Moveable:ShatterMesh
// @int index index of a mesh
void Moveable::ShatterMesh(int meshId)
{
	if (!MeshExists(meshId))
		return;

	ExplodeItemNode(_moveable, meshId, 0, 128);
}

/// Get state of specified mesh swap of object
// Returns true if specified mesh is swapped on an object, and false
// if it is not swapped.
// @function Moveable:GetMeshSwapped
// @int index index of a mesh
// @treturn bool mesh swap status
bool Moveable::GetMeshSwapped(int meshId) const
{
	if (!MeshExists(meshId))
		return false;

	return _moveable->Model.MeshIndex[meshId] == _moveable->Model.BaseMesh + meshId;
}

/// Set state of specified mesh swap of object
// Use this to swap specified mesh of an object.
// @function Moveable:SwapMesh
// @int index index of a mesh
// @int slotIndex index of a slot to get meshswap from
// @int[opt] swapIndex index of a mesh from meshswap slot to use
void Moveable::SwapMesh(int meshId, int swapSlotId, sol::optional<int> swapMeshIndex)
{
	if (!MeshExists(meshId))
		return;

	if (!swapMeshIndex.has_value())
		 swapMeshIndex = meshId;

	if (swapSlotId <= -1 || swapSlotId >= ID_NUMBER_OBJECTS)
	{
		TENLog("Specified meshswap slot ID is incorrect!", LogLevel::Error);
		return;
	}

	if (!Objects[swapSlotId].loaded)
	{
		TENLog("Object in specified meshswap slot doesn't exist in level!", LogLevel::Error);
		return;
	}

	if (swapMeshIndex.value() >= Objects[swapSlotId].nmeshes)
	{
		TENLog("Specified meshswap index does not exist in meshswap slot!", LogLevel::Error);
		return;
	}

	_moveable->Model.MeshIndex[meshId] = Objects[swapSlotId].meshIndex + swapMeshIndex.value();
}

/// Set state of specified mesh swap of object
// Use this to bring back original unswapped mesh
// @function Moveable:UnswapMesh
// @int index index of a mesh to unswap
void Moveable::UnswapMesh(int meshId)
{
	if (!MeshExists(meshId))
		return;

	_moveable->Model.MeshIndex[meshId] = _moveable->Model.BaseMesh + meshId;
}

/// Enable the item, as if a trigger for it had been stepped on.
// @function Moveable:Enable
// @tparam float timeout time (in seconds) after which moveable automatically disables (optional).
void Moveable::EnableItem(sol::optional<float> timer)
{
	if (_moveableID == NO_VALUE)
		return;

	bool wasInvisible = false;
	if (_moveable->Status == ITEM_INVISIBLE)
		wasInvisible = true;

	_moveable->Flags |= CODE_BITS;
	_moveable->Timer = timer.has_value() ? (timer.value() * FPS) : 0;
	Trigger(_moveableID);

	// Try add colliding in case the item went from invisible -> activated
	if (wasInvisible)
		dynamic_cast<ObjectsHandler*>(g_GameScriptEntities)->TryAddColliding(_moveableID);
}

/// Disable the item, as if an antitrigger for it had been stepped on (i.e. it will close an open door or extinguish a flame emitter).
// Note that this will not trigger an OnKilled callback.
// @function Moveable:Disable
void Moveable::DisableItem()
{
	if (_moveableID == NO_VALUE)
		return;

	Antitrigger(_moveableID);

	if (_moveableID > NO_VALUE && (_moveable->Status == ITEM_INVISIBLE))
		dynamic_cast<ObjectsHandler*>(g_GameScriptEntities)->TryRemoveColliding(_moveableID);
}

/// Explode this moveable. Also kills and disables it.
// @function Moveable:Explode
void Moveable::Explode()
{
	if (!_moveable->Active && !_moveable->IsLara())
		return;

	CreatureDie(_moveableID, true);
}

/// Shatter this moveable. Also kills and disables it.
// @function Moveable:Shatter
void Moveable::Shatter()
{
	if (!_moveable->Active && !_moveable->IsLara())
		return;

	_moveable->Flags |= IFLAG_KILLED | IFLAG_INVISIBLE;
	for (int i = 0; i < Objects[_moveable->ObjectNumber].nmeshes; i++)
		ExplodeItemNode(_moveable, i, 0, 128);

	CreatureDie(_moveableID, false);
	KillItem(_moveableID);
}

/// Get the item's collision state.
// @treturn bool item's collision state
// @function Moveable:GetCollidable
bool Moveable::GetCollidable()
{
	return _moveable->Collidable;
}

/// Set the item's collision.
// @bool collidable true if the caller should be collidable, false if no collision should occur.
// @function Moveable:SetCollidable
void Moveable::SetCollidable(bool isCollidable)
{
	_moveable->Collidable = isCollidable;
}

/// Make the item invisible. Alias for `Moveable:SetVisible(false)`.
// @function Moveable:MakeInvisible
void Moveable::MakeInvisible()
{
	SetVisible(false);
}
/// Set the item's visibility. __An invisible item will have collision turned off, as if it no longer exists in the game world__.
// @bool visible true if the caller should become visible, false if it should become invisible
// @function Moveable:SetVisible
void Moveable::SetVisible(bool isVisible)
{
	if (!isVisible)
	{
		if (Objects[_moveable->ObjectNumber].intelligent)
		{
			DisableItem();
		}
		else
		{
			RemoveActiveItem(_moveableID, false);
		}

		_moveable->Status = ITEM_INVISIBLE;

		if (_moveableID > NO_VALUE)
			dynamic_cast<ObjectsHandler*>(g_GameScriptEntities)->TryRemoveColliding(_moveableID);
	}
	else
	{
		if (Objects[_moveable->ObjectNumber].intelligent)
		{
			if(!(_moveable->Flags & IFLAG_KILLED))
			{
				EnableItem(sol::nullopt);
			}
			else
			{
				_moveable->Status = ITEM_ACTIVE;
			}
		}
		else
		{
			AddActiveItem(_moveableID);
			_moveable->Status = ITEM_ACTIVE;
		}

		if (_moveableID > NO_VALUE)
			dynamic_cast<ObjectsHandler*>(g_GameScriptEntities)->TryAddColliding(_moveableID);
	}
}

void Moveable::Invalidate()
{
	// Keep m_item as-is so it can be properly removed from moveables set when destructor is called.
	_moveableID = NO_VALUE;
	_initialized = false;
}

/// Test if the object is in a valid state (i.e. has not been destroyed through Lua or killed by Lara).
// @function Moveable:GetValid
// @treturn bool valid true if the object is still not destroyed
bool Moveable::GetValid() const
{
	return _moveableID > NO_VALUE;
}

/// Destroy the moveable. This will mean it can no longer be used, except to re-initialize it with another object.
// @function Moveable:Destroy
void Moveable::Destroy()
{
	if (_moveableID > NO_VALUE)
	{
		dynamic_cast<ObjectsHandler*>(g_GameScriptEntities)->RemoveMoveableFromMap(_moveable, this);
		_callbackRemoveName(_moveable->Name);
		KillItem(_moveableID);
	}

	Invalidate();
}

bool Moveable::MeshExists(int index) const
{
	if (index < 0 || index >= Objects[_moveable->ObjectNumber].nmeshes)
	{
		ScriptAssertF(false, "Mesh index {} does not exist in moveable '{}'", index, _moveable->Name);
		return false;
	}

	return true;
}

/// Attach camera to an object.
// @function Moveable:AttachObjCamera
// @tparam int mesh Mesh of a moveable to use as a camera position.
// @tparam Moveable target Target moveable to attach camera to.
// @tparam int mesh Mesh of a target moveable to use as a camera target.
void Moveable::AttachObjCamera(short camMeshId, Moveable& mov, short targetMeshId)
{
	if ((_moveable->Active || _moveable->IsLara()) && (mov._moveable->Active || mov._moveable->IsLara()))
		ObjCamera(_moveable, camMeshId, mov._moveable, targetMeshId, true);
}

/// Borrow animation from an object
// @function Moveable:AnimFromObject
// @tparam Objects.ObjID objectID Object ID to take animation and stateID from.
// @tparam int animNumber Animation from object.
// @tparam int stateID state State from object.
void Moveable::AnimFromObject(GAME_OBJECT_ID objectID, int animNumber, int stateID)
{
	_moveable->Animation.AnimObjectID = objectID;
	_moveable->Animation.AnimNumber = animNumber;
	_moveable->Animation.ActiveState = stateID;
	_moveable->Animation.FrameNumber = 0;
	AnimateItem(*_moveable);
}
