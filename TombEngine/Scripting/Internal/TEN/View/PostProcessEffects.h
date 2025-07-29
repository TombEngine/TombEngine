#pragma once
#include "Renderer/RendererEnums.h"
#include <string>

/// Constants for the post-process effects to apply.
// To be used with @{View.SetPostProcessMode} function.
// @enum View.PostProcessMode
// @pragma nostrip

static const std::unordered_map<std::string, PostProcessMode> POSTPROCESS_MODES
{
	/// No postprocess effect.
	// @mem NONE
	{ "NONE", PostProcessMode::None },

	/// Black & white effect.
	// @mem MONOCHROME
	{ "MONOCHROME", PostProcessMode::Monochrome },

	/// Negative image effect.
	// @mem NEGATIVE
	{ "NEGATIVE", PostProcessMode::Negative },

	/// Similar to negative effect, but with different color operation.
	// @mem EXCLUSION
	{ "EXCLUSION", PostProcessMode::Exclusion }
};
