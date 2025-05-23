#pragma once

#include "Game/Effects/DisplaySprite.h"

using namespace TEN::Effects::DisplaySprite;

namespace TEN::Scripting::View
{
	/// Constants for sprite align modes.
	// @enum View.AlignMode
	// @pragma nostrip

	/// Table of View.AlignMode constants.
	// 
	// The following constants are inside View.AlignMode. To be used with @{Strings.DisplayString} class.
	// 
	// - `CENTER`
	// - `CENTER_TOP`
	// - `CENTER_BOTTOM`
	// - `CENTER_LEFT`
	// - `CENTER_RIGHT`
	// - `TOP_LEFT`
	// - `TOP_RIGHT`
	// - `BOTTOM_LEFT`
	// - `BOTTOM_RIGHT`
	// 
	// @table View.AlignMode

	static const std::unordered_map<std::string, DisplaySpriteAlignMode> ALIGN_MODES
	{
		{ "CENTER", DisplaySpriteAlignMode::Center },
		{ "CENTER_TOP", DisplaySpriteAlignMode::CenterTop },
		{ "CENTER_BOTTOM", DisplaySpriteAlignMode::CenterBottom },
		{ "CENTER_LEFT", DisplaySpriteAlignMode::CenterLeft },
		{ "CENTER_RIGHT", DisplaySpriteAlignMode::CenterRight },
		{ "TOP_LEFT", DisplaySpriteAlignMode::TopLeft },
		{ "TOP_RIGHT", DisplaySpriteAlignMode::TopRight },
		{ "BOTTOM_LEFT", DisplaySpriteAlignMode::BottomLeft },
		{ "BOTTOM_RIGHT", DisplaySpriteAlignMode::BottomRight }
	};
}
