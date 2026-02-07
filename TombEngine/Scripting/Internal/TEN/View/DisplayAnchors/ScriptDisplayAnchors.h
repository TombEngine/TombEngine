#pragma once

#include "Scripting/Internal/TEN/Types/Vec2/Vec2.h"

namespace TEN::Scripting::Types
{
	class ScriptDisplayAnchors
	{
	public:
		static void Register(sol::table& parent);

		// Fields

		Vec2 TOP_LEFT = Vec2(0.0f, 0.0f);
		Vec2 TOP_CENTER = Vec2(0.0f, 0.0f);
		Vec2 TOP_RIGHT = Vec2(0.0f, 0.0f);
		Vec2 CENTER_LEFT = Vec2(0.0f, 0.0f);
		Vec2 CENTER = Vec2(0.0f, 0.0f);
		Vec2 CENTER_RIGHT = Vec2(0.0f, 0.0f);
		Vec2 BOTTOM_LEFT = Vec2(0.0f, 0.0f);
		Vec2 BOTTOM_CENTER = Vec2(0.0f, 0.0f);
		Vec2 BOTTOM_RIGHT = Vec2(0.0f, 0.0f);

		// Constructors

		ScriptDisplayAnchors() = default;
		ScriptDisplayAnchors(const Vec2& TOP_LEFT, const Vec2& TOP_CENTER, const Vec2& TOP_RIGHT,
		               const Vec2& CENTER_LEFT, const Vec2& CENTER, const Vec2& CENTER_RIGHT,
		               const Vec2& BOTTOM_LEFT, const Vec2& BOTTOM_CENTER, const Vec2& BOTTOM_RIGHT);

		// Meta functions

		std::string ToString() const;
	};
}
