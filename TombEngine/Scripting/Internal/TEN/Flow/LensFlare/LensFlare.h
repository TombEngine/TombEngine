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
	class LensFlare
	{
	public:
		static void Register(sol::table& parent);

	private:
		// Fields

		int	 _sunSpriteID = SPRITE_TYPES::SPR_LENS_FLARE_3;
		bool _isEnabled	  = false;

		Rotation	_rotation = {};
		ScriptColor _color	  = 0;

	public:
		// Constructors

		LensFlare() = default;
		LensFlare(float pitch, float yaw, const ScriptColor& color);

		// Getters

		int			GetSunSpriteID() const;
		float		GetPitch() const;
		float		GetYaw() const;
		ScriptColor GetColor() const;
		bool		GetEnabled() const;

		// Setters

		void SetSunSpriteID(int spriteID);
		void SetPitch(float pitch);
		void SetYaw(float yaw);
		void SetColor(const ScriptColor& color);
		void SetEnabled(bool value);
	};
}
