#include "framework.h"
#include "Sound/SoundtrackObject.h"

#include "Specific/configuration.h"
#include "Sound/sound.h"
#include "Sound/SoundtrackRegistry.h"

using namespace TEN::Sound;

unsigned long long SoundtrackObject::_nextRuntimeID = 1;

SoundtrackObject::SoundtrackObject(const std::string& trackName, const SoundtrackOptions& options)
{
	_trackName = trackName;
	_options = options;
	_runtimeID = _nextRuntimeID++;

	// Clamp volume.
	_options.Volume = std::clamp(_options.Volume, 0.0f, 1.0f);

	// Clamp fade times to non-negative.
	_options.FadeInTime = std::max(0, _options.FadeInTime);
	_options.FadeOutTime = std::max(0, _options.FadeOutTime);
	_options.CrossfadeTime = std::max(0, _options.CrossfadeTime);

	if (_options.AutoPlay)
		Play();
}

bool SoundtrackObject::Play()
{
	if (!g_Configuration.EnableSound)
		return false;

	if (_trackName.empty())
		return false;

	// If paused, resume instead.
	if (_state == SoundtrackState::Paused && IsStreamValid())
	{
		Resume();
		return true;
	}

	// If already playing (one-shot retrigger), restart from beginning.
	if (_state == SoundtrackState::Playing && IsStreamValid())
	{
		BASS_ChannelSetPosition(_stream, 0, BASS_POS_BYTE);
		return true;
	}

	// Allocate stream if needed.
	if (!AllocateStream())
		return false;

	// Set volume.
	BASS_ChannelSetAttribute(_stream, BASS_ATTRIB_VOL, _options.Volume);

	// Handle fade-in.
	if (_options.FadeInTime > 0)
	{
		BASS_ChannelSetAttribute(_stream, BASS_ATTRIB_VOL, 0.0f);
		BASS_ChannelSlideAttribute(_stream, BASS_ATTRIB_VOL, _options.Volume, _options.FadeInTime);
	}

	// Handle start position.
	if (_options.StartPosition.has_value())
	{
		QWORD length = BASS_ChannelGetLength(_stream, BASS_POS_BYTE);
		if (length > _options.StartPosition.value())
			BASS_ChannelSetPosition(_stream, _options.StartPosition.value(), BASS_POS_BYTE);
	}
	else if (_options.ShuffleStart)
	{
		QWORD length = BASS_ChannelGetLength(_stream, BASS_POS_BYTE);
		QWORD newPos = (QWORD)(length * ((float)rand() / (float)RAND_MAX));
		BASS_ChannelSetPosition(_stream, newPos, BASS_POS_BYTE);
	}

	// Start playback.
	BASS_ChannelPlay(_stream, false);

	_state = SoundtrackState::Playing;
	return true;
}

void SoundtrackObject::Pause()
{
	if (_state != SoundtrackState::Playing || !IsStreamValid())
		return;

	BASS_ChannelPause(_stream);
	_state = SoundtrackState::Paused;
}

void SoundtrackObject::Resume()
{
	if (_state != SoundtrackState::Paused || !IsStreamValid())
		return;

	BASS_ChannelPlay(_stream, false);
	_state = SoundtrackState::Playing;
}

void SoundtrackObject::Stop(std::optional<int> fadeOutTime)
{
	if (!IsStreamValid())
	{
		_state = SoundtrackState::Stopped;
		return;
	}

	int fadeTime = fadeOutTime.value_or(_options.FadeOutTime);

	if (fadeTime > 0)
	{
		BASS_ChannelSlideAttribute(_stream, BASS_ATTRIB_VOL | BASS_SLIDE_LOG, -1.0f, fadeTime);
	}
	else
	{
		BASS_ChannelStop(_stream);
		BASS_StreamFree(_stream);
		_stream = 0;
	}

	_state = SoundtrackState::Stopped;
}

void SoundtrackObject::SetVolume(float volume)
{
	_options.Volume = std::clamp(volume, 0.0f, 1.0f);

	if (IsStreamValid())
		BASS_ChannelSetAttribute(_stream, BASS_ATTRIB_VOL, _options.Volume);
}

float SoundtrackObject::GetVolume() const
{
	return _options.Volume;
}

void SoundtrackObject::SetPlayMode(SoundTrackType mode)
{
	_options.PlayMode = mode;
}

SoundTrackType SoundtrackObject::GetPlayMode() const
{
	return _options.PlayMode;
}

void SoundtrackObject::SetPosition(float normalized)
{
	normalized = std::clamp(normalized, 0.0f, 1.0f);

	if (!IsStreamValid())
		return;

	QWORD length = BASS_ChannelGetLength(_stream, BASS_POS_BYTE);
	QWORD newPos = (QWORD)(length * normalized);
	BASS_ChannelSetPosition(_stream, newPos, BASS_POS_BYTE);
}

float SoundtrackObject::GetPosition() const
{
	if (!IsStreamValid())
		return 0.0f;

	QWORD length = BASS_ChannelGetLength(_stream, BASS_POS_BYTE);
	if (length == 0)
		return 0.0f;

	QWORD pos = BASS_ChannelGetPosition(_stream, BASS_POS_BYTE);
	return (float)pos / (float)length;
}

void SoundtrackObject::SetBytePosition(QWORD position)
{
	if (!IsStreamValid())
		return;

	QWORD length = BASS_ChannelGetLength(_stream, BASS_POS_BYTE);
	if (position < length)
		BASS_ChannelSetPosition(_stream, position, BASS_POS_BYTE);
}

QWORD SoundtrackObject::GetBytePosition() const
{
	if (!IsStreamValid())
		return 0;

	return BASS_ChannelGetPosition(_stream, BASS_POS_BYTE);
}

QWORD SoundtrackObject::GetLength() const
{
	if (!IsStreamValid())
		return 0;

	return BASS_ChannelGetLength(_stream, BASS_POS_BYTE);
}

bool SoundtrackObject::IsPlaying() const
{
	if (_state != SoundtrackState::Playing)
		return false;

	// Also verify with BASS that the channel is truly active.
	if (IsStreamValid())
		return BASS_ChannelIsActive(_stream) == BASS_ACTIVE_PLAYING;

	return false;
}

bool SoundtrackObject::IsPaused() const
{
	return _state == SoundtrackState::Paused;
}

bool SoundtrackObject::IsActive() const
{
	return _state == SoundtrackState::Playing || _state == SoundtrackState::Paused;
}

std::string SoundtrackObject::GetTrackName() const
{
	return _trackName;
}

unsigned long long SoundtrackObject::GetRuntimeID() const
{
	return _runtimeID;
}

const SoundtrackOptions& SoundtrackObject::GetOptions() const
{
	return _options;
}

bool SoundtrackObject::ShouldPersist() const
{
	return _options.PersistInSave && IsActive();
}

bool SoundtrackObject::AllocateStream()
{
	auto trackPath = ResolveTrackPath();
	if (trackPath.empty())
		return false;

	unsigned int flags = BASS_UNICODE | BASS_SAMPLE_FLOAT | BASS_ASYNCFILE;

	// Determine looping.
	bool shouldLoop = false;
	if (_options.Loop.has_value())
	{
		shouldLoop = _options.Loop.value();
	}
	else
	{
		// Default: BGM loops, others don't.
		shouldLoop = (_options.PlayMode == SoundTrackType::BGM);
	}

	if (shouldLoop)
		flags |= BASS_SAMPLE_LOOP;

	auto fullPath = std::filesystem::path(trackPath);
	_stream = BASS_StreamCreateFile(false, fullPath.c_str(), 0, 0, flags);

	if (_stream == 0)
		return false;

	return true;
}

void SoundtrackObject::ReleaseStream()
{
	if (_stream != 0)
	{
		BASS_StreamFree(_stream);
		_stream = 0;
	}
}

bool SoundtrackObject::IsStreamValid() const
{
	return _stream != 0 && BASS_ChannelIsActive(_stream) != BASS_ACTIVE_STOPPED;
}

std::string SoundtrackObject::ResolveTrackPath() const
{
	auto audioDir = TEN::Sound::GetFullAudioDirectory();
	auto fullTrackName = std::filesystem::path(audioDir + _trackName);

	if (fullTrackName.has_extension() && std::filesystem::is_regular_file(fullTrackName))
		return fullTrackName.string();

	// Try known extensions.
	const std::string extensions[] = { ".wav", ".ogg", ".mp3" };
	for (auto& ext : extensions)
	{
		fullTrackName.replace_extension(ext);
		if (std::filesystem::is_regular_file(fullTrackName))
			return fullTrackName.string();
	}

	return {};
}
