#pragma once

#include "Renderer/RendererEnums.h"

namespace TEN::Scripting::Effects
{
	/// Constants for blend mode IDs.
	// All blending modes except `OPAQUE`, `ADDITIVE` and `ALPHA_BLEND` will use depth sorting for applicable polygons.
	// This may reduce engine performance, so it is preferable to minimize usage of other blending modes.
	// @enum Effects.BlendID
	// @pragma nostrip

	static const auto BLEND_IDS = std::unordered_map<std::string, BlendMode>
	{
		/// No transparency.
		// @mem OPAQUE
		{ "OPAQUE", BlendMode::Opaque },

		/// So called "magenta transparency", every pixel can be either fully transparent or opaque.
		// @mem ALPHA_TEST
		{ "ALPHA_TEST", BlendMode::AlphaTest },

		/// Standard additive blending.
		// @mem ADDITIVE
		{ "ADDITIVE", BlendMode::Additive },

		/// No depth test blending.
		// @mem NO_DEPTH_TEST
		{ "NO_DEPTH_TEST", BlendMode::NoDepthTest },

		/// Subtractive blending, with brighter texture areas making everything darker behind them.
		// @mem SUBTRACTIVE
		{ "SUBTRACTIVE", BlendMode::Subtractive },

		/// Wireframe mode.
		// @mem WIREFRAME
		{ "WIREFRAME", BlendMode::Wireframe },

		/// Produces "inversion" effect.
		// @mem EXCLUDE
		{ "EXCLUDE", BlendMode::Exclude },

		/// Similar to `ADDITIVE`, but without excessive overbright.
		// @mem SCREEN
		{ "SCREEN", BlendMode::Screen },

		/// Similar to `SCREEN`, but with a little different blending formula.
		// @mem LIGHTEN
		{ "LIGHTEN", BlendMode::Lighten },

		/// True alpha blending. Should be used for textures with gradually changing alpha values.
		// @mem ALPHA_BLEND
		{ "ALPHA_BLEND", BlendMode::AlphaBlend },

		// COMPATIBILITY
		{ "ALPHATEST", BlendMode::AlphaTest },
		{ "NOZTEST", BlendMode::NoDepthTest },
		{ "ALPHABLEND", BlendMode::AlphaBlend }
	};
}
