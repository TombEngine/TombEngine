#pragma once
#include <string>
#include <unordered_map>

#include "Sound/sound.h"

/***
Constants for room reverb types.
@enum Objects.RoomReverb
@pragma nostrip
*/

/*** Table of Objects.RoomReverb constants.
<br>
Corresponds to room reverb setting set in Tomb Editor. To be used with @{Objects.Room.GetReverbType} and @{Objects.Room.SetReverbType} functions.
The following constants are inside RoomReverb.

 - `OUTSIDE`
 - `SMALL`
 - `MEDIUM`
 - `LARGE`
 - `PIPE`

@table Objects.RoomReverb
*/

static const std::unordered_map<std::string, ReverbType> ROOM_REVERB_TYPES
{
	{ "OUTSIDE", ReverbType::Outside },
	{ "SMALL", ReverbType::Small },
	{ "MEDIUM", ReverbType::Medium },
	{ "LARGE", ReverbType::Large },
	{ "PIPE", ReverbType::Pipe }
};
