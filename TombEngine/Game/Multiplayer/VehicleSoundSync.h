#pragma once

#include <vector>

class Pose;

namespace TEN::Multiplayer
{
	// Called at the top of SoundEffect(). Queues the sound when it is played at the pose of the vehicle that the local player drives.
	// soundEnv is a SoundEnvironment value as an int. pitchMult is the pitch multiplier that vehicles use for the engine revs.
	void NotifyVehicleSound(int soundID, const Pose* pose, int soundEnv, float pitchMult);

	// Returns the vehicle sounds queued since the last call, then clears the queue.
	// Four ints per sound: sound ID, pitch multiplier * 100, SoundEnvironment value, 1 if the sample is looped.
	std::vector<int> PopPendingVehicleSounds();
}
