#pragma once
#include <string>
#include <unordered_map>

#include "Sound/sound.h"

/// Constants for room reverb types.
// Corresponds to room reverb setting set in Tomb Editor. To be used with @{Objects.Room.GetReverbType} and @{Objects.Room.SetReverbType} functions.
// @enum Objects.RoomReverb
// @pragma nostrip

static const std::unordered_map<std::string, ReverbType> ROOM_REVERB_TYPES
{
	/// Outdoor / open environment reverb.
	// @mem OUTSIDE
	{ "OUTSIDE", ReverbType::Outside },

	/// Small room reverb.
	// @mem SMALL
	{ "SMALL", ReverbType::Small },

	/// Medium room reverb.
	// @mem MEDIUM
	{ "MEDIUM", ReverbType::Medium },

	/// Large room reverb.
	// @mem LARGE
	{ "LARGE", ReverbType::Large },

	/// Pipe / tunnel reverb.
	// @mem PIPE
	{ "PIPE", ReverbType::Pipe }
};
