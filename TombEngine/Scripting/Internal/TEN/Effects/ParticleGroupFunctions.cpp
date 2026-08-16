#include "framework.h"
#include "Scripting/Internal/TEN/Effects/ParticleGroupFunctions.h"

#include "Game/effects/ParticleGroup.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Objects/Utils/object_helper.h"
#include "Scripting/Internal/LuaHandler.h"
#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/TEN/Effects/BlendIDs.h"
#include "Scripting/Internal/TEN/Types/Color/Color.h"
#include "Scripting/Internal/TEN/Types/Rotation/Rotation.h"
#include "Scripting/Internal/TEN/Types/Vec3/Vec3.h"
#include "Specific/level.h"

using namespace TEN::Effects::ParticleGroups;
using namespace TEN::Math;
using namespace TEN::Scripting::Types;

/// Create and manage particle groups.
// @tenclass Effects.ParticleGroups 
// @pragma nostrip

namespace ParticleKey
{
	constexpr const char* Id = "id";
	constexpr const char* Position = "position";
	constexpr const char* Velocity = "velocity";
	constexpr const char* Acceleration = "acceleration";
	constexpr const char* Size = "size";
	constexpr const char* Rotation = "rotation";
	constexpr const char* Color = "color";
	constexpr const char* Age = "age";
	constexpr const char* Lifetime = "lifetime";
	constexpr const char* AgeNormalized = "ageNormalized";
	constexpr const char* SubIndex = "subIndex";
	constexpr const char* ObjectID = "objectID";
	constexpr const char* Orientation = "orientation";
	constexpr const char* ContactRadius = "contactRadius";
	constexpr const char* TouchingPlayer = "touchingPlayer";
	constexpr const char* Teleport = "teleport";
}

namespace TEN::Scripting::Effects::ParticleGroups
{
	// Private static helpers

	sol::table LuaParticleGroup::MakeParticleTable(sol::state_view& lua, const GroupParticle& p)
	{
		auto tbl = lua.create_table();
		tbl[ParticleKey::Id] = p.ID;
		tbl[ParticleKey::Position] = Vec3(p.Position);
		tbl[ParticleKey::Velocity] = Vec3(p.Velocity);
		tbl[ParticleKey::Acceleration] = Vec3(p.Acceleration);
		tbl[ParticleKey::Size] = p.Size;
		tbl[ParticleKey::Rotation] = p.Rotation / RADIAN;
		tbl[ParticleKey::Color] = ScriptColor(p.ParticleColor);
		tbl[ParticleKey::Age] = p.Age;
		tbl[ParticleKey::Lifetime] = p.Lifetime;
		tbl[ParticleKey::AgeNormalized] = p.AgeNormalized;
		tbl[ParticleKey::SubIndex] = p.SubIndex;
		tbl[ParticleKey::ObjectID] = p.ObjectID;
		tbl[ParticleKey::Orientation] = Rotation(p.Orientation.x / RADIAN, p.Orientation.y / RADIAN, p.Orientation.z / RADIAN);
		tbl[ParticleKey::ContactRadius] = p.ContactRadius;
		tbl[ParticleKey::TouchingPlayer] = p.TouchingPlayer;
		return tbl;
	}

	void LuaParticleGroup::ApplyParticleTable(GroupParticle& p, const sol::table& data)
	{
		if (auto pos = data.get<sol::optional<Vec3>>(ParticleKey::Position))
			p.Position = pos->ToVector3();
		if (auto vel = data.get<sol::optional<Vec3>>(ParticleKey::Velocity))
			p.Velocity = vel->ToVector3();
		if (auto accel = data.get<sol::optional<Vec3>>(ParticleKey::Acceleration))
			p.Acceleration = accel->ToVector3();
		if (auto size = data.get<sol::optional<float>>(ParticleKey::Size))
			p.Size = *size;
		if (auto rot = data.get<sol::optional<float>>(ParticleKey::Rotation))
			p.Rotation = *rot * RADIAN;
		if (auto sprite = data.get<sol::optional<int>>(ParticleKey::SubIndex))
			p.SubIndex = *sprite;
		if (auto seq = data.get<sol::optional<GAME_OBJECT_ID>>(ParticleKey::ObjectID))
			p.ObjectID = *seq;
		if (auto color = data.get<sol::optional<ScriptColor>>(ParticleKey::Color))
			p.ParticleColor = Color(*color);
		if (auto orient = data.get<sol::optional<Rotation>>(ParticleKey::Orientation))
			p.Orientation = Vector3(orient->x * RADIAN, orient->y * RADIAN, orient->z * RADIAN);
		if (auto lt = data.get<sol::optional<float>>(ParticleKey::Lifetime))
			p.Lifetime = std::max(0.01f, *lt);
		if (auto age = data.get<sol::optional<float>>(ParticleKey::Age))
			p.Age = std::clamp(*age, 0.0f, p.Lifetime);
		if (auto cr = data.get<sol::optional<float>>(ParticleKey::ContactRadius))
			p.ContactRadius = std::max(1.0f, *cr);
		if (auto tp = data.get<sol::optional<bool>>(ParticleKey::Teleport); tp && *tp)
			p.Teleport = true;
	}

	// Constructor

	LuaParticleGroup::LuaParticleGroup(GAME_OBJECT_ID objectID, int maxParticles)
	{
		if (!CheckIfSlotExists(objectID, "ParticleGroup"))
			return;

		int id = CreateParticleGroup(objectID, maxParticles);
		if (id < 0)
			return;

		_handle = ParticleGroupHandle{ id, ParticleGroupList[id].Generation };
	}

	// Emission control

	void LuaParticleGroup::Start()
	{
		if (auto* group = _handle.Get()) group->Start();
	}

	void LuaParticleGroup::Stop()
	{
		if (auto* group = _handle.Get()) group->Stop();
	}

	void LuaParticleGroup::Pause()
	{
		if (auto* group = _handle.Get()) group->Pause();
	}

	void LuaParticleGroup::Resume()
	{
		if (auto* group = _handle.Get()) group->Resume();
	}

	void LuaParticleGroup::EmitBurst(int count)
	{
		if (auto* group = _handle.Get()) group->EmitBurst(count);
	}

	// Queries

	int LuaParticleGroup::GetActiveCount() const
	{
		const auto* group = _handle.Get();
		return group ? group->GetActiveCount() : 0;
	}

	bool LuaParticleGroup::IsMeshGroup() const
	{
		const auto* group = _handle.Get();
		return group ? group->IsMeshGroup() : false;
	}

	int LuaParticleGroup::GetID() const
	{
		const auto* group = _handle.Get();
		return group ? group->ID : -1;
	}

	bool LuaParticleGroup::IsActive() const
	{
		return _handle.IsValid();
	}

	// Emitter

	void LuaParticleGroup::SetEmissionRate(float rate)
	{
		if (auto* group = _handle.Get()) group->EmissionRate = std::max(0.0f, rate);
	}

	void LuaParticleGroup::SetPosition(const Vec3& pos)
	{
		auto* group = _handle.Get();
		if (!group)
			return;

		group->EmitterPosition = pos.ToVector3();
		group->RoomNumber = FindRoomNumber(Vector3i((int)pos.x, (int)pos.y, (int)pos.z));
	}

	sol::optional<Vec3> LuaParticleGroup::GetPosition() const
	{
		const auto* group = _handle.Get();
		if (!group)
			return sol::nullopt;

		return Vec3(group->EmitterPosition);
	}

	// Initial particle properties

	void LuaParticleGroup::SetInitialVelocity(const Vec3& vel)
	{
		if (auto* group = _handle.Get()) group->InitVelocity = vel.ToVector3();
	}

	void LuaParticleGroup::SetInitialVelocityRandom(const Vec3& range)
	{
		if (auto* group = _handle.Get()) group->InitVelocityRandom = range.ToVector3();
	}

	void LuaParticleGroup::SetInitialAcceleration(const Vec3& accel)
	{
		if (auto* group = _handle.Get()) group->InitAcceleration = accel.ToVector3();
	}

	void LuaParticleGroup::SetLifetime(float seconds)
	{
		auto* group = _handle.Get();
		if (!group)
			return;

		seconds = std::max(0.01f, seconds);
		group->LifetimeMin = seconds;
		group->LifetimeMax = seconds;
	}

	void LuaParticleGroup::SetLifetimeRange(float minSec, float maxSec)
	{
		auto* group = _handle.Get();
		if (!group)
			return;

		group->LifetimeMin = std::max(0.01f, minSec);
		group->LifetimeMax = std::max(group->LifetimeMin, maxSec);
	}

	void LuaParticleGroup::SetInitialSize(float size)
	{
		auto* group = _handle.Get();
		if (!group)
			return;

		group->InitSizeMin = size;
		group->InitSizeMax = size;
	}

	void LuaParticleGroup::SetInitialSizeRange(float minSize, float maxSize)
	{
		auto* group = _handle.Get();
		if (!group)
			return;

		group->InitSizeMin = minSize;
		group->InitSizeMax = std::max(minSize, maxSize);
	}

	void LuaParticleGroup::SetInitialColor(const ScriptColor& color)
	{
		auto* group = _handle.Get();
		if (!group)
			return;

		group->InitColorMin = Color(color);
		group->InitColorMax = Color(color);
	}

	void LuaParticleGroup::SetInitialColorRange(const ScriptColor& minColor, const ScriptColor& maxColor)
	{
		auto* group = _handle.Get();
		if (!group)
			return;

		group->InitColorMin = Color(minColor);
		group->InitColorMax = Color(maxColor);
	}

	void LuaParticleGroup::SetInitialRotation(float rotation)
	{
		if (auto* group = _handle.Get()) group->InitRotation = rotation * RADIAN;
	}

	void LuaParticleGroup::SetInitialRotationVelocity(float rotVel)
	{
		if (auto* group = _handle.Get()) group->InitRotationVel = rotVel * RADIAN;
	}

	void LuaParticleGroup::SetBlendMode(BlendMode mode)
	{
		if (auto* group = _handle.Get()) group->RenderBlendMode = mode;
	}

	void LuaParticleGroup::SetObjectID(GAME_OBJECT_ID objectID)
	{
		auto* group = _handle.Get();
		if (!group)
			return;

		if (CheckIfSlotExists(objectID, "ParticleGroup:SetObjectID"))
			group->ObjectID = objectID;
	}

	void LuaParticleGroup::SetSubIndex(int index)
	{
		auto* group = _handle.Get();
		if (!group)
			return;

		group->InitSubIndex = index;
	}

	sol::optional<int> LuaParticleGroup::GetSubIndex() const
	{
		const auto* group = _handle.Get();
		if (!group)
			return sol::nullopt;

		return group->InitSubIndex;
	}

	sol::optional<GAME_OBJECT_ID> LuaParticleGroup::GetObjectID() const
	{
		const auto* group = _handle.Get();
		if (!group)
			return sol::nullopt;

		return group->ObjectID;
	}

	void LuaParticleGroup::SetInitialOrientation(const Rotation& orient)
	{
		if (auto* group = _handle.Get())
		{
			group->InitOrientation = Vector3(
				orient.x * RADIAN, orient.y * RADIAN, orient.z * RADIAN);
		}
	}

	// Contact

	void LuaParticleGroup::SetContactRadius(float radius)
	{
		if (auto* group = _handle.Get()) group->InitContactRadius = std::max(1.0f, radius);
	}

	// Per-particle access

	sol::object LuaParticleGroup::GetParticle(int index, sol::this_state s) const
	{
		const auto* group = _handle.Get();
		if (!group)
			return sol::nil;

		if (index < 0 || index >= (int)group->Particles.size() || !group->Particles[index].Active)
			return sol::nil;

		sol::state_view lua(s);
		return MakeParticleTable(lua, group->Particles[index]);
	}

	void LuaParticleGroup::SetParticle(int index, sol::table data)
	{
		auto* group = _handle.Get();
		if (!group || group->State == ParticleGroupState::Paused)
			return;

		if (index < 0 || index >= (int)group->Particles.size() || !group->Particles[index].Active)
			return;

		ApplyParticleTable(group->Particles[index], data);
	}

	void LuaParticleGroup::ForEachParticle(sol::function callback, sol::this_state s)
	{
		auto* group = _handle.Get();
		if (!group || group->State == ParticleGroupState::Paused)
			return;

		sol::state_view lua(s);
		for (int i = 0; i < (int)group->Particles.size(); i++)
		{
			if (!group->Particles[i].Active)
				continue;

			auto tbl    = MakeParticleTable(lua, group->Particles[i]);
			auto result = callback(i, tbl);

			// If the callback returns a table, apply changes back to the particle.
			if (result.valid() && result.get_type() == sol::type::table)
				ApplyParticleTable(group->Particles[i], result);
		}
	}

	// Registration

	void LuaParticleGroup::Register(sol::table& parent)
	{
		using ctors = sol::constructors<LuaParticleGroup(GAME_OBJECT_ID, int)>;

		/// Create a ParticleGroup for managing collections of particles with Lua-driven behavior.
		// If the specified object ID is a sprite sequence, particles render as sprites.
		// If it is a mesh object (moveable), particles render as 3D meshes, and the sub index
		// selects which mesh within the object to draw.
		// @function ParticleGroup
		// @tparam Objects.ObjID objectID Object slot ID (sprite sequence or mesh object).
		// @tparam int maxParticles Maximum number of particles in the group.
		// @treturn ParticleGroup A new ParticleGroup.
		// @usage
		// local group = TEN.Effects.ParticleGroup(TEN.Objects.ObjID.DEFAULT_SPRITES, 200)
		//  group:SetPosition(Vec3(0, 0, 1024))
		//  group:Start()
		parent.new_usertype<LuaParticleGroup>(
			ScriptReserved_ParticleGroup,
			ctors(),
			sol::call_constructor, ctors(),

			/// Start emitting particles.
			// @function ParticleGroup:Start
			// @usage
			// group:Start()
			ScriptReserved_ParticleGroupStart, &LuaParticleGroup::Start,

			/// Stop emitting particles. Existing particles continue until they expire.
			// @function ParticleGroup:Stop
			// @usage
			// group:Stop()
			ScriptReserved_ParticleGroupStop, &LuaParticleGroup::Stop,

			/// Pause the group. Stops emission and freezes all existing particles in place.
			// @function ParticleGroup:Pause
			// @usage
			// group:Pause()
			ScriptReserved_ParticleGroupPause, &LuaParticleGroup::Pause,

			/// Resume a paused group.
			// @function ParticleGroup:Resume
			// @usage
			// group:Resume()
			ScriptReserved_ParticleGroupResume, &LuaParticleGroup::Resume,

			/// Emit a burst of particles immediately.
			// @function ParticleGroup:EmitBurst
			// @tparam int count Number of particles to emit.
			// @usage
			// group:EmitBurst(50)
			ScriptReserved_ParticleGroupEmitBurst, &LuaParticleGroup::EmitBurst,

			/// Get number of active particles.
			// @function ParticleGroup:GetActiveCount
			// @treturn int Number of active particles.
			// @usage
			// local count = group:GetActiveCount()
			ScriptReserved_ParticleGroupGetActiveCount, &LuaParticleGroup::GetActiveCount,

			/// Set the emission rate in particles per second.
			// @function ParticleGroup:SetEmissionRate
			// @tparam float rate Particles per second.
			// @usage
			// group:SetEmissionRate(60)
			ScriptReserved_ParticleGroupSetEmissionRate, &LuaParticleGroup::SetEmissionRate,

			/// Set the emitter position.
			// @function ParticleGroup:SetPosition
			// @tparam Vec3 pos World position.
			// @usage
			// group:SetPosition(Vec3(0, 0, 1024))
			ScriptReserved_ParticleGroupSetPosition, &LuaParticleGroup::SetPosition,

			/// Get the emitter position.
			// @function ParticleGroup:GetPosition
			// @treturn Vec3 World position.
			// @usage
			// local pos = group:GetPosition()
			ScriptReserved_ParticleGroupGetPosition, &LuaParticleGroup::GetPosition,

			/// Set initial velocity for new particles.
			// @function ParticleGroup:SetInitialVelocity
			// @tparam Vec3 vel Velocity vector in world units per second.
			// @usage
			// group:SetInitialVelocity(Vec3(0, 200, 0))
			ScriptReserved_ParticleGroupSetInitialVelocity, &LuaParticleGroup::SetInitialVelocity,

			/// Set random range added to initial velocity.
			// @function ParticleGroup:SetInitialVelocityRandom
			// @tparam Vec3 range Per-axis random range (+/- range).
			// @usage
			// group:SetInitialVelocityRandom(Vec3(300, 40, 300))
			ScriptReserved_ParticleGroupSetVelocityRandom, &LuaParticleGroup::SetInitialVelocityRandom,

			/// Set initial acceleration for new particles.
			// @function ParticleGroup:SetInitialAcceleration
			// @tparam Vec3 accel Acceleration vector in world units per second squared.
			// @usage
			// group:SetInitialAcceleration(Vec3(0, 30, 0))
			ScriptReserved_ParticleGroupSetInitialAcceleration, &LuaParticleGroup::SetInitialAcceleration,

			/// Set a fixed lifetime for new particles.
			// @function ParticleGroup:SetLifetime
			// @tparam float seconds Lifetime in seconds.
			// @usage
			// group:SetLifetime(3.0)
			ScriptReserved_ParticleGroupSetLifetime, &LuaParticleGroup::SetLifetime,

			/// Set lifetime range for new particles.
			// @function ParticleGroup:SetLifetimeRange
			// @tparam float minSeconds Minimum lifetime.
			// @tparam float maxSeconds Maximum lifetime.
			// @usage
			// group:SetLifetimeRange(2.0, 5.0)
			ScriptReserved_ParticleGroupSetLifetimeRange, &LuaParticleGroup::SetLifetimeRange,

			/// Set a fixed initial size for new particles.
			// @function ParticleGroup:SetInitialSize
			// @tparam float size Particle size in world units.
			// @usage
			// group:SetInitialSize(128)
			ScriptReserved_ParticleGroupSetInitialSize, &LuaParticleGroup::SetInitialSize,

			/// Set initial size range for new particles.
			// @function ParticleGroup:SetInitialSizeRange
			// @tparam float min Minimum size.
			// @tparam float max Maximum size.
			// @usage
			// group:SetInitialSizeRange(48, 128)
			ScriptReserved_ParticleGroupSetInitialSizeRange, &LuaParticleGroup::SetInitialSizeRange,

			/// Set initial color for new particles.
			// @function ParticleGroup:SetInitialColor
			// @tparam Color color Particle color.
			// @usage
			// group:SetInitialColor(Color(255, 160, 50))
			ScriptReserved_ParticleGroupSetInitialColor, &LuaParticleGroup::SetInitialColor,

			/// Set initial color range for new particles.
			// @function ParticleGroup:SetInitialColorRange
			// @tparam Color min Minimum color.
			// @tparam Color max Maximum color.
			// @usage
			// group:SetInitialColorRange(Color(255, 100, 20), Color(255, 200, 80))
			ScriptReserved_ParticleGroupSetInitialColorRange, &LuaParticleGroup::SetInitialColorRange,

			/// Set initial rotation for new particles in degrees.
			// @function ParticleGroup:SetInitialRotation
			// @tparam float rotation Rotation in degrees.
			// @usage
			// group:SetInitialRotation(90)
			ScriptReserved_ParticleGroupSetInitialRotation, &LuaParticleGroup::SetInitialRotation,

			/// Set initial rotational velocity for new particles.
			// @function ParticleGroup:SetInitialRotationVelocity
			// @tparam float rotVel Rotational velocity in degrees per second.
			// @usage
			// group:SetInitialRotationVelocity(45)
			ScriptReserved_ParticleGroupSetInitialRotVel, &LuaParticleGroup::SetInitialRotationVelocity,

			/// Set blend mode for rendering. Applies to sprite groups only.
			// Mesh groups use per-material blend modes.
			// @function ParticleGroup:SetBlendMode
			// @tparam Effects.BlendID mode Blend mode.
			// @usage
			// group:SetBlendMode(TEN.Effects.BlendID.ADDITIVE)
			ScriptReserved_ParticleGroupSetBlendMode, &LuaParticleGroup::SetBlendMode,

			/// Set the object slot (sprite sequence or mesh object) for the group.
			// For sprite sequences, the sub-index selects which sprite to draw.
			// For mesh objects, the sub-index selects which mesh to draw.
			// @function ParticleGroup:SetObjectID
			// @tparam Objects.ObjID objectID Object slot ID.
			// @usage
			// group:SetObjectID(TEN.Objects.ObjID.DEFAULT_SPRITES)
			ScriptReserved_ParticleGroupSetObjectID, &LuaParticleGroup::SetObjectID,

			/// Set sprite or mesh sub-index for newly emitted particles.
			// @function ParticleGroup:SetSubIndex
			// @tparam int index Sprite or mesh sub-index.
			// @usage
			// group:SetSubIndex(2)
			ScriptReserved_ParticleGroupSetSubIndex, &LuaParticleGroup::SetSubIndex,

			/// Get the current sprite or mesh sub-index used for new particles.
			// @function ParticleGroup:GetSubIndex
			// @treturn int Current sub-index.
			// @usage
			// local idx = group:GetSubIndex()
			ScriptReserved_ParticleGroupGetSubIndex, &LuaParticleGroup::GetSubIndex,

			/// Get the current object slot ID.
			// @function ParticleGroup:GetObjectID
			// @treturn Objects.ObjID Current object slot ID.
			// @usage
			// local objID = group:GetObjectID()
			ScriptReserved_ParticleGroupGetObjectID, &LuaParticleGroup::GetObjectID,

			/// Check if this group renders 3D meshes.
			// @function ParticleGroup:IsMeshGroup
			// @treturn bool True if particles render as 3D meshes, false for sprites.
			// @usage
			// local isMesh = group:IsMeshGroup()
			ScriptReserved_ParticleGroupIsMeshGroup, &LuaParticleGroup::IsMeshGroup,

			/// Set initial orientation for mesh particles in degrees (pitch, yaw, roll).
			// For sprite groups it fixes the orientation of the sprites.
			// @function ParticleGroup:SetInitialOrientation
			// @tparam Rotation orientation Orientation in degrees.
			// @usage
			// group:SetInitialOrientation(Rotation(0, 90, 0))
			ScriptReserved_ParticleGroupSetInitialOrientation, &LuaParticleGroup::SetInitialOrientation,

			/// Set the contact radius used for deadly bounds detection.
			// Uses an AABB check against Lara's deadly bounds. Default is 128 world units.
			// New particles inherit this value.
			// @function ParticleGroup:SetContactRadius
			// @tparam float radius Distance in world units. Must be greater than 0.
			// @usage
			// group:SetContactRadius(256)
			ScriptReserved_ParticleGroupSetContactRadius, &LuaParticleGroup::SetContactRadius,

			/// Get a specific particle's data as a table.
			// Returns nil if the index is out of range, the particle is inactive, or the group is invalid.
			// @function ParticleGroup:GetParticle
			// @tparam int index Particle index (0-based).
			// @treturn table|nil Particle data table.
			// @usage
			// local p = group:GetParticle(0)
			ScriptReserved_ParticleGroupGetParticle, &LuaParticleGroup::GetParticle,

			/// Set a specific particle's properties from a table.
			// All rotation and orientation values are in degrees.
			// Set teleport = true to suppress interpolation after a large position jump.
			// @function ParticleGroup:SetParticle
			// @tparam int index Particle index (0-based).
			// @tparam table data Table with properties to set.
			// @usage
			// group:SetParticle(0, { position = Vec3(0, 1024, 0), size = 256 })
			ScriptReserved_ParticleGroupSetParticle, &LuaParticleGroup::SetParticle,

			/// Iterate over all active particles, calling a function for each.
			// If the callback returns a table, those fields are applied back to the particle.
			// All rotation and orientation values are in degrees.
			// @function ParticleGroup:ForEachParticle
			// @tparam function callback Function receiving (index, particleTable) for each active particle.
			// @usage
			// group:ForEachParticle(function(index, particle)
			//     return { position = particle.position + Vec3(0, 10, 0) }
			// end)
			ScriptReserved_ParticleGroupForEachParticle, &LuaParticleGroup::ForEachParticle,

		/// Structure for a Particle table.
		// @table Particle
		// @tfield int id Unique particle ID. Read-only.
		// @tfield Vec3 position World position of the particle.
		// @tfield Vec3 velocity Directional velocity in world units per second.
		// @tfield Vec3 acceleration Acceleration applied to velocity in world units per second squared.
		// @tfield float size Current size of the particle in world units. For mesh particles, this also controls the uniform mesh scale.
		// @tfield float rotation Current rotation in degrees. Applies to sprite particles only.
		// @tfield Color color Current color of the particle.
		// @tfield float age Current age of the particle in seconds. Clamped to [0, lifetime] when set.
		// @tfield float lifetime Total lifespan of the particle in seconds. Must be greater than 0 when set.
		// @tfield float ageNormalized Current age as a normalized value between 0 and 1. Recalculated each frame; direct writes are ignored.
		// @tfield int subIndex Current sprite or mesh sub-index within the object slot.
		// @tfield Objects.ObjID objectID Object slot used to render this particle. Overrides the group default per particle.
		// @tfield Rotation orientation Current orientation in degrees along the X, Y, and Z axes. For mesh particles, controls mesh rotation. For sprite particles, a non-zero orientation renders the sprite as a fixed sprite instead of a camera-facing sprite.
		// @tfield float contactRadius Half-extent in world units used for AABB contact detection. Default is 128.
		// @tfield bool touchingPlayer Read-only. True if the particle's contact radius overlaps Lara's deadly bounds.
		// @tfield bool teleport Write-only control. Set to true to suppress interpolation after a large position, size, or rotation change.
	}
}
