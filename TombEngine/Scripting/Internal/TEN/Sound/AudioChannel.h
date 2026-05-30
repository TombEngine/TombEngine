#pragma once

#include "Sound/SoundTrackManager.h"

namespace sol { class state; }

/***
Audio channel for named soundtrack control.

@tenclass Sound.AudioChannel
@pragma nostrip
*/

namespace TEN::Scripting::Sound
{
	class AudioChannel
	{
	private:
		std::string _channelName = {};

	public:
		static void Register(sol::state& state, sol::table& parent);

		static std::unique_ptr<AudioChannel> Create(
			const std::string& name,
			sol::optional<std::string> track,
			sol::optional<TrackPreset> preset);

		AudioChannel(const std::string& name);

		void  Play(sol::optional<std::string> track, sol::optional<TrackPreset> preset);
		void  SetTrack(const std::string& track, sol::optional<int> crossfadeTime);
		void  Stop(sol::optional<int> fadeOutTime);
		void  Pause();
		void  Resume();
		void  Clear();
		void  SetVolume(float volume);
		float GetVolume() const;
		void  SetShuffleStart(bool enable);
		void  SetPosition(const TEN::Scripting::Time& time);
		TEN::Scripting::Time GetPosition() const;
		float GetNormalizedPosition() const;
		bool  IsPlaying() const;
		std::string GetName() const;
		std::string GetTrack() const;
		float GetLoudness() const;
		void  SetFlags(int flags);
		int   GetFlags() const;
	};
}
