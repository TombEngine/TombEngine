#pragma once
#include "Game/camera.h"

/// Constants for the type of the Camera.
// To be used with @{View.GetCameraType} function.
// @enum View.CameraType
// @pragma nostrip

enum class ScriptCameraType
{
	Normal,
	Fixed,
	Look,
	Combat,
	Flyby,
	Binoculars,
	Lasersight
};

static const std::unordered_map<std::string, ScriptCameraType> CAMERA_TYPE
{
	/// Standard in-game camera when weapons are holstered.
	// @mem NORMAL
	{ "NORMAL", ScriptCameraType::Normal },

	/// In-game camera when weapons are unholstered.
	// @mem COMBAT
	{ "COMBAT", ScriptCameraType::Combat },

	/// Classic fixed camera.
	// @mem FIXED
	{ "FIXED", ScriptCameraType::Fixed },

	/// Look camera.
	// @mem LOOK
	{ "LOOK", ScriptCameraType::Look },

	/// Flyby or tracking camera.
	// @mem FLYBY
	{ "FLYBY", ScriptCameraType::Flyby },

	/// Binocular camera.
	// @mem BINOCULARS
	{ "BINOCULARS", ScriptCameraType::Binoculars },

	/// Lasersight camera.
	// @mem LASERSIGHT
	{ "LASERSIGHT", ScriptCameraType::Lasersight },

	// Deprecated aliases
	{ "CHASE", ScriptCameraType::Normal }, // DEPRECATED
	{ "HEAVY", ScriptCameraType::Fixed }   // DEPRECATED
};
