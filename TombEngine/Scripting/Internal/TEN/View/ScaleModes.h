#pragma once
#include <string>
#include <unordered_map>

#include "Game/effects/DisplaySprite.h"

using namespace TEN::Effects::DisplaySprite;

namespace TEN::Scripting::View
{
	/// Constants for display sprite scale modes.
	// To be used with @{View.DisplaySprite} class.
	// @enum View.ScaleMode
	// @pragma nostrip

	static const std::unordered_map<std::string, DisplaySpriteScaleMode> SCALE_MODES
	{
		/// Image will proportionally fit the whole image into the sprite surface.
		// @mem FIT
		{ "FIT", DisplaySpriteScaleMode::Fit },

		/// Image will scale up proportionally and crop to fill all sprite surface.
		// @mem FILL
		{ "FILL", DisplaySpriteScaleMode::Fill },

		/// Image will stretch according to sprite dimensions, not taking aspect ratio into consideration.
		// @mem STRETCH
		{ "STRETCH", DisplaySpriteScaleMode::Stretch }
	};
}
