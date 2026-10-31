#include "framework.h"
#include "Scripting/Internal/TEN/Flow/LensFlare/LensFlare.h"

#include <algorithm>
#include <cmath>

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
			LensFlare(float, float),
			LensFlare(float, float, const ScriptColor&),
			LensFlare(float, float, const ScriptColor&, const ScriptColor&)>;

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

			/// (@{Color}) Lens flare's primary color (colorA).
			// @mem color
			"color", sol::property(&LensFlare::GetColor, &LensFlare::SetColor),

			/// (@{Color}) Lens flare's secondary color (colorB, for gradient mode).
			// @mem colorB
			"colorB", sol::property(&LensFlare::GetColorB, &LensFlare::SetColorB),

			/// (bool) Procedural lens flare effects (starburst spike + ghost lens artifacts).
			// Default = true. Set to false to draw only the central sun sprite without the
			// secondary screen-space glare, e.g. when using the sun sprite as a moon for night levels.
			// @mem effects
			"effects", sol::property(&LensFlare::GetEffects, &LensFlare::SetEffects),

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

	// ---- Constructors ----

	/// Create a LensFlare with automatic realistic sun color.
	// @function LensFlare
	// @tparam float pitch Pitch angle in degrees.
	// @tparam float yaw Yaw angle in degrees.
	// @treturn LensFlare A new LensFlare with auto-realistic color.
	LensFlare::LensFlare(float pitch, float yaw)
	{
		_isEnabled = true;
		_rotation = Rotation(pitch, yaw, 0.0f);
		_colorMode = LensFlareColorMode::AutoRealistic;
	}

	/// Create a LensFlare with a single fixed color.
	// @function LensFlare
	// @tparam float pitch Pitch angle in degrees.
	// @tparam float yaw Yaw angle in degrees.
	// @tparam @{Color} color Color of the lens flare.
	// @treturn LensFlare A new LensFlare with a fixed color.
	LensFlare::LensFlare(float pitch, float yaw, const ScriptColor& color)
	{
		_isEnabled = true;
		_color = color;
		_rotation = Rotation(pitch, yaw, 0.0f);
		_colorMode = LensFlareColorMode::SingleColor;
	}

	/// Create a LensFlare with a two-color gradient based on elevation.
	// @function LensFlare
	// @tparam float pitch Pitch angle in degrees.
	// @tparam float yaw Yaw angle in degrees.
	// @tparam @{Color} colorA Horizon color (low elevation).
	// @tparam @{Color} colorB Zenith color (high elevation).
	// @treturn LensFlare A new LensFlare with gradient colors.
	LensFlare::LensFlare(float pitch, float yaw, const ScriptColor& colorA, const ScriptColor& colorB)
	{
		_isEnabled = true;
		_color = colorA;
		_colorB = colorB;
		_rotation = Rotation(pitch, yaw, 0.0f);
		_colorMode = LensFlareColorMode::GradientTwoColor;
	}

	// ---- Getters ----

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

	ScriptColor LensFlare::GetColorB() const
	{
		return _colorB;
	}

	bool LensFlare::GetEnabled() const
	{
		return _isEnabled;
	}

	bool LensFlare::GetEffects() const
	{
		return _effects;
	}

	LensFlareColorMode LensFlare::GetColorMode() const
	{
		return _colorMode;
	}

	// ---- Setters ----

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

	void LensFlare::SetColorB(const ScriptColor& color)
	{
		_colorB = color;
	}
	
	void LensFlare::SetEnabled(bool value)
	{
		_isEnabled = value;
	}

	void LensFlare::SetEffects(bool value)
	{
		_effects = value;
	}

	void LensFlare::SetColorMode(LensFlareColorMode mode)
	{
		_colorMode = mode;
	}

	// ---- Color evaluation ----

	float LensFlare::GetNormalizedElevation() const
	{
		// Pitch: 0 = horizontal, 90 = straight up, negative = below horizon.
		float pitch = _rotation.x;
		float elevation = std::clamp(pitch / 90.0f, 0.0f, 1.0f);
		return elevation;
	}

	Color LensFlare::EvaluateColor() const
	{
		switch (_colorMode)
		{
		case LensFlareColorMode::AutoRealistic:
			return ComputeRealisticSunColor(GetNormalizedElevation());

		case LensFlareColorMode::GradientTwoColor:
		{
			float t = GetNormalizedElevation();
			// Smooth the interpolation for a nicer gradient.
			t = t * t * (3.0f - 2.0f * t); // smoothstep

			Color cA = _color;
			Color cB = _colorB;
			Color result;
			result.x = cA.x + (cB.x - cA.x) * t;
			result.y = cA.y + (cB.y - cA.y) * t;
			result.z = cA.z + (cB.z - cA.z) * t;
			result.w = cA.w + (cB.w - cA.w) * t;
			return result;
		}

		case LensFlareColorMode::SingleColor:
		default:
			return _color;
		}
	}

	// ---- Realistic automatic sun color model ----

	Color ComputeRealisticSunColor(float elevation)
	{
		// elevation: 0 = horizon, 1 = zenith.
		// Apply a slight power curve so the red/orange zone
		// lingers near the horizon and transitions smoothly.
		float t = std::clamp(elevation, 0.0f, 1.0f);

		// Key color stops (linear RGB, 0..1):
		//   t=0.00  deep orange-red   (255, 100,  40) / 255
		//   t=0.15  warm orange       (255, 160,  80) / 255
		//   t=0.35  golden yellow     (255, 220, 150) / 255
		//   t=0.60  pale yellow       (255, 245, 220) / 255
		//   t=1.00  near-white warm   (255, 253, 245) / 255

		struct ColorStop { float pos; float r, g, b; };
		constexpr ColorStop stops[] = {
			{ 0.00f, 1.000f, 0.392f, 0.157f },  // deep orange-red
			{ 0.15f, 1.000f, 0.627f, 0.314f },  // warm orange
			{ 0.35f, 1.000f, 0.863f, 0.588f },  // golden yellow
			{ 0.60f, 1.000f, 0.961f, 0.863f },  // pale yellow
			{ 1.00f, 1.000f, 0.992f, 0.961f },  // near-white warm
		};
		constexpr int stopCount = sizeof(stops) / sizeof(stops[0]);

		// Find the two surrounding stops.
		int idx = 0;
		for (int i = 0; i < stopCount - 1; i++)
		{
			if (t >= stops[i].pos && t <= stops[i + 1].pos)
			{
				idx = i;
				break;
			}
		}

		float range = stops[idx + 1].pos - stops[idx].pos;
		float localT = (range > 0.0f) ? (t - stops[idx].pos) / range : 0.0f;
		// Apply smoothstep to local interpolation.
		localT = localT * localT * (3.0f - 2.0f * localT);

		float r = stops[idx].r + (stops[idx + 1].r - stops[idx].r) * localT;
		float g = stops[idx].g + (stops[idx + 1].g - stops[idx].g) * localT;
		float b = stops[idx].b + (stops[idx + 1].b - stops[idx].b) * localT;

		return Color(r, g, b, 1.0f);
	}
}
