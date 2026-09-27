#pragma once
#include "Renderer/RendererEnums.h"
#include <string>

/// Constants for the depth-of-field blur mode.
// To be used with @{View.SetDOF} function.
// @enum View.DOFMode
// @pragma nostrip

static const std::unordered_map<std::string, DOFMode> DOF_MODES
{
	/// No depth-of-field effect.
	// @mem NONE
	{ "NONE", DOFMode::None },

	/// Blur both the foreground and the background of the scene.
	// @mem FULL
	{ "FULL", DOFMode::Full },

	/// Blur scene foreground only.
	// @mem FRONT
	{ "FRONT", DOFMode::Front },

	/// Blur scene background only.
	// @mem BACK
	{ "BACK", DOFMode::Back }
};
