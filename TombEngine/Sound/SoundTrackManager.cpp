#include "framework.h"
#include "Sound/SoundTrackManager.h"

#include "Specific/configuration.h"
#include "Specific/trutils.h"

#include "Scripting/Include/ScriptInterfaceGame.h"
#include "Sound/sound.h"

using namespace TEN::Utils;

SoundTrackManager* g_SoundTrackManager = nullptr;

static const std::string TRACKS_EXTENSIONS[] = { ".wav", ".ogg", ".mp3" };

// -- SoundTrackManager private methods --------------------------------------

bool SoundTrackManager::IsBuiltinName(const std::string& name)
{
    return name == SOUND_TRACK_CHANNEL_BGM     ||
           name == SOUND_TRACK_CHANNEL_ONESHOT ||
           name == SOUND_TRACK_CHANNEL_VOICE;
}

TrackChannel* SoundTrackManager::FindChannel(const std::string& name)
{
    auto channelIt = _channels.find(name);
    if (channelIt == _channels.end())
        return nullptr;

    return &channelIt->second;
}

const TrackChannel* SoundTrackManager::FindChannel(const std::string& name) const
{
    auto channelIt = _channels.find(name);
    if (channelIt == _channels.end())
        return nullptr;

    return &channelIt->second;
}

std::string SoundTrackManager::ResolveTrackPath(const std::string& track) const
{
    auto fullPath = std::filesystem::path(_audioDirectory + track);

    if (!fullPath.has_extension() || !std::filesystem::is_regular_file(fullPath))
    {
        for (const auto& ext : TRACKS_EXTENSIONS)
        {
            fullPath.replace_extension(ext);
            if (std::filesystem::is_regular_file(fullPath))
                return fullPath.string();
        }

        return {};
    }

    return fullPath.string();
}

void SoundTrackManager::ConfigureFromPreset(TrackChannel& channel, TrackPreset preset)
{
    switch (preset)
    {
    case TrackPreset::BGM:
        channel.Flags         = TrackFlags::Loop | TrackFlags::Crossfade | TrackFlags::ShuffleStart;
        channel.FadeOutTime   = SOUND_XFADETIME_BGM;
        channel.CrossfadeTime = SOUND_XFADETIME_BGM;
        break;

    case TrackPreset::Loop:
        channel.Flags         = TrackFlags::Loop | TrackFlags::Crossfade;
        channel.FadeOutTime   = SOUND_XFADETIME_BGM;
        channel.CrossfadeTime = SOUND_XFADETIME_BGM;
        break;

    case TrackPreset::OneShot:
        channel.Flags       = TrackFlags::AutoFree | TrackFlags::DampBGM | TrackFlags::RestoreBGM;
        channel.FadeOutTime = SOUND_XFADETIME_ONESHOT;
        break;

    case TrackPreset::Voice:
        channel.Flags       = TrackFlags::AutoFree;
        channel.FadeOutTime = SOUND_XFADETIME_ONESHOT;
        break;
    }

    channel.Preset = preset;
}

void SoundTrackManager::AddBuiltin(const std::string& name, TrackPreset preset)
{
    auto channel = TrackChannel{};
    channel.Name = name;

    ConfigureFromPreset(channel, preset);

    _channels[name] = std::move(channel);
}

void SoundTrackManager::RestoreBGMVolume()
{
    for (auto& [name, ch] : _channels)
    {
        if (ch.Preset != TrackPreset::BGM || !BASS_ChannelIsActive(ch.Stream))
            continue;

        float masterVol = ((float)_globalVolume / 100.0f) * ch.Volume;
        BASS_ChannelSlideAttribute(ch.Stream, BASS_ATTRIB_VOL, masterVol, SOUND_XFADETIME_BGM_START);
    }
}

// NOTE: DWORD here is BASS's own cross-platform type (uint32_t on Linux/macOS, unsigned long on Windows).
void CALLBACK SoundTrackManager::OnTrackFinished(HSYNC, DWORD, DWORD, void*)
{
    if (g_SoundTrackManager)
        g_SoundTrackManager->RestoreBGMVolume();
}

// -- SoundTrackManager public methods ---------------------------------------

SoundTrackManager::SoundTrackManager(const std::string& audioDirectory)
{
    _audioDirectory = audioDirectory;

    AddBuiltin(SOUND_TRACK_CHANNEL_BGM,     TrackPreset::BGM);
    AddBuiltin(SOUND_TRACK_CHANNEL_ONESHOT, TrackPreset::OneShot);
    AddBuiltin(SOUND_TRACK_CHANNEL_VOICE,   TrackPreset::Voice);
}

bool SoundTrackManager::EnsureChannelExists(const std::string& name)
{
    if (name.empty())
    {
        TENLog("Cannot create AudioChannel with empty name.", LogLevel::Warning);
        return false;
    }

    if (_channels.count(name) > 0)
        return true;

    if ((int)_channels.size() >= SOUND_TRACK_CHANNEL_LIMIT)
    {
        TENLog("Sound channel limit of " + std::to_string(SOUND_TRACK_CHANNEL_LIMIT) + " reached. Channel \"" + name + "\" was not created.", LogLevel::Warning);
        return false;
    }

    auto channel = TrackChannel{};
    channel.Name = name;
    _channels[name] = std::move(channel);
    return true;
}

bool SoundTrackManager::Play(const std::string& channelName, std::optional<std::string> track,
                              std::optional<TrackPreset> preset, std::optional<QWORD> startPos,
                              int forceFadeIn, bool fireCallbacks)
{
    if (!g_Configuration.EnableSound)
        return false;

    if (!EnsureChannelExists(channelName))
        return false;

    auto* channel = FindChannel(channelName);
    if (!channel)
        return false;

    // Apply preset if provided.
    if (preset.has_value())
        ConfigureFromPreset(*channel, preset.value());

    // Resolve track: argument takes priority over stored default.
    auto trackName = track.has_value() ? track.value() : channel->Track;
    if (trackName.empty())
    {
        TENLog("AudioChannel '" + channelName + "' has no track to play.", LogLevel::Warning);
        return false;
    }

    bool channelActive = (BASS_ChannelIsActive(channel->Stream) != 0);

    // Same-track guard: if same track is already playing, optionally seek.
    if (channelActive && channel->Track.compare(trackName) == 0)
    {
        if (startPos.has_value() && BASS_ChannelGetLength(channel->Stream, BASS_POS_BYTE) > startPos.value())
            BASS_ChannelSetPosition(channel->Stream, startPos.value(), BASS_POS_BYTE);

        return true;
    }

    // Build BASS stream flags.
    unsigned int bassFlags = BASS_UNICODE | BASS_STREAM_AUTOFREE | BASS_SAMPLE_FLOAT | BASS_ASYNCFILE;
    if (HasTrackFlag(channel->Flags, TrackFlags::Loop))
        bassFlags |= BASS_SAMPLE_LOOP;

    // Resolve file path with extension fallback.
    auto fullPath = ResolveTrackPath(trackName);
    if (fullPath.empty())
    {
        TENLog("No soundtrack file found with name '" + trackName + "'.", LogLevel::Warning);
        return false;
    }

    // Determine fade-in duration. An explicit request (forceFadeIn) takes priority.
    int fadeInDuration = 0;
    if (forceFadeIn > 0)
    {
        fadeInDuration = forceFadeIn;
    }
    else if (HasTrackFlag(channel->Flags, TrackFlags::Crossfade))
    {
        fadeInDuration = channelActive ? channel->CrossfadeTime : SOUND_XFADETIME_BGM_START;
    }

    // Fade out the current stream if one is active, using the same duration as the fade-in
    // so both tracks crossfade over one another instead of the outgoing track cutting out early.
    if (channelActive)
    {
        int fadeOutDuration = (fadeInDuration > 0) ? fadeInDuration : channel->FadeOutTime;
        BASS_ChannelSlideAttribute(channel->Stream, BASS_ATTRIB_VOL, -1.0f, fadeOutDuration);
    }

    // Create new BASS stream.
    auto stream = BASS_StreamCreateFile(false, std::filesystem::path(fullPath).c_str(), 0, 0, bassFlags);
    if (Sound_CheckBASSError("Opening soundtrack '%s'", false, trackName.c_str()))
        return false;

    float masterVol = ((float)_globalVolume / 100.0f) * channel->Volume;

    // DampBGM: lower all BGM-type channel volumes while this channel is playing.
    if (HasTrackFlag(channel->Flags, TrackFlags::DampBGM))
    {
        for (auto& [name, ch] : _channels)
        {
            if (ch.Preset != TrackPreset::BGM || !BASS_ChannelIsActive(ch.Stream))
                continue;

            float dampedVol = ((float)_globalVolume / 100.0f) * ch.Volume * channel->DampVolume;
            BASS_ChannelSlideAttribute(ch.Stream, BASS_ATTRIB_VOL, dampedVol, SOUND_XFADETIME_BGM_START);
        }
    }

    // RestoreBGM: register a one-shot callback to restore BGM volume when this stream frees.
    if (HasTrackFlag(channel->Flags, TrackFlags::RestoreBGM))
        BASS_ChannelSetSync(stream, BASS_SYNC_FREE | BASS_SYNC_ONETIME | BASS_SYNC_MIXTIME, 0, OnTrackFinished, nullptr);

    // Set initial volume with optional fade-in.
    if (fadeInDuration > 0)
    {
        BASS_ChannelSetAttribute(stream, BASS_ATTRIB_VOL, 0.0f);
        BASS_ChannelSlideAttribute(stream, BASS_ATTRIB_VOL, masterVol, fadeInDuration);
    }
    else
    {
        BASS_ChannelSetAttribute(stream, BASS_ATTRIB_VOL, masterVol);
    }

    // ShuffleStart: randomize playhead for looped tracks when no explicit position is given.
    if (HasTrackFlag(channel->Flags, TrackFlags::ShuffleStart) && !startPos.has_value())
    {
        auto trackLen = BASS_ChannelGetLength(stream, BASS_POS_BYTE);
        auto newPos   = (QWORD)(trackLen * ((float)GetRandomControl() / (float)RAND_MAX));
        BASS_ChannelSetPosition(stream, newPos, BASS_POS_BYTE);
    }

    BASS_ChannelPlay(stream, false);

    // Apply explicit start position, which overrides the shuffle above.
    if (startPos.has_value() && BASS_ChannelGetLength(stream, BASS_POS_BYTE) > startPos.value())
        BASS_ChannelSetPosition(stream, startPos.value(), BASS_POS_BYTE);

    if (Sound_CheckBASSError("Playing soundtrack '%s'", true, trackName.c_str()))
        return false;

    channel->Stream = stream;
    channel->Track  = trackName;
    channel->Status = TrackStatus::Playing;

    // Load subtitles for the voice channel.
    if (channel->Name == SOUND_TRACK_CHANNEL_VOICE)
        LoadSubtitles(trackName);

    // Notify scripts that the channel started playing (fires PRE_AUDIO_CHANNEL once).
    if (fireCallbacks && g_GameScript)
        g_GameScript->OnAudioChannelStarted(channel->Name);

    return true;
}

void SoundTrackManager::SetTrack(const std::string& channelName, const std::string& track, int crossfadeTimeMs)
{
    auto* channel = FindChannel(channelName);
    if (!channel)
        return;

    bool channelActive = BASS_ChannelIsActive(channel->Stream) != 0;
    if (crossfadeTimeMs > 0)
        channel->CrossfadeTime = crossfadeTimeMs;

    if (channelActive)
    {
        // Channel is already playing: crossfade to the new track immediately.
        Play(channelName, track, std::nullopt, std::nullopt, crossfadeTimeMs);
    }
    else
    {
        // Channel is idle: store the track as the pending default without playing it.
        channel->Track = track;
    }
}

void SoundTrackManager::Stop(const std::string& channelName, std::optional<int> fadeOutTime)
{
    auto* channel = FindChannel(channelName);
    if (!channel || channel->Stream == 0)
        return;

    int fadeTime = fadeOutTime.has_value() ? fadeOutTime.value() : channel->FadeOutTime;
    BASS_ChannelSlideAttribute(channel->Stream, BASS_ATTRIB_VOL | BASS_SLIDE_LOG, -1.0f, fadeTime);

    channel->Track  = {};
    channel->Stream = 0;
    channel->Status = TrackStatus::Inactive;

    if (g_GameScript)
        g_GameScript->OnAudioChannelStopped(channelName);
}

void SoundTrackManager::StopAll(std::optional<int> fadeOutTime)
{
    // Collect names first: firing stop callbacks may run Lua that mutates the channel map.
    auto names = std::vector<std::string>{};
    for (const auto& [name, channel] : _channels)
        names.push_back(channel.Name);

    for (const auto& name : names)
        Stop(name, fadeOutTime);
}

void SoundTrackManager::Pause(const std::string& channelName)
{
    auto* channel = FindChannel(channelName);
    if (!channel || channel->Stream == 0)
        return;

    if (BASS_ChannelIsActive(channel->Stream) == BASS_ACTIVE_PLAYING)
    {
        BASS_ChannelPause(channel->Stream);
        channel->Status = TrackStatus::Paused;
    }
}

void SoundTrackManager::PauseAll(bool excludeVoice)
{
    for (auto& [name, channel] : _channels)
    {
        if (excludeVoice && channel.Name == SOUND_TRACK_CHANNEL_VOICE)
            continue;

        Pause(channel.Name);
    }
}

void SoundTrackManager::Resume(const std::string& channelName)
{
    auto* channel = FindChannel(channelName);
    if (!channel || channel->Stream == 0)
        return;

    if (BASS_ChannelIsActive(channel->Stream) == BASS_ACTIVE_PAUSED)
    {
        BASS_ChannelStart(channel->Stream);
        channel->Status = TrackStatus::Playing;
    }
}

void SoundTrackManager::ResumeAll()
{
    for (auto& [name, channel] : _channels)
        Resume(channel.Name);
}

void SoundTrackManager::SetGlobalVolume(int volume)
{
    _globalVolume = volume;

    for (auto& [name, channel] : _channels)
    {
        if (!BASS_ChannelIsActive(channel.Stream))
            continue;

        float vol = ((float)volume / 100.0f) * channel.Volume;
        BASS_ChannelSetAttribute(channel.Stream, BASS_ATTRIB_VOL, vol);
    }
}

int SoundTrackManager::GetGlobalVolume() const
{
    return _globalVolume;
}

void SoundTrackManager::SetChannelVolume(const std::string& channelName, float volume)
{
    auto* channel = FindChannel(channelName);
    if (!channel)
        return;

    channel->Volume = std::clamp(volume, 0.0f, 1.0f);

    if (BASS_ChannelIsActive(channel->Stream))
    {
        float vol = ((float)_globalVolume / 100.0f) * channel->Volume;
        BASS_ChannelSetAttribute(channel->Stream, BASS_ATTRIB_VOL, vol);
    }
}

float SoundTrackManager::GetChannelVolume(const std::string& channelName) const
{
    auto* channel = FindChannel(channelName);
    if (!channel)
        return 0.0f;

    return channel->Volume;
}

void SoundTrackManager::SetShuffleStart(const std::string& channelName, bool enable)
{
    auto* channel = FindChannel(channelName);
    if (!channel)
        return;

    if (enable)
        channel->Flags |= TrackFlags::ShuffleStart;
    else
        channel->Flags = channel->Flags & ~TrackFlags::ShuffleStart;
}

void SoundTrackManager::SetDampBGM(const std::string& channelName, float volume)
{
    auto* channel = FindChannel(channelName);
    if (!channel)
        return;

    channel->DampVolume = std::clamp(volume, 0.0f, 1.0f);

    // Damping (and the matching restore) is active whenever the target volume is below full.
    auto dampFlags = TrackFlags::DampBGM | TrackFlags::RestoreBGM;
    if (channel->DampVolume < 1.0f)
        channel->Flags = channel->Flags | dampFlags;
    else
        channel->Flags = channel->Flags & ~dampFlags;
}

float SoundTrackManager::GetDampBGM(const std::string& channelName) const
{
    auto* channel = FindChannel(channelName);
    if (!channel || !HasTrackFlag(channel->Flags, TrackFlags::DampBGM))
        return 0.0f;

    return channel->DampVolume;
}

void SoundTrackManager::SetChannelFlags(const std::string& channelName, TrackFlags flags)
{
    auto* channel = FindChannel(channelName);
    if (!channel)
        return;

    channel->Flags = flags;
}

TrackFlags SoundTrackManager::GetChannelFlags(const std::string& channelName) const
{
    auto* channel = FindChannel(channelName);
    if (!channel)
        return TrackFlags::None;

    return channel->Flags;
}

void SoundTrackManager::SetChannelPreset(const std::string& channelName, TrackPreset preset)
{
    auto* channel = FindChannel(channelName);
    if (!channel)
        return;

    ConfigureFromPreset(*channel, preset);
}

TrackPreset SoundTrackManager::GetChannelPreset(const std::string& channelName) const
{
    const auto* channel = FindChannel(channelName);
    if (!channel)
        return TrackPreset::OneShot;

    return channel->Preset;
}

void SoundTrackManager::SetCrossfadeTime(const std::string& channelName, int ms)
{
    auto* channel = FindChannel(channelName);
    if (!channel)
        return;

    channel->CrossfadeTime = ms;

    // A non-zero crossfade time implies the Crossfade flag; zero disables crossfading.
    if (ms > 0)
        channel->Flags |= TrackFlags::Crossfade;
    else
        channel->Flags = channel->Flags & ~TrackFlags::Crossfade;
}

bool SoundTrackManager::SetPositionSeconds(const std::string& channelName, double seconds)
{
    auto* channel = FindChannel(channelName);
    if (!channel || !BASS_ChannelIsActive(channel->Stream))
        return false;

    auto bytePos = BASS_ChannelSeconds2Bytes(channel->Stream, seconds);
    BASS_ChannelSetPosition(channel->Stream, bytePos, BASS_POS_BYTE);
    return true;
}

double SoundTrackManager::GetPositionSeconds(const std::string& channelName) const
{
    auto* channel = FindChannel(channelName);
    if (!channel || !BASS_ChannelIsActive(channel->Stream))
        return 0.0;

    auto bytePos = BASS_ChannelGetPosition(channel->Stream, BASS_POS_BYTE);
    return BASS_ChannelBytes2Seconds(channel->Stream, bytePos);
}

QWORD SoundTrackManager::GetPositionBytes(const std::string& channelName) const
{
    auto* channel = FindChannel(channelName);
    if (!channel || !BASS_ChannelIsActive(channel->Stream))
        return 0;

    return BASS_ChannelGetPosition(channel->Stream, BASS_POS_BYTE);
}

float SoundTrackManager::GetNormalizedPosition(const std::string& channelName) const
{
    auto* channel = FindChannel(channelName);
    if (!channel || !BASS_ChannelIsActive(channel->Stream))
        return 0.0f;

    auto totalBytes = BASS_ChannelGetLength(channel->Stream, BASS_POS_BYTE);
    if (totalBytes == 0)
        return 0.0f;

    auto curBytes = BASS_ChannelGetPosition(channel->Stream, BASS_POS_BYTE);
    return (float)curBytes / (float)totalBytes;
}

bool SoundTrackManager::IsPlaying(const std::string& channelName) const
{
    auto* channel = FindChannel(channelName);
    if (!channel)
        return false;

    return BASS_ChannelIsActive(channel->Stream) == BASS_ACTIVE_PLAYING;
}

bool SoundTrackManager::IsActive(const std::string& channelName) const
{
    auto* channel = FindChannel(channelName);
    if (!channel)
        return false;

    return BASS_ChannelIsActive(channel->Stream) != BASS_ACTIVE_STOPPED;
}

bool SoundTrackManager::IsPlayingTrack(const std::string& trackName) const
{
    auto lowerTrack = ToLower(trackName);
    for (const auto& [name, channel] : _channels)
    {
        if (!BASS_ChannelIsActive(channel.Stream))
            continue;

        if (ToLower(channel.Track) == lowerTrack)
            return true;
    }

    return false;
}

bool SoundTrackManager::IsPlayingTrack(const std::string& trackName, const std::string& channelName) const
{
    auto* channel = FindChannel(channelName);
    if (!channel || !BASS_ChannelIsActive(channel->Stream))
        return false;

    return ToLower(channel->Track) == ToLower(trackName);
}

std::string SoundTrackManager::GetTrackName(const std::string& channelName) const
{
    auto* channel = FindChannel(channelName);
    if (!channel)
        return {};

    return channel->Track;
}

float SoundTrackManager::GetLoudness(const std::string& channelName) const
{
    float result = 0.0f;

    if (!g_Configuration.EnableSound)
        return result;

    auto* channel = FindChannel(channelName);
    if (!channel || !BASS_ChannelIsActive(channel->Stream))
        return result;

    BASS_ChannelGetLevelEx(channel->Stream, &result, 0.1f, BASS_LEVEL_MONO | BASS_LEVEL_RMS);
    return std::clamp(result * 2.0f, 0.0f, 1.0f);
}

void SoundTrackManager::Clear(const std::string& channelName)
{
    if (IsBuiltinName(channelName))
    {
        TENLog("Cannot clear built-in audio channel '" + channelName + "'.", LogLevel::Warning);
        return;
    }

    auto* channel = FindChannel(channelName);
    if (!channel)
        return;

    bool wasActive = (channel->Stream != 0);
    if (wasActive)
        BASS_ChannelStop(channel->Stream);

    _channels.erase(channelName);

    if (wasActive && g_GameScript)
        g_GameScript->OnAudioChannelStopped(channelName);
}

void SoundTrackManager::ClearAll()
{
    for (auto& [name, channel] : _channels)
    {
        if (channel.Stream != 0)
            BASS_ChannelStop(channel.Stream);
    }

    _channels.clear();

    // Re-create built-in channels with default state.
    AddBuiltin(SOUND_TRACK_CHANNEL_BGM,     TrackPreset::BGM);
    AddBuiltin(SOUND_TRACK_CHANNEL_ONESHOT, TrackPreset::OneShot);
    AddBuiltin(SOUND_TRACK_CHANNEL_VOICE,   TrackPreset::Voice);
}

void SoundTrackManager::Update()
{
    // Collect names first: firing callbacks may run Lua that mutates the channel map.
    auto stopped = std::vector<std::string>{};

    for (auto& [name, channel] : _channels)
    {
        if (channel.Status == TrackStatus::Inactive || channel.Stream == 0)
            continue;

        auto bassStatus = BASS_ChannelIsActive(channel.Stream);
        if (bassStatus == BASS_ACTIVE_STOPPED || bassStatus == BASS_ACTIVE_STALLED)
        {
            channel.Status = TrackStatus::Inactive;
            channel.Stream = 0;
            stopped.push_back(channel.Name);
        }
    }

    if (g_GameScript)
    {
        for (const auto& name : stopped)
            g_GameScript->OnAudioChannelStopped(name);
    }
}

std::vector<HSTREAM> SoundTrackManager::GetActiveStreams() const
{
    auto streams = std::vector<HSTREAM>{};
    for (const auto& [name, channel] : _channels)
    {
        if (channel.Stream != 0)
            streams.push_back(channel.Stream);
    }

    return streams;
}

std::vector<TrackChannel> SoundTrackManager::GetAllChannelStates() const
{
    auto states = std::vector<TrackChannel>{};
    for (const auto& [name, channel] : _channels)
    {
        if (channel.Track.empty() || channel.Stream == 0)
            continue;

        // Skip channels that have stopped; only serialize channels still playing (including paused).
        auto bassStatus = BASS_ChannelIsActive(channel.Stream);
        if (bassStatus == BASS_ACTIVE_STOPPED)
            continue;

        auto state = channel;
        state.SavedPosition = BASS_ChannelGetPosition(channel.Stream, BASS_POS_BYTE);
        state.Stream        = 0;
        state.Status        = (bassStatus == BASS_ACTIVE_PAUSED) ? TrackStatus::Paused : TrackStatus::Playing;
        states.push_back(state);
    }

    return states;
}

void SoundTrackManager::RestoreFromSave(const std::vector<TrackChannel>& states)
{
    ClearAll();

    for (const auto& state : states)
    {
        if (!EnsureChannelExists(state.Name))
            continue;

        auto* channel = FindChannel(state.Name);
        if (!channel)
            continue;

        channel->Preset        = state.Preset;
        channel->Flags         = state.Flags;
        channel->FadeOutTime   = state.FadeOutTime;
        channel->CrossfadeTime = state.CrossfadeTime;
        channel->Volume        = state.Volume;
        channel->DampVolume    = state.DampVolume;

        if (!state.Track.empty())
        {
            auto startPos = (state.SavedPosition > 0)
                ? std::optional<QWORD>(state.SavedPosition)
                : std::nullopt;
            Play(state.Name, state.Track, std::nullopt, startPos, SOUND_XFADETIME_LEVELJUMP, false);

            if (state.Status == TrackStatus::Paused)
                Pause(state.Name);
        }
    }
}
