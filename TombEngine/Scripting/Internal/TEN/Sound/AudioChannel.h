#pragma once

#include "Scripting/Internal/TEN/Types/Time/Time.h"
#include "Sound/sound.h"
#include "Sound/SoundTrackManager.h"

namespace sol { class state; }

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
			sol::optional<SoundTrackType> type);

		AudioChannel(const std::string& name);

		void  Play(sol::optional<std::string> track, sol::optional<SoundTrackType> type);
		void  SetTrack(const std::string& track, sol::optional<TEN::Scripting::Time> crossfadeTime);
		void  Stop(sol::optional<TEN::Scripting::Time> fadeOutTime);
		void  Pause();
		void  Resume();
		void  Clear();
		void  SetVolume(float volume);
		float GetVolume() const;
		void  SetShuffleStart(bool enable);
		void  SetDampBGM(bool enable);
		bool  GetDampBGM() const;
		void  SetPosition(const TEN::Scripting::Time& time);
		TEN::Scripting::Time GetPosition() const;
		float GetNormalizedPosition() const;
		bool  IsPlaying() const;
		std::string GetName() const;
		std::string GetTrack() const;
		float GetLoudness() const;
		SoundTrackType GetType() const;
		void  SetType(SoundTrackType type);
		void  SetCrossFadeLength(const TEN::Scripting::Time& time);
	};
}
