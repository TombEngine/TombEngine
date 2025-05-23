#pragma once
#include "Renderer/RendererEnums.h"
#include <string>

/***
Constants for the post-process effects to apply.
@enum View.PostProcessMode
@pragma nostrip
*/

/*** Table of View.PostProcessMode effect constants. To be used with @{View.SetPostProcessMode} function.

 - `NONE` - No postprocess effect.
 - `MONOCHROME` - Black & white effect.
 - `NEGATIVE` - Negative image effect.
 - `EXCLUSION` - Similar to negative effect, but with different color operation.

@table View.PostProcessMode
*/

/***  (for use with SetPostProcessMode() function).
@ PostProcessMode
*/

static const std::unordered_map<std::string, PostProcessMode> POSTPROCESS_MODES
{
	{ "NONE", PostProcessMode::None },
	{ "MONOCHROME", PostProcessMode::Monochrome },
	{ "NEGATIVE", PostProcessMode::Negative },
	{ "EXCLUSION", PostProcessMode::Exclusion }
};
