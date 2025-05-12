#pragma once
#include <string>
#include <unordered_map>

#include "Game/effects/DisplaySprite.h"

using namespace TEN::Effects::DisplaySprite;

namespace TEN::Scripting::View
{

	/// Constants for display sprite scale modes.
	// @enum View.ScaleMode
	// @pragma nostrip

	/// Table of View.ScaleMode constants. To be used with @{View.DisplaySprite} class.
	// 
	// - `FIT` - Image will proportionally fit the whole image into the sprite surface.
	// - `FILL` - Image will scale up proportionally and crop to fill all sprite surface.
	// - `STRETCH` - Image will stretch according to sprite dimensions, not taking aspect ratio into consideration.
	// 
	// @table View.ScaleMode

	static const std::unordered_map<std::string, DisplaySpriteScaleMode> SCALE_MODES
	{
		{ "FIT", DisplaySpriteScaleMode::Fit },
		{ "FILL", DisplaySpriteScaleMode::Fill },
		{ "STRETCH", DisplaySpriteScaleMode::Stretch }
	};
}
