#include "framework.h"
#include "Scripting/Internal/TEN/Flow/LensFlare/LensFlare.h"

#include "Objects/game_object_ids.h"
#include "Scripting/Internal/TEN/Types/Color/Color.h"
#include "Scripting/Internal/TEN/Types/Rotation/Rotation.h"
#include "Specific/level.h"

using namespace TEN::Scripting::Types;

/// Represents a global lens flare (not to be confused with the lens flare object). To be used with @{Flow.Level.lensFlare} property.
//
// @tenprimitive Flow.LensFlare
// @pragma nostrip

namespace TEN::Scripting
{
	void LensFlare::Register(sol::table& parent)
	{
		using ctors = sol::constructors<
			LensFlare(float, float, const ScriptColor&)>;

		// Register type.
		parent.new_usertype<LensFlare>(
			"LensFlare",
			ctors(), sol::call_constructor, ctors(),

			/// (bool) Lens flare enabled state.
			// If set to true, lens flare will be visible.
			// @mem enabled
			"enabled", sol::property(&LensFlare::GetEnabled, &LensFlare::SetEnabled),

			/// (int) Lens flare's sun sprite ID in DEFAULT_SPRITES sequence.
			// @mem spriteID
			"spriteID", sol::property(&LensFlare::GetSunSpriteID, &LensFlare::SetSunSpriteID),

			/// (float) Lens flare's pitch (vertical) angle in degrees.
			// @mem pitch
			"pitch", sol::property(&LensFlare::GetPitch, &LensFlare::SetPitch),

			/// (float) Lens flare's yaw (horizontal) angle in degrees.
			// @mem yaw
			"yaw", sol::property(&LensFlare::GetYaw, &LensFlare::SetYaw),

			/// (@{Color}) Lens flare's color.
			// @mem color
			"color", sol::property(&LensFlare::GetColor, &LensFlare::SetColor),

			// Compatibility.
			"GetSunSpriteID", &LensFlare::GetSunSpriteID,
			"GetPitch", &LensFlare::GetPitch,
			"GetYaw", &LensFlare::GetYaw,
			"GetColor", &LensFlare::GetColor,
			"GetEnabled", &LensFlare::GetEnabled,

			"SetSunSpriteID", &LensFlare::SetSunSpriteID,
			"SetPitch", &LensFlare::SetPitch,
			"SetYaw", &LensFlare::SetYaw,
			"SetColor", &LensFlare::SetColor,
			"SetEnabled", &LensFlare::SetEnabled);
	}

	/// Create a LensFlare object.
	// @function LensFlare
	// @tparam float pitch Pitch angle in degrees.
	// @tparam float yaw Yaw angle in degrees.
	// @tparam @{Color} color Color of the lensflare.
	// @treturn LensFlare A new LensFlare object.
	LensFlare::LensFlare(float pitch, float yaw, const ScriptColor& color)
	{
		_isEnabled = true;
		_color = color;
		_rotation = Rotation(pitch, yaw, 0.0f);
	}

	int LensFlare::GetSunSpriteID() const
	{
		return _sunSpriteID;
	}

	float LensFlare::GetPitch() const
	{
		return _rotation.x;
	}
	
	float LensFlare::GetYaw() const
	{
		return _rotation.y;
	}

	ScriptColor LensFlare::GetColor() const
	{
		return _color;
	}

	bool LensFlare::GetEnabled() const
	{
		return _isEnabled;
	}

	void LensFlare::SetSunSpriteID(int spriteID)
	{
		// Sprite ID out of range; return early.
		if (spriteID < 0 || g_Level.Sprites.size() > spriteID)
		{
			TENLog("Sun sprite ID out of range.");
			return;
		}

		_sunSpriteID = spriteID;
	}

	void LensFlare::SetPitch(float pitch)
	{
		_rotation.x = pitch;
	}

	void LensFlare::SetYaw(float yaw)
	{
		_rotation.y = yaw;
	}

	void LensFlare::SetColor(const ScriptColor& color)
	{
		_color = color;
	}
	
	void LensFlare::SetEnabled(bool value)
	{
		_isEnabled = value;
	}
}
