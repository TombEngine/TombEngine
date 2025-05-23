#pragma once
#include "game_object_ids.h"

template <typename T, typename Enum> constexpr bool is_underlying = std::is_same_v<T, std::underlying_type_t<Enum>>;
template <typename T, typename Enum> constexpr bool same_or_underlying = std::is_same_v<T, Enum> || is_underlying<T, Enum>;

// += operator that only allows addition of shorts or GAME_OBJECT_IDs. This is to
// allow us to see and manually review any places where we're adding something that
// might be the wrong type.
template <typename T> std::enable_if_t<same_or_underlying<T, GAME_OBJECT_ID>, GAME_OBJECT_ID>& operator+=(GAME_OBJECT_ID& lhs, T const& rhs)
{
	lhs = GAME_OBJECT_ID{ lhs + GAME_OBJECT_ID{ rhs } };
	return lhs;
}

template <typename T> std::enable_if_t<std::is_same_v<T, std::underlying_type_t<GAME_OBJECT_ID>>, GAME_OBJECT_ID>
	from_underlying(T rhs)
	{
		return GAME_OBJECT_ID{ rhs };
	}

	enum SPRITE_TYPES
	{
		SPR_FIRE0,
		SPR_FIRE1,
		SPR_FIRE2,
		SPR_FIRE3,
		SPR_SPLASH1,
		SPR_SPLASH2,
		SPR_SPLASH3,
		SPR_SPLASH4,
		SPR_SPLASH,
		SPR_RIPPLES,
		SPR_LENS_FLARE,
		SPR_LENS_FLARE_LIGHT,
		SPR_BULLETIMPACT,
		SPR_BUBBLES,
		SPR_UNDERWATERDUST,
		SPR_BLOOD,
		SPR_EMPTY1,
		SPR_BUBBLE_CLUSTER,
		SPR_EMPTY2,
		SPR_BACKGROUND,
		SPR_GUI_UPLEFT,
		SPR_GUI_UPRIGHT,
		SPR_GUI_DOWNLEFT,
		SPR_GUI_DOWNRIGHT,
		SPR_GUI_DOWN,
		SPR_GUI_UP,
		SPR_GUI_LEFT,
		SPR_GUI_RIGHT,
		SPR_LIGHTHING,
		SPR_LENS_FLARE_1,
		SPR_LENS_FLARE_2,
		SPR_LENS_FLARE_3,
		SPR_LENS_FLARE_4
	};
