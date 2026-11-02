#pragma once

#include "Objects/game_object_ids.h"
#include "Objects/objectslist.h"
#include "Scripting/Internal/TEN/Types/Color/Color.h"
#include "Scripting/Internal/TEN/Types/Rotation/Rotation.h"

namespace sol { class state; }

namespace TEN::Scripting::Types { class ScriptColor; }

using namespace TEN::Scripting::Types;

namespace TEN::Scripting
{
	// Color mode for the global lens flare / sun.
	enum class LensFlareColorMode
	{
		AutoRealistic,     // No color provided — derive from sun elevation.
		SingleColor,       // One color provided — use as-is.
		GradientTwoColor   // Two colors provided — interpolate by elevation.
	};

	class LensFlare
	{
	public:
		static void Register(sol::table& parent);

	private:
		// Fields

		int	 _sunSpriteID = SPRITE_TYPES::SPR_LENS_FLARE_3;
		bool _isEnabled	  = false;
		bool _effects	  = true; // Procedural starburst + ghost lens artifacts.

		Rotation	_rotation = {};
		ScriptColor _color	  = 0;
		ScriptColor _colorB   = ScriptColor(255, 250, 235);  // Zenith color for gradient mode.

		LensFlareColorMode _colorMode = LensFlareColorMode::SingleColor;

	public:
		// Constructors

		LensFlare() = default;
		LensFlare(float pitch, float yaw);                                              // AutoRealistic
		LensFlare(float pitch, float yaw, const ScriptColor& color);                    // SingleColor
		LensFlare(float pitch, float yaw, const ScriptColor& colorA, const ScriptColor& colorB); // Gradient

		// Getters

		int			       GetSunSpriteID() const;
		float		       GetPitch() const;
		float		       GetYaw() const;
		ScriptColor        GetColor() const;
		ScriptColor        GetColorB() const;
		bool		       GetEnabled() const;
		bool		       GetEffects() const;
		LensFlareColorMode GetColorMode() const;

		// Setters

		void SetSunSpriteID(int spriteID);
		void SetPitch(float pitch);
		void SetYaw(float yaw);
		void SetColor(const ScriptColor& color);
		void SetColorB(const ScriptColor& color);
		void SetEnabled(bool value);
		void SetEffects(bool value);
		void SetColorMode(LensFlareColorMode mode);

		// Evaluate the effective sun color for the current elevation.
		Color EvaluateColor() const;

		// Compute normalized elevation [0..1] from pitch (0 = horizon, 1 = zenith).
		float GetNormalizedElevation() const;
	};

	// Compute realistic sun color from normalized elevation [0,1].
	Color ComputeRealisticSunColor(float elevation);
}
