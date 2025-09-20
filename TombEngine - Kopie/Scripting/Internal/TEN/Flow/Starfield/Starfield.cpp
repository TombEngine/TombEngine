#include "framework.h"
#include "Scripting/Internal/TEN/Flow/Starfield/Starfield.h"

#include "Game/effects/weather.h"
#include "Specific/level.h"

using namespace TEN::Effects::Environment;

/// Represents a star field in the sky. To be used with @{Flow.Level.starfield} property.
// @tenprimitive Flow.Starfield
// @pragma nostrip

namespace TEN::Scripting
{
	void Starfield::Register(sol::table& parent)
	{
		using ctors = sol::constructors<
			Starfield(int),
			Starfield(int, int, int, float)>;
		
		// Register type.
		parent.new_usertype<Starfield>(
			"Starfield",
			ctors(), sol::call_constructor, ctors(),

			/// (int) Amount of visible stars.
			// @mem starCount
			"starCount", sol::property(&Starfield::GetStarCount, &Starfield::SetStarCount),

			/// (int) Amount of visible meteors.
			// @mem meteorCount
			"meteorCount", sol::property(&Starfield::GetMeteorCount, &Starfield::SetMeteorCount),

			/// (int) Meteor spawn density.
			// @mem meteorSpawnDensity
			"meteorSpawnDensity", sol::property(&Starfield::GetMeteorSpawnDensity, &Starfield::SetMeteorSpawnDensity),

			/// (int) Meteor velocity.
			// @mem meteorVelocity
			"meteorVelocity", sol::property(&Starfield::GetMeteorVelocity, &Starfield::SetMeteorVelocity),

			// Compatibility.
			"GetStarCount", &Starfield::GetStarCount,
			"GetMeteorCount", &Starfield::GetMeteorCount,
			"GetMeteorSpawnDensity", &Starfield::GetMeteorSpawnDensity,
			"GetMeteorVelocity", &Starfield::GetMeteorVelocity,

			"SetStarCount", &Starfield::SetStarCount,
			"SetMeteorCount", &Starfield::SetMeteorCount,
			"SetMeteorSpawnDensity", &Starfield::SetMeteorSpawnDensity,
			"SetMeteorVelocity", &Starfield::SetMeteorVelocity);
	}

	/// Create a starfield object with only stars.
	// @function Starfield
	// @tparam int starCount Star count.
	// @treturn Starfield A new Starfield object.
	Starfield::Starfield(int starCount)
	{
		_starCount = starCount;
	}

	/// Create a starfield object with stars and meteors.
	// @function Starfield
	// @tparam int starCount Star count. __Max: 6000__
	// @tparam int meteorCount Meteor count. __Max: 100__
	// @tparam int meteorSpawnDensity Meteor spawn density.
	// @tparam int meteorVel Meteor velocity.
	// @treturn Starfield A new Starfield object.
	Starfield::Starfield(int starCount, int meteorCount, int meteorSpawnDensity, float meteorVel)
	{
		if (starCount < 0 || starCount > STAR_COUNT_MAX)
			TENLog("Star count must be in range [0, " + std::to_string(STAR_COUNT_MAX) + "].", LogLevel::Warning);

		if (meteorCount < 0 || meteorCount > METEOR_COUNT_MAX)
			TENLog("Meteor count must be in range [0, " + std::to_string(METEOR_COUNT_MAX) + "].", LogLevel::Warning);

		_starCount = std::clamp(starCount, 0, STAR_COUNT_MAX);
		_meteorCount = std::clamp(meteorCount, 0, METEOR_COUNT_MAX);
		_meteorSpawnDensity = meteorSpawnDensity;
		_meteorVelocity = meteorVel;
	}

	int Starfield::GetStarCount() const
	{
		return _starCount;
	}

	int Starfield::GetMeteorCount() const
	{
		return _meteorCount;
	}

	int Starfield::GetMeteorSpawnDensity() const
	{
		return _meteorSpawnDensity;
	}

	float Starfield::GetMeteorVelocity() const
	{
		return _meteorVelocity;
	}

	void Starfield::SetStarCount(int count)
	{
		if (count < 0 || count > STAR_COUNT_MAX)
			TENLog("Star count must be in range [0, " + std::to_string(STAR_COUNT_MAX) + "].", LogLevel::Warning);

		_starCount = std::clamp(count, 0, STAR_COUNT_MAX);
	}

	void Starfield::SetMeteorCount(int count)
	{
		if (count < 0 || count > METEOR_COUNT_MAX)
			TENLog("Meteor count must be in range [0, " + std::to_string(METEOR_COUNT_MAX) + "].", LogLevel::Warning);

		_meteorCount = std::clamp(count, 0, METEOR_COUNT_MAX);
	}

	void Starfield::SetMeteorSpawnDensity(int spawnDensity)
	{
		_meteorSpawnDensity = spawnDensity;
	}

	void Starfield::SetMeteorVelocity(float vel)
	{
		_meteorVelocity = vel;
	}
}
