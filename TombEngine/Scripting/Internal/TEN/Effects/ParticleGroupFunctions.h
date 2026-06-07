#pragma once

#include "Game/effects/ParticleGroup.h"
#include "Scripting/Internal/TEN/Types/Color/Color.h"
#include "Scripting/Internal/TEN/Types/Vec3/Vec3.h"

using namespace TEN::Effects::ParticleGroups;
using namespace TEN::Scripting::Types;

namespace sol { class state; }

namespace TEN::Scripting::Effects::ParticleGroups
{
	class LuaParticleGroup
	{
	public:
		static void Register(sol::table& parent);

	private:
		ParticleGroupHandle _handle = {};

		static sol::table MakeParticleTable(sol::state_view& lua, const GroupParticle& p);
		static void       ApplyParticleTable(GroupParticle& p, const sol::table& data);

	public:
		LuaParticleGroup() = default;
		LuaParticleGroup(GAME_OBJECT_ID objectID, int maxParticles);

		// Emission control
		void Start();
		void Stop();
		void Pause();
		void Resume();
		void EmitBurst(int count);

		// Queries
		int  GetActiveCount() const;
		bool IsMeshGroup() const;
		int  GetID() const;
		bool IsActive() const;

		// Emitter
		void                SetEmissionRate(float rate);
		void                SetPosition(const Vec3& pos);
		sol::optional<Vec3> GetPosition() const;

		// Initial particle properties
		void                          SetInitialVelocity(const Vec3& vel);
		void                          SetInitialVelocityRandom(const Vec3& range);
		void                          SetInitialAcceleration(const Vec3& accel);
		void                          SetLifetime(float seconds);
		void                          SetLifetimeRange(float minSec, float maxSec);
		void                          SetInitialSize(float size);
		void                          SetInitialSizeRange(float minSize, float maxSize);
		void                          SetInitialColor(const ScriptColor& color);
		void                          SetInitialColorRange(const ScriptColor& minColor, const ScriptColor& maxColor);
		void                          SetInitialRotation(float rotation);
		void                          SetInitialRotationVelocity(float rotVel);
		void                          SetBlendMode(BlendMode mode);
		void                          SetObjectID(GAME_OBJECT_ID objectID);
		void                          SetSubIndex(int index);
		sol::optional<int>            GetSubIndex() const;
		sol::optional<GAME_OBJECT_ID> GetObjectID() const;
		void                          SetInitialOrientation(const Rotation& orient);

		// Contact
		void SetContactRadius(float radius);

		// Per-particle access
		sol::object GetParticle(int index, sol::this_state s) const;
		void        SetParticle(int index, sol::table data);
		void        ForEachParticle(sol::function callback, sol::this_state s);
	};
}
