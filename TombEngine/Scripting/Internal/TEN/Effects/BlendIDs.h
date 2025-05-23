#pragma once

#include "Renderer/RendererEnums.h"

namespace TEN::Scripting::Effects
{
	/// Constants for blend mode IDs.
	// @enum Effects.BlendID
	// @pragma nostrip

	/// Table of Effects.BlendID constants.
	// <br>
	// All blending modes except `OPAQUE`, `ADDITIVE` and `ALPHA_BLEND` will use depth sorting for applicable polygons.
	// This may reduce engine performance, so it is preferable to minimize usage of other blending modes.
	//
	// - `OPAQUE` - No transparency.
	// - `ALPHA_TEST` - So called "magenta transparency", every pixel can be either fully transparent or opaque.
	// - `ADDITIVE` - Standard additive blending.
	// - `SUBTRACTIVE` - Subtractive blending, with brighter texture areas making everything darker behind them.
	// - `EXCLUDE` - Produces "inversion" effect.
	// - `SCREEN` - Similar to `ADDITIVE`, but without excessive overbright.
	// - `LIGHTEN` - Similar to `SCREEN`, but with a little different blending formula.
	// - `ALPHA_BLEND` - True alpha blending. Should be used for textures with gradually changing alpha values.
	//
	// @table Effects.BlendID

	static const auto BLEND_IDS = std::unordered_map<std::string, BlendMode>
	{
		{ "OPAQUE", BlendMode::Opaque },
		{ "ALPHA_TEST", BlendMode::AlphaTest },
		{ "ADDITIVE", BlendMode::Additive },
		{ "NO_DEPTH_TEST", BlendMode::NoDepthTest },
		{ "SUBTRACTIVE", BlendMode::Subtractive },
		{ "WIREFRAME", BlendMode::Wireframe },
		{ "EXCLUDE", BlendMode::Exclude },
		{ "SCREEN", BlendMode::Screen },
		{ "LIGHTEN", BlendMode::Lighten },
		{ "ALPHA_BLEND", BlendMode::AlphaBlend },

		// COMPATIBILITY
		{ "ALPHATEST", BlendMode::AlphaTest },
		{ "NOZTEST", BlendMode::NoDepthTest },
		{ "ALPHABLEND", BlendMode::AlphaBlend }
	};
}
