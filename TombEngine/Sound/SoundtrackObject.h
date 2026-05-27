#pragma once

#include <optional>
#include <string>
#include "Sound/sound.h"

namespace TEN::Sound
{
	enum class SoundtrackState
	{
		Stopped,
		Playing,
		Paused
	};

	struct SoundtrackOptions
	{
		float Volume = 1.0f;
		SoundTrackType PlayMode = SoundTrackType::OneShot;
		int FadeInTime = 0;
		int FadeOutTime = 0;
		int CrossfadeTime = 0;
		std::optional<bool> Loop = std::nullopt;
		std::optional<QWORD> StartPosition = std::nullopt;
		bool ShuffleStart = false;
		bool DampenAmbient = true;
		bool RestoreAmbient = true;
		bool PersistInSave = true;
		bool AutoPlay = false;
	};

	class SoundtrackObject
	{
	private:
		std::string _trackName = {};
		unsigned long long _runtimeID = 0;
		SoundtrackOptions _options = {};
		SoundtrackState _state = SoundtrackState::Stopped;
		HSTREAM _stream = 0;

		static unsigned long long _nextRuntimeID;

	public:
		// Construction

		SoundtrackObject() = default;
		SoundtrackObject(const std::string& trackName, const SoundtrackOptions& options);

		// Playback

		bool Play();
		void Pause();
		void Resume();
		void Stop(std::optional<int> fadeOutTime = std::nullopt);

		// Volume

		void SetVolume(float volume);
		float GetVolume() const;

		// Play mode

		void SetPlayMode(SoundTrackType mode);
		SoundTrackType GetPlayMode() const;

		// Position

		void SetPosition(float normalized);
		float GetPosition() const;

		void SetBytePosition(QWORD position);
		QWORD GetBytePosition() const;

		QWORD GetLength() const;

		// State queries

		bool IsPlaying() const;
		bool IsPaused() const;
		bool IsActive() const;
		std::string GetTrackName() const;
		unsigned long long GetRuntimeID() const;

		// Options access

		const SoundtrackOptions& GetOptions() const;

		// Persistence

		bool ShouldPersist() const;

	private:
		bool AllocateStream();
		void ReleaseStream();
		bool IsStreamValid() const;
		std::string ResolveTrackPath() const;
	};
}
