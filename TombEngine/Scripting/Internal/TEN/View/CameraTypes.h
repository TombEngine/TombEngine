#pragma once
#include "Game/camera.h"

/***
Constants for the type of the Camera.
@enum View.CameraType
@pragma nostrip
*/

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

/*** Table of View.CameraType constants. To be used with @{View.GetCameraType} function.
@table CameraType

 - `NORMAL` - Standard in-game camera when weapons are holstered.
 - `COMBAT` - In-game camera when weapons are unholstered.
 - `FIXED` - Classic fixed camera.
 - `LOOK` - Look camera.
 - `FLYBY` - Flyby or tracking camera.
 - `BINOCULARS` - Binocular camera.
 - `LASERSIGHT` - Lasersight camera.
*/

static const std::unordered_map<std::string, ScriptCameraType> CAMERA_TYPE
{
	{ "CHASE",		ScriptCameraType::Normal		}, // DEPRECATED
	{ "NORMAL",		ScriptCameraType::Normal		},
	{ "COMBAT",		ScriptCameraType::Combat		},
	{ "FIXED",		ScriptCameraType::Fixed			},
	{ "HEAVY",		ScriptCameraType::Fixed			}, // DEPRECATED
	{ "LOOK",		ScriptCameraType::Look			},
	{ "FLYBY",		ScriptCameraType::Flyby			},
	{ "BINOCULARS", ScriptCameraType::Binoculars	},
	{ "LASERSIGHT", ScriptCameraType::Lasersight	}
};
