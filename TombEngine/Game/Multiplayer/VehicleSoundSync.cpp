#include "framework.h"
#include "Game/Multiplayer/VehicleSoundSync.h"

#include <algorithm>

#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_struct.h"
#include "Game/Multiplayer/Multiplayer.h"
#include "Sound/sound.h"
#include "Specific/level.h"

namespace TEN::Multiplayer
{
	constexpr auto PENDING_VEHICLE_SOUND_COUNT_MAX = 8;
	constexpr auto VEHICLE_SOUND_PITCH_MIN		   = 10;
	constexpr auto VEHICLE_SOUND_PITCH_MAX		   = 400;

	struct PendingVehicleSound
	{
		int SoundID		= 0;
		int Pitch		= 100;
		int Environment = 0;
		int IsLooped	= 0;
	};

	static PendingVehicleSound s_pendingVehicleSounds[PENDING_VEHICLE_SOUND_COUNT_MAX];
	static int				   s_pendingVehicleSoundCount = 0;

	void NotifyVehicleSound(int soundID, const Pose* pose, int soundEnv, float pitchMult)
	{
		if (pose == nullptr || !IsRemotePlayerEnabled())
			return;

		int vehicleItemNumber = Lara.Context.Vehicle;
		if (vehicleItemNumber < 0 || vehicleItemNumber >= (int)g_Level.Items.size())
			return;

		if (pose != &g_Level.Items[vehicleItemNumber].Pose)
			return;

		if (soundID < 0 || soundID >= (int)g_Level.SoundMap.size())
			return;

		int sampleIndex = g_Level.SoundMap[soundID];
		if (sampleIndex < 0)
			return;

		int pitch = std::clamp((int)(pitchMult * 100.0f), VEHICLE_SOUND_PITCH_MIN, VEHICLE_SOUND_PITCH_MAX);
		int isLooped = (((g_Level.SoundDetails[sampleIndex].Flags & 3) == (int)SoundPlayMode::Looped) ? 1 : 0);

		// One entry per sound ID and tick; a later call with a new pitch replaces the earlier one.
		for (int i = 0; i < s_pendingVehicleSoundCount; i++)
		{
			if (s_pendingVehicleSounds[i].SoundID == soundID)
			{
				s_pendingVehicleSounds[i].Pitch = pitch;
				return;
			}
		}

		if (s_pendingVehicleSoundCount >= PENDING_VEHICLE_SOUND_COUNT_MAX)
			return;

		auto& sound = s_pendingVehicleSounds[s_pendingVehicleSoundCount++];
		sound.SoundID = soundID;
		sound.Pitch = pitch;
		sound.Environment = soundEnv;
		sound.IsLooped = isLooped;
	}

	std::vector<int> PopPendingVehicleSounds()
	{
		auto sounds = std::vector<int>{};
		sounds.reserve(s_pendingVehicleSoundCount * 4);

		for (int i = 0; i < s_pendingVehicleSoundCount; i++)
		{
			sounds.push_back(s_pendingVehicleSounds[i].SoundID);
			sounds.push_back(s_pendingVehicleSounds[i].Pitch);
			sounds.push_back(s_pendingVehicleSounds[i].Environment);
			sounds.push_back(s_pendingVehicleSounds[i].IsLooped);
		}

		s_pendingVehicleSoundCount = 0;
		return sounds;
	}
}
